#include "input.h"
#include "draw.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define ZOOM_STEP 1.25f
#define WHEEL_ZOOM 1.15f
#define DROP_CONFIRM_MS 4000

static bool ctrl_down(Uint16 mod)
{
    return (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
}

static void screen_to_canvas(const App *a, int sx, int sy, float *cx, float *cy)
{
    view_to_canvas(&a->view, sx + 0.5f, sy + 0.5f, cx, cy);
}

static void update_cursor(App *a)
{
    SDL_Cursor *want = a->cursor_arrow;
    if (a->drag == DRAG_PAN) want = a->cursor_move;
    else if (a->drag == DRAG_PAINT) want = a->cursor_cross;
    else if (a->mouse_inside && !a->show_help) want = a->space_held ? a->cursor_move : a->cursor_cross;
    else if (a->mouse_y < UI_TOOLBAR_H) {
        Hit h = ui_hit(a->win_w, a->win_h, a->mouse_x, a->mouse_y);
        if (h.kind != HIT_BAR && h.kind != HIT_NONE) want = a->cursor_hand;
    }
    if (want && want != a->cursor_current) {
        SDL_SetCursor(want);
        a->cursor_current = want;
    }
}

static void start_drag(App *a, Drag d)
{
    a->drag = d;
    SDL_CaptureMouse(SDL_TRUE);
}

static void stop_drag(App *a)
{
    a->drag = DRAG_NONE;
    SDL_CaptureMouse(SDL_FALSE);
    a->redraw = true;
}

static void constrain(const App *a, float *x, float *y)
{
    float dx = *x - a->anchor_x, dy = *y - a->anchor_y;
    if (a->tool == TOOL_LINE) {
        float len = sqrtf(dx * dx + dy * dy);
        float angle = atan2f(dy, dx);
        float step = 3.14159265f / 4.0f;
        angle = roundf(angle / step) * step;
        *x = a->anchor_x + cosf(angle) * len;
        *y = a->anchor_y + sinf(angle) * len;
    } else {
        float d = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
        *x = a->anchor_x + (dx < 0 ? -d : d);
        *y = a->anchor_y + (dy < 0 ? -d : d);
    }
}

static void draw_shape(App *a)
{
    float x = a->cur_x, y = a->cur_y;
    if (a->shift_held) constrain(a, &x, &y);
    Canvas *c = &a->canvas;
    if (a->tool == TOOL_LINE) {
        draw_capsule(c, a->anchor_x, a->anchor_y, x, y, a->size, a->paint_color);
        return;
    }
    int x0 = (int)floorf(a->anchor_x), y0 = (int)floorf(a->anchor_y);
    int x1 = (int)floorf(x), y1 = (int)floorf(y);
    if (a->tool == TOOL_RECT) draw_rect(c, x0, y0, x1, y1, a->size, a->filled, a->paint_color);
    else draw_ellipse(c, x0, y0, x1, y1, a->size, a->filled, a->paint_color);
}

static bool is_shape(Tool t)
{
    return t == TOOL_LINE || t == TOOL_RECT || t == TOOL_ELLIPSE;
}

static void pick_color(App *a, float cx, float cy, bool primary)
{
    int x = (int)floorf(cx), y = (int)floorf(cy);
    if (!canvas_contains(&a->canvas, x, y)) return;
    uint32_t c = canvas_get(&a->canvas, x, y);
    if (primary) a->primary = c;
    else a->secondary = c;
    a->redraw = true;
}

static void begin_paint(App *a, Uint8 button, int sx, int sy)
{
    float cx, cy;
    screen_to_canvas(a, sx, sy, &cx, &cy);
    bool primary = button == SDL_BUTTON_LEFT;
    if (a->tool == TOOL_PICKER || (SDL_GetModState() & KMOD_ALT)) {
        pick_color(a, cx, cy, primary);
        if (a->tool == TOOL_PICKER) app_set_tool(a, a->return_tool);
        return;
    }
    a->paint_color = a->tool == TOOL_ERASER ? a->secondary : primary ? a->primary : a->secondary;
    if (a->tool == TOOL_FILL) {
        history_begin(&a->history);
        draw_flood(&a->canvas, (int)floorf(cx), (int)floorf(cy), a->paint_color);
        history_commit(&a->history);
        app_update_title(a);
        return;
    }
    history_begin(&a->history);
    a->drag_button = button;
    start_drag(a, DRAG_PAINT);
    a->anchor_x = a->cur_x = cx;
    a->anchor_y = a->cur_y = cy;
    if (is_shape(a->tool)) {
        draw_shape(a);
    } else {
        if (a->shift_held && a->has_last) draw_capsule(&a->canvas, a->last_x, a->last_y, cx, cy, a->size, a->paint_color);
        else draw_capsule(&a->canvas, cx, cy, cx, cy, a->size, a->paint_color);
        a->last_x = cx;
        a->last_y = cy;
    }
    a->redraw = true;
}

static void move_paint(App *a, int sx, int sy)
{
    float cx, cy;
    screen_to_canvas(a, sx, sy, &cx, &cy);
    a->cur_x = cx;
    a->cur_y = cy;
    if (is_shape(a->tool)) {
        history_revert(&a->history);
        draw_shape(a);
    } else {
        draw_capsule(&a->canvas, a->last_x, a->last_y, cx, cy, a->size, a->paint_color);
        a->last_x = cx;
        a->last_y = cy;
    }
    a->redraw = true;
}

static void end_paint(App *a)
{
    history_commit(&a->history);
    a->has_last = !is_shape(a->tool);
    stop_drag(a);
    app_update_title(a);
}

static void cancel_paint(App *a)
{
    history_cancel(&a->history);
    stop_drag(a);
}

static void zoom_center(App *a, float factor)
{
    const SDL_Rect *r = &a->view.area;
    view_zoom_at(&a->view, factor, r->x + r->w * 0.5f, r->y + r->h * 0.5f);
    a->redraw = true;
}

static int size_step(int size)
{
    return size < 10 ? 1 : size < 40 ? 2 : size < 100 ? 5 : 10;
}

static void swap_colors(App *a)
{
    uint32_t t = a->primary;
    a->primary = a->secondary;
    a->secondary = t;
    a->redraw = true;
}

static void on_mouse_down(App *a, SDL_MouseButtonEvent *b)
{
    a->mouse_x = b->x;
    a->mouse_y = b->y;
    if (a->drag != DRAG_NONE) return;
    if (a->show_help) {
        a->show_help = false;
        a->redraw = true;
        return;
    }
    Hit h = ui_hit(a->win_w, a->win_h, b->x, b->y);
    if (h.kind == HIT_CANVAS) {
        if (b->button == SDL_BUTTON_MIDDLE || (b->button == SDL_BUTTON_LEFT && a->space_held)) {
            a->pan_x = (float)b->x;
            a->pan_y = (float)b->y;
            start_drag(a, DRAG_PAN);
            a->redraw = true;
        } else if (b->button == SDL_BUTTON_LEFT || b->button == SDL_BUTTON_RIGHT) {
            begin_paint(a, b->button, b->x, b->y);
        }
        return;
    }
    bool left = b->button == SDL_BUTTON_LEFT;
    switch (h.kind) {
    case HIT_TOOL:
        if (left) app_set_tool(a, (Tool)h.index);
        break;
    case HIT_FILLED:
        if (left) a->filled = !a->filled;
        break;
    case HIT_PRIMARY:
    case HIT_SECONDARY:
        swap_colors(a);
        break;
    case HIT_PALETTE:
        if (left) a->primary = a->palette[h.index];
        else if (b->button == SDL_BUTTON_RIGHT) a->secondary = a->palette[h.index];
        break;
    case HIT_SLIDER:
        if (left) {
            app_set_size(a, ui_slider_value(b->x));
            start_drag(a, DRAG_SLIDER);
        }
        break;
    default:
        break;
    }
    a->redraw = true;
}

static void on_mouse_up(App *a, SDL_MouseButtonEvent *b)
{
    switch (a->drag) {
    case DRAG_PAINT:
        if (b->button == a->drag_button) end_paint(a);
        break;
    case DRAG_PAN:
        if (b->button == SDL_BUTTON_MIDDLE || b->button == SDL_BUTTON_LEFT) stop_drag(a);
        break;
    case DRAG_SLIDER:
        if (b->button == SDL_BUTTON_LEFT) stop_drag(a);
        break;
    default:
        break;
    }
}

static void on_mouse_move(App *a, SDL_MouseMotionEvent *m)
{
    a->mouse_x = m->x;
    a->mouse_y = m->y;
    a->mouse_inside = view_in_area(&a->view, m->x, m->y);
    switch (a->drag) {
    case DRAG_PAINT:
        move_paint(a, m->x, m->y);
        break;
    case DRAG_PAN:
        view_pan(&a->view, m->x - a->pan_x, m->y - a->pan_y);
        a->pan_x = (float)m->x;
        a->pan_y = (float)m->y;
        break;
    case DRAG_SLIDER:
        app_set_size(a, ui_slider_value(m->x));
        break;
    default:
        break;
    }
    a->redraw = true;
}

static void on_wheel(App *a, SDL_MouseWheelEvent *w)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
    float y = w->preciseY;
#else
    float y = (float)w->y;
#endif
    if (w->direction == SDL_MOUSEWHEEL_FLIPPED) y = -y;
    if (y == 0.0f) return;
    if (ctrl_down(SDL_GetModState())) {
        int step = size_step(a->size);
        app_set_size(a, a->size + (y > 0 ? step : -step));
        return;
    }
    if (!a->mouse_inside) return;
    view_zoom_at(&a->view, powf(WHEEL_ZOOM, y), a->mouse_x + 0.5f, a->mouse_y + 0.5f);
    if (a->drag == DRAG_PAINT) move_paint(a, a->mouse_x, a->mouse_y);
    a->redraw = true;
}

