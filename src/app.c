#include "app.h"
#include "font.h"
#include "io.h"
#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HISTORY_LIMIT ((size_t)256 << 20)
#define MESSAGE_MS 3500
#define CONFIRM_MS 3000

typedef struct {
    char path[PATH_CAP];
    IoImage image;
    unsigned state;
    unsigned document;
    bool ok;
} SaveJob;

static Uint32 save_event_type;

static const uint32_t default_palette[PALETTE_COUNT] = {
    0xFF000000, 0xFF7F7F7F, 0xFF880015, 0xFFED1C24, 0xFFFF7F27,
    0xFFFFF200, 0xFF22B14C, 0xFF00A2E8, 0xFF3F48CC, 0xFFA349A4,
    0xFFFFFFFF, 0xFFC3C3C3, 0xFFB97A57, 0xFFFFAEC9, 0xFFFFC90E,
    0xFFEFE4B0, 0xFFB5E61D, 0xFF99D9EA, 0xFF7092BE, 0xFFC8BFE7,
};

static const char *tool_names[TOOL_COUNT] = {
    "Brush", "Eraser", "Line", "Rectangle", "Ellipse", "Fill", "Picker",
};

const char *app_tool_name(Tool t)
{
    return tool_names[t];
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static bool make_texture(App *a)
{
    SDL_Texture *t = SDL_CreateTexture(a->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                       a->canvas.w, a->canvas.h);
    if (!t) return false;
    if (a->texture) SDL_DestroyTexture(a->texture);
    a->texture = t;
    a->canvas.dirty = box_empty();
    canvas_mark(&a->canvas, 0, 0, a->canvas.w - 1, a->canvas.h - 1);
    return true;
}

static void pick_window_size(int cw, int ch, int *w, int *h)
{
    SDL_Rect usable = { 0, 0, 1280, 800 };
    SDL_GetDisplayUsableBounds(0, &usable);
    int want_w = cw + 64, want_h = ch + UI_TOOLBAR_H + UI_STATUS_H + 64;
    int max_w = usable.w * 9 / 10, max_h = usable.h * 9 / 10;
    *w = want_w < UI_MIN_W ? UI_MIN_W : want_w;
    *h = want_h < UI_MIN_H ? UI_MIN_H : want_h;
    if (*w > max_w && max_w >= UI_MIN_W) *w = max_w;
    if (*h > max_h && max_h >= UI_MIN_H) *h = max_h;
}

bool app_init(App *a, int cw, int ch, const char *path)
{
    a->tool = TOOL_BRUSH;
    a->return_tool = TOOL_BRUSH;
    a->size = 8;
    a->primary = 0xFF000000;
    a->secondary = 0xFFFFFFFF;
    memcpy(a->palette, default_palette, sizeof a->palette);
    a->running = true;
    a->redraw = true;

    a->save_event = SDL_RegisterEvents(1);
    if (a->save_event == (Uint32)-1) return false;
    save_event_type = a->save_event;

    uint32_t *loaded = NULL;
    if (path) {
        snprintf(a->path, sizeof a->path, "%s", path);
        if (file_exists(path)) {
            const char *err = "unknown error";
            loaded = io_load(path, &cw, &ch, &err);
            if (!loaded) {
                fprintf(stderr, "spint: cannot open %s: %s\n", path, err);
                return false;
            }
        }
    }

    int w, h;
    pick_window_size(cw, ch, &w, &h);
    a->window = SDL_CreateWindow("Spint", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!a->window) {
        fprintf(stderr, "spint: cannot create window: %s\n", SDL_GetError());
        free(loaded);
        return false;
    }
    SDL_SetWindowMinimumSize(a->window, UI_MIN_W, UI_MIN_H);

    a->renderer = SDL_CreateRenderer(a->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!a->renderer) a->renderer = SDL_CreateRenderer(a->window, -1, SDL_RENDERER_SOFTWARE);
    if (!a->renderer) {
        fprintf(stderr, "spint: cannot create renderer: %s\n", SDL_GetError());
        free(loaded);
        return false;
    }
    SDL_RendererInfo info;
    a->max_texture = CANVAS_MAX_SIDE;
    if (SDL_GetRendererInfo(a->renderer, &info) == 0) {
        int m = info.max_texture_width < info.max_texture_height ? info.max_texture_width : info.max_texture_height;
        if (m > 0 && m < a->max_texture) a->max_texture = m;
    }
    if (cw > a->max_texture || ch > a->max_texture) {
        fprintf(stderr, "spint: canvas %dx%d is larger than this GPU allows (%d)\n", cw, ch, a->max_texture);
        free(loaded);
        return false;
    }

    if (loaded) {
        canvas_adopt(&a->canvas, loaded, cw, ch);
    } else if (!canvas_init(&a->canvas, cw, ch, 0xFFFFFFFF)) {
        fprintf(stderr, "spint: cannot allocate a %dx%d canvas\n", cw, ch);
        return false;
    }
    if (!history_init(&a->history, &a->canvas, HISTORY_LIMIT) || !make_texture(a) || !font_init(a->renderer) ||
        !ui_init(a->renderer)) {
        fprintf(stderr, "spint: initialization failed: %s\n", SDL_GetError());
        return false;
    }

    a->cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    a->cursor_cross = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    a->cursor_move = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    a->cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

    SDL_GetWindowSize(a->window, &a->win_w, &a->win_h);
    app_layout(a);
    view_set_canvas(&a->view, a->canvas.w, a->canvas.h);
    a->saved_state = history_state(&a->history);
    app_update_title(a);
    if (path && !loaded) app_message(a, "New file %s", base_name(path));
    return true;
}

static void finish_save(App *a, SaveJob *job)
{
    SDL_WaitThread(a->save_thread, NULL);
    a->save_thread = NULL;
    a->saving = false;
    if (job->ok) {
        if (job->document == a->document) {
            snprintf(a->path, sizeof a->path, "%s", job->path);
            a->saved_state = job->state;
        }
        app_message(a, "Saved %s", base_name(job->path));
    } else {
        app_message(a, "Could not save %s", base_name(job->path));
    }
    io_release(&job->image);
    free(job);
    app_update_title(a);
}

void app_quit(App *a)
{
    if (a->save_thread) {
        SDL_Event e;
        SDL_WaitThread(a->save_thread, NULL);
        a->save_thread = NULL;
        while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, a->save_event, a->save_event) == 1) {
            SaveJob *job = e.user.data1;
            io_release(&job->image);
            free(job);
        }
    }
    history_free(&a->history);
    canvas_free(&a->canvas);
    font_free();
    ui_free();
    SDL_Cursor *cursors[] = { a->cursor_arrow, a->cursor_cross, a->cursor_move, a->cursor_hand };
    for (size_t i = 0; i < sizeof cursors / sizeof *cursors; i++)
        if (cursors[i]) SDL_FreeCursor(cursors[i]);
    if (a->texture) SDL_DestroyTexture(a->texture);
    if (a->renderer) SDL_DestroyRenderer(a->renderer);
    if (a->window) SDL_DestroyWindow(a->window);
}