static void on_drop(App *a, char *path)
{
    if (a->drag != DRAG_NONE) return;
    Uint32 now = SDL_GetTicks();
    bool armed = a->drop_armed_until && !SDL_TICKS_PASSED(now, a->drop_armed_until) && strcmp(a->drop_path, path) == 0;
    if (app_modified(a) && !armed) {
        snprintf(a->drop_path, sizeof a->drop_path, "%s", path);
        a->drop_armed_until = now + DROP_CONFIRM_MS;
        app_message(a, "Unsaved changes. Drop the file again to open it anyway");
        return;
    }
    a->drop_armed_until = 0;
    app_open(a, path);
}

static void on_key_down(App *a, SDL_KeyboardEvent *k)
{
    SDL_Keycode key = k->keysym.sym;
    Uint16 mod = k->keysym.mod;
    bool shift = (mod & KMOD_SHIFT) != 0;

    if (key == SDLK_LSHIFT || key == SDLK_RSHIFT) {
        a->shift_held = true;
        if (a->drag == DRAG_PAINT && is_shape(a->tool)) move_paint(a, a->mouse_x, a->mouse_y);
        return;
    }
    if (key == SDLK_SPACE) {
        a->space_held = true;
        a->redraw = true;
        return;
    }
    if (key == SDLK_ESCAPE) {
        if (a->drag == DRAG_PAINT) cancel_paint(a);
        else if (a->show_help) a->show_help = false;
        a->redraw = true;
        return;
    }
    if (key == SDLK_F1) {
        if (!k->repeat) a->show_help = !a->show_help;
        a->redraw = true;
        return;
    }

    if (ctrl_down(mod)) {
        switch (key) {
        case SDLK_z:
            if (shift) app_redo(a);
            else app_undo(a);
            break;
        case SDLK_y:
            app_redo(a);
            break;
        case SDLK_s:
            if (!k->repeat) app_save(a, shift);
            break;
        case SDLK_n:
            if (!k->repeat) app_clear(a);
            break;
        case SDLK_q:
        case SDLK_w:
            if (!k->repeat) app_request_quit(a);
            break;
        case SDLK_0:
        case SDLK_KP_0:
            view_actual(&a->view);
            break;
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS:
            zoom_center(a, ZOOM_STEP);
            break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            zoom_center(a, 1.0f / ZOOM_STEP);
            break;
        default:
            break;
        }
        a->redraw = true;
        return;
    }
    if (a->drag == DRAG_PAINT) return;
    if (k->repeat && key != SDLK_LEFTBRACKET && key != SDLK_RIGHTBRACKET && key != SDLK_EQUALS &&
        key != SDLK_PLUS && key != SDLK_KP_PLUS && key != SDLK_MINUS && key != SDLK_KP_MINUS)
        return;

    switch (key) {
    case SDLK_b: app_set_tool(a, TOOL_BRUSH); break;
    case SDLK_e: app_set_tool(a, TOOL_ERASER); break;
    case SDLK_l: app_set_tool(a, TOOL_LINE); break;
    case SDLK_r: app_set_tool(a, TOOL_RECT); break;
    case SDLK_o: app_set_tool(a, TOOL_ELLIPSE); break;
    case SDLK_f: app_set_tool(a, TOOL_FILL); break;
    case SDLK_i: app_set_tool(a, TOOL_PICKER); break;
    case SDLK_g: a->filled = !a->filled; break;
    case SDLK_x: swap_colors(a); break;
    case SDLK_h: a->show_help = !a->show_help; break;
    case SDLK_LEFTBRACKET: app_set_size(a, a->size - size_step(a->size - 1)); break;
    case SDLK_RIGHTBRACKET: app_set_size(a, a->size + size_step(a->size)); break;
    case SDLK_0:
    case SDLK_KP_0:
        view_fit(&a->view);
        break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_KP_PLUS:
        zoom_center(a, ZOOM_STEP);
        break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
        zoom_center(a, 1.0f / ZOOM_STEP);
        break;
    default:
        if (key >= SDLK_1 && key <= SDLK_9) a->primary = a->palette[key - SDLK_1];
        break;
    }
    a->redraw = true;
}