void app_layout(App *a)
{
    SDL_Rect area = { 0, UI_TOOLBAR_H, a->win_w, a->win_h - UI_TOOLBAR_H - UI_STATUS_H };
    if (area.h < 1) area.h = 1;
    view_set_area(&a->view, area);
    a->redraw = true;
}

bool app_modified(const App *a)
{
    return history_state(&a->history) != a->saved_state;
}

void app_update_title(App *a)
{
    char title[PATH_CAP + 32];
    snprintf(title, sizeof title, "Spint - %s%s", a->path[0] ? base_name(a->path) : "untitled",
             app_modified(a) ? " *" : "");
    SDL_SetWindowTitle(a->window, title);
    a->redraw = true;
}

void app_message(App *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->message, sizeof a->message, fmt, ap);
    va_end(ap);
    a->message_until = SDL_GetTicks() + MESSAGE_MS;
    a->redraw = true;
}

void app_set_tool(App *a, Tool t)
{
    a->tool = t;
    if (t != TOOL_PICKER) a->return_tool = t;
    a->redraw = true;
}

void app_set_size(App *a, int size)
{
    if (size < BRUSH_MIN) size = BRUSH_MIN;
    if (size > BRUSH_MAX) size = BRUSH_MAX;
    a->size = size;
    a->redraw = true;
}