static void on_key_up(App *a, SDL_KeyboardEvent *k)
{
    SDL_Keycode key = k->keysym.sym;
    if (key == SDLK_LSHIFT || key == SDLK_RSHIFT) {
        a->shift_held = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (a->drag == DRAG_PAINT && is_shape(a->tool)) move_paint(a, a->mouse_x, a->mouse_y);
    } else if (key == SDLK_SPACE) {
        a->space_held = false;
        a->redraw = true;
    }
}

static void on_window(App *a, SDL_WindowEvent *w)
{
    switch (w->event) {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        a->win_w = w->data1;
        a->win_h = w->data2;
        app_layout(a);
        break;
    case SDL_WINDOWEVENT_LEAVE:
        a->mouse_inside = false;
        a->redraw = true;
        break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
        a->space_held = false;
        a->shift_held = false;
        if (a->drag == DRAG_PAINT) end_paint(a);
        else if (a->drag != DRAG_NONE) stop_drag(a);
        break;
    default:
        a->redraw = true;
        break;
    }
}

void input_event(App *a, SDL_Event *e)
{
    switch (e->type) {
    case SDL_QUIT:
        app_request_quit(a);
        break;
    case SDL_WINDOWEVENT:
        on_window(a, &e->window);
        break;
    case SDL_MOUSEMOTION:
        on_mouse_move(a, &e->motion);
        break;
    case SDL_MOUSEBUTTONDOWN:
        on_mouse_down(a, &e->button);
        break;
    case SDL_MOUSEBUTTONUP:
        on_mouse_up(a, &e->button);
        break;
    case SDL_MOUSEWHEEL:
        on_wheel(a, &e->wheel);
        break;
    case SDL_KEYDOWN:
        on_key_down(a, &e->key);
        break;
    case SDL_KEYUP:
        on_key_up(a, &e->key);
        break;
    case SDL_DROPFILE:
        on_drop(a, e->drop.file);
        SDL_free(e->drop.file);
        break;
    default:
        if (e->type == a->save_event) app_save_done(a, e);
        break;
    }
    update_cursor(a);
}