bool app_open(App *a, const char *path)
{
    int w, h;
    const char *err = "unknown error";
    uint32_t *px = io_load(path, &w, &h, &err);
    if (!px) {
        app_message(a, "Cannot open %s: %s", base_name(path), err);
        return false;
    }
    if (w > a->max_texture || h > a->max_texture) {
        free(px);
        app_message(a, "Image is larger than %d pixels", a->max_texture);
        return false;
    }
    history_cancel(&a->history);
    a->drag = DRAG_NONE;
    a->has_last = false;
    canvas_adopt(&a->canvas, px, w, h);
    a->document++;
    if (!history_reset(&a->history) || !make_texture(a)) {
        fprintf(stderr, "spint: out of memory\n");
        exit(1);
    }
    view_set_canvas(&a->view, w, h);
    snprintf(a->path, sizeof a->path, "%s", path);
    a->saved_state = history_state(&a->history);
    app_update_title(a);
    app_message(a, "Opened %s (%dx%d)", base_name(path), w, h);
    return true;
}

static int save_worker(void *data)
{
    SaveJob *job = data;
    job->ok = io_write(job->path, &job->image);
    return 0;
}

static int save_thread_main(void *data)
{
    save_worker(data);
    SDL_Event e;
    SDL_zero(e);
    e.type = save_event_type;
    e.user.data1 = data;
    SDL_PushEvent(&e);
    return 0;
}

static void timestamp_name(char *out, size_t cap, const char *dir_of)
{
    char stamp[32];
    time_t t = time(NULL);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&t));
    const char *slash = dir_of ? strrchr(dir_of, '/') : NULL;
    int dir_len = slash ? (int)(slash - dir_of + 1) : 0;
    snprintf(out, cap, "%.*sspint-%s.png", dir_len, dir_of ? dir_of : "", stamp);
    for (int i = 2; file_exists(out) && i < 1000; i++)
        snprintf(out, cap, "%.*sspint-%s-%d.png", dir_len, dir_of ? dir_of : "", stamp, i);
}

void app_save(App *a, bool new_name)
{
    if (a->saving) {
        app_message(a, "Still saving, please wait");
        return;
    }
    if (a->drag == DRAG_PAINT) return;
    SaveJob *job = calloc(1, sizeof *job);
    if (!job) return;
    if (new_name || !a->path[0]) {
        timestamp_name(job->path, sizeof job->path, a->path[0] ? a->path : NULL);
    } else if (io_supported(a->path)) {
        snprintf(job->path, sizeof job->path, "%s", a->path);
    } else {
        snprintf(job->path, sizeof job->path, "%.*s.png", PATH_CAP - 5, a->path);
    }
    if (!io_pack(&job->image, a->canvas.px, a->canvas.w, a->canvas.h)) {
        free(job);
        app_message(a, "Out of memory while saving");
        return;
    }
    job->state = history_state(&a->history);
    job->document = a->document;
    a->saving = true;
    a->save_thread = SDL_CreateThread(save_thread_main, "spint-save", job);
    if (!a->save_thread) {
        save_worker(job);
        SDL_Event e;
        SDL_zero(e);
        e.type = a->save_event;
        e.user.data1 = job;
        app_save_done(a, &e);
        return;
    }
    app_message(a, "Saving %s", base_name(job->path));
}

void app_save_done(App *a, SDL_Event *e)
{
    finish_save(a, e->user.data1);
}

void app_clear(App *a)
{
    if (a->drag == DRAG_PAINT) return;
    history_begin(&a->history);
    canvas_fill(&a->canvas, 0xFFFFFFFF);
    history_commit(&a->history);
    app_update_title(a);
}

void app_undo(App *a)
{
    if (a->drag == DRAG_PAINT) return;
    if (!history_undo(&a->history)) app_message(a, "Nothing to undo");
    app_update_title(a);
}

void app_redo(App *a)
{
    if (a->drag == DRAG_PAINT) return;
    if (!history_redo(&a->history)) app_message(a, "Nothing to redo");
    app_update_title(a);
}

void app_request_quit(App *a)
{
    Uint32 now = SDL_GetTicks();
    bool armed = a->quit_armed_until && !SDL_TICKS_PASSED(now, a->quit_armed_until);
    if (!app_modified(a) || armed) {
        a->running = false;
        return;
    }
    a->quit_armed_until = now + CONFIRM_MS;
    app_message(a, "Unsaved changes. Quit again to discard, or Ctrl+S to save");
}
