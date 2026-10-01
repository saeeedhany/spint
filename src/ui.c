#include "ui.h"
#include "font.h"

#include <stdio.h>
#include <string.h>

#define PAD 8
#define BUTTON 36
#define BUTTON_STEP 40
#define ICON 12
#define ICON_SCALE 2
#define ICON_COUNT 8
#define ICON_FILLED 7

#define FILLED_X (PAD + TOOL_COUNT * BUTTON_STEP + 8)
#define SWATCH 26
#define SWATCH_X (FILLED_X + BUTTON + 20)
#define PALETTE_X (SWATCH_X + 58)
#define CELL 18
#define CELL_STEP 20
#define PALETTE_COLS 10
#define SLIDER_X (PALETTE_X + PALETTE_COLS * CELL_STEP + 16)
#define SLIDER_W 140
#define SLIDER_Y (UI_TOOLBAR_H / 2)

#define COLOR_BAR 0xFF2B2B2F
#define COLOR_BAR_EDGE 0xFF1C1C1F
#define COLOR_HOVER 0xFF3E3E44
#define COLOR_ACTIVE 0xFF2F6FB5
#define COLOR_ICON 0xFFE6E6E6
#define COLOR_TEXT 0xFFD8D8D8
#define COLOR_DIM 0xFF9A9AA2
#define COLOR_STATUS 0xFF202024
#define COLOR_ACCENT 0xFF6CB4FF

static const char *const icons[ICON_COUNT][ICON] = {
    {
        "..........##",
        ".........###",
        "........###.",
        ".......###..",
        "......###...",
        ".....###....",
        "....###.....",
        "...##.......",
        ".####.......",
        "#####.......",
        "####........",
        "##..........",
    },
    {
        "............",
        "......#####.",
        ".....#....##",
        "....#....#.#",
        "...#....#..#",
        "..######...#",
        "..#....#..#.",
        "..#....#.#..",
        "..#....##...",
        "..######....",
        "............",
        "############",
    },
    {
        "##..........",
        "###.........",
        ".###........",
        "..###.......",
        "...###......",
        "....###.....",
        ".....###....",
        "......###...",
        ".......###..",
        "........###.",
        ".........###",
        "..........##",
    },
    {
        "............",
        "############",
        "##........##",
        "##........##",
        "##........##",
        "##........##",
        "##........##",
        "##........##",
        "##........##",
        "############",
        "............",
        "............",
    },
    {
        "....####....",
        "..########..",
        ".##......##.",
        "##........##",
        "##........##",
        "##........##",
        "##........##",
        "##........##",
        "##........##",
        ".##......##.",
        "..########..",
        "....####....",
    },
    {
        ".....#......",
        "....###.....",
        "...#####....",
        "..#######...",
        ".#########..",
        "###########.",
        ".#########.#",
        "..#######..#",
        "...#####..##",
        "....###...##",
        ".....#......",
        "............",
    },
    {
        ".........##.",
        "........####",
        ".......#####",
        "......####..",
        ".....###....",
        "....#.#.....",
        "...#.#......",
        "..#.#.......",
        ".#.#........",
        ".##.........",
        "#...........",
        "............",
    },
    {
        "............",
        "############",
        "############",
        "############",
        "############",
        "############",
        "############",
        "############",
        "############",
        "############",
        "............",
        "............",
    },
};

static const char *const tool_keys[TOOL_COUNT] = { "B", "E", "L", "R", "O", "F", "I" };

static const char *const help_lines[] = {
    "Spint shortcuts",
    "",
    "B Brush  E Eraser  L Line  R Rectangle  O Ellipse",
    "F Fill  I Picker  G Toggle filled shapes",
    "Left button paints primary, right paints secondary",
    "1-9 Palette color  X Swap colors",
    "[ ] or Ctrl+Wheel  Brush size",
    "Shift  Straight lines, squares and circles",
    "Shift+Click with brush  Line from last point",
    "Alt+Click  Pick a color from the canvas",
    "Wheel  Zoom  Middle drag or Space+drag  Pan",
    "0 Fit to window  Ctrl+0 Actual size",
    "Ctrl+Z Undo  Ctrl+Y or Ctrl+Shift+Z Redo",
    "Ctrl+S Save  Ctrl+Shift+S Save as new file",
    "Ctrl+N Clear canvas  Drop an image to open it",
    "Esc Cancel shape  Ctrl+Q Quit  F1 Close help",
};

static SDL_Texture *icon_atlas;

bool ui_init(SDL_Renderer *r)
{
    static uint32_t px[ICON][ICON * ICON_COUNT];
    memset(px, 0, sizeof px);
    for (int i = 0; i < ICON_COUNT; i++)
        for (int y = 0; y < ICON; y++)
            for (int x = 0; x < ICON; x++)
                if (icons[i][y][x] == '#') px[y][i * ICON + x] = 0xFFFFFFFFu;
    icon_atlas = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, ICON * ICON_COUNT, ICON);
    if (!icon_atlas) return false;
    SDL_UpdateTexture(icon_atlas, NULL, px, ICON * ICON_COUNT * sizeof(uint32_t));
    SDL_SetTextureBlendMode(icon_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(icon_atlas, SDL_ScaleModeNearest);
    return true;
}

void ui_free(void)
{
    if (icon_atlas) SDL_DestroyTexture(icon_atlas);
    icon_atlas = NULL;
}

const char *ui_tool_key(int tool)
{
    return tool_keys[tool];
}

static SDL_Rect tool_rect(int i)
{
    SDL_Rect r = { PAD + i * BUTTON_STEP, PAD, BUTTON, BUTTON };
    return r;
}

static SDL_Rect filled_rect(void)
{
    SDL_Rect r = { FILLED_X, PAD, BUTTON, BUTTON };
    return r;
}

static SDL_Rect primary_rect(void)
{
    SDL_Rect r = { SWATCH_X, PAD, SWATCH, SWATCH };
    return r;
}

static SDL_Rect secondary_rect(void)
{
    SDL_Rect r = { SWATCH_X + 14, PAD + 12, SWATCH, SWATCH };
    return r;
}

static SDL_Rect cell_rect(int i)
{
    SDL_Rect r = { PALETTE_X + (i % PALETTE_COLS) * CELL_STEP, PAD + 1 + (i / PALETTE_COLS) * CELL_STEP, CELL, CELL };
    return r;
}

static SDL_Rect slider_rect(void)
{
    SDL_Rect r = { SLIDER_X - 8, PAD, SLIDER_W + 16, BUTTON };
    return r;
}

static bool inside(SDL_Rect r, int x, int y)
{
    SDL_Point p = { x, y };
    return SDL_PointInRect(&p, &r);
}

Hit ui_hit(int win_w, int win_h, int x, int y)
{
    Hit h = { HIT_NONE, 0 };
    (void)win_w;
    if (y >= win_h - UI_STATUS_H) {
        h.kind = HIT_BAR;
        return h;
    }
    if (y >= UI_TOOLBAR_H) {
        h.kind = HIT_CANVAS;
        return h;
    }
    h.kind = HIT_BAR;
    for (int i = 0; i < TOOL_COUNT; i++)
        if (inside(tool_rect(i), x, y)) {
            h.kind = HIT_TOOL;
            h.index = i;
            return h;
        }
    if (inside(filled_rect(), x, y)) h.kind = HIT_FILLED;
    else if (inside(primary_rect(), x, y)) h.kind = HIT_PRIMARY;
    else if (inside(secondary_rect(), x, y)) h.kind = HIT_SECONDARY;
    else if (inside(slider_rect(), x, y)) h.kind = HIT_SLIDER;
    else
        for (int i = 0; i < PALETTE_COUNT; i++)
            if (inside(cell_rect(i), x, y)) {
                h.kind = HIT_PALETTE;
                h.index = i;
                break;
            }
    return h;
}

static float slider_t(int size)
{
    float t = (float)(size - BRUSH_MIN) / (BRUSH_MAX - BRUSH_MIN);
    return SDL_sqrtf(t);
}

int ui_slider_value(int x)
{
    float t = (float)(x - SLIDER_X) / SLIDER_W;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return BRUSH_MIN + (int)(t * t * (BRUSH_MAX - BRUSH_MIN) + 0.5f);
}

static void set_color(SDL_Renderer *r, uint32_t c)
{
    SDL_SetRenderDrawColor(r, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, (c >> 24) & 0xFF);
}

static void fill(SDL_Renderer *r, SDL_Rect rect, uint32_t c)
{
    set_color(r, c);
    SDL_RenderFillRect(r, &rect);
}

static void outline(SDL_Renderer *r, SDL_Rect rect, uint32_t c)
{
    set_color(r, c);
    SDL_RenderDrawRect(r, &rect);
}

static void draw_icon(SDL_Renderer *r, int icon, SDL_Rect button, uint32_t c)
{
    SDL_Rect src = { icon * ICON, 0, ICON, ICON };
    int s = ICON * ICON_SCALE;
    SDL_Rect dst = { button.x + (button.w - s) / 2, button.y + (button.h - s) / 2, s, s };
    SDL_SetTextureColorMod(icon_atlas, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    SDL_RenderCopy(r, icon_atlas, &src, &dst);
}

static void draw_button(App *a, SDL_Rect rect, int icon, bool active)
{
    bool hover = inside(rect, a->mouse_x, a->mouse_y) && a->drag == DRAG_NONE;
    if (active) fill(a->renderer, rect, COLOR_ACTIVE);
    else if (hover) fill(a->renderer, rect, COLOR_HOVER);
    draw_icon(a->renderer, icon, rect, COLOR_ICON);
}

static void draw_swatch(SDL_Renderer *r, SDL_Rect rect, uint32_t c)
{
    fill(r, rect, 0xFF000000);
    SDL_Rect in = { rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2 };
    fill(r, in, 0xFFFFFFFF);
    SDL_Rect core = { rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4 };
    fill(r, core, c);
}

static void separator(SDL_Renderer *r, int x)
{
    SDL_Rect s = { x, PAD + 4, 1, BUTTON - 8 };
    fill(r, s, 0xFF45454C);
}

static void draw_toolbar(App *a)
{
    SDL_Renderer *r = a->renderer;
    SDL_Rect bar = { 0, 0, a->win_w, UI_TOOLBAR_H };
    fill(r, bar, COLOR_BAR);
    SDL_Rect edge = { 0, UI_TOOLBAR_H - 1, a->win_w, 1 };
    fill(r, edge, COLOR_BAR_EDGE);

    for (int i = 0; i < TOOL_COUNT; i++) draw_button(a, tool_rect(i), i, a->tool == (Tool)i);
    separator(r, FILLED_X - 6);
    draw_button(a, filled_rect(), a->filled ? ICON_FILLED : TOOL_RECT, a->filled);
    separator(r, SWATCH_X - 10);

    draw_swatch(r, secondary_rect(), a->secondary);
    draw_swatch(r, primary_rect(), a->primary);

    for (int i = 0; i < PALETTE_COUNT; i++) {
        SDL_Rect c = cell_rect(i);
        fill(r, c, a->palette[i]);
        bool hover = inside(c, a->mouse_x, a->mouse_y) && a->drag == DRAG_NONE;
        if (a->palette[i] == a->primary || hover) {
            SDL_Rect o = { c.x - 2, c.y - 2, c.w + 4, c.h + 4 };
            outline(r, o, hover ? COLOR_ACCENT : 0xFFFFFFFF);
        } else {
            outline(r, c, 0xFF55555C);
        }
    }
    separator(r, SLIDER_X - 10);

    SDL_Rect track = { SLIDER_X, SLIDER_Y - 2, SLIDER_W, 4 };
    fill(r, track, 0xFF4A4A52);
    int knob_x = SLIDER_X + (int)(slider_t(a->size) * SLIDER_W + 0.5f);
    SDL_Rect done = { SLIDER_X, SLIDER_Y - 2, knob_x - SLIDER_X, 4 };
    fill(r, done, COLOR_ACTIVE);
    SDL_Rect knob = { knob_x - 5, SLIDER_Y - 9, 10, 18 };
    bool hot = a->drag == DRAG_SLIDER || (inside(slider_rect(), a->mouse_x, a->mouse_y) && a->drag == DRAG_NONE);
    fill(r, knob, hot ? 0xFFFFFFFF : 0xFFD0D0D6);

    char text[32];
    snprintf(text, sizeof text, "%d px", a->size);
    font_draw(r, SLIDER_X + SLIDER_W + 14, SLIDER_Y - FONT_HEIGHT / 2, text, COLOR_TEXT);
}

static void hover_hint(App *a, char *out, size_t cap)
{
    out[0] = 0;
    if (a->drag != DRAG_NONE) return;
    Hit h = ui_hit(a->win_w, a->win_h, a->mouse_x, a->mouse_y);
    switch (h.kind) {
    case HIT_TOOL:
        snprintf(out, cap, "%s (%s)", app_tool_name((Tool)h.index), tool_keys[h.index]);
        break;
    case HIT_FILLED:
        snprintf(out, cap, "Filled shapes: %s (G)", a->filled ? "on" : "off");
        break;
    case HIT_PRIMARY:
    case HIT_SECONDARY:
        snprintf(out, cap, "Swap colors (X)");
        break;
    case HIT_PALETTE: {
        uint32_t c = a->palette[h.index];
        snprintf(out, cap, "#%06X  left: primary, right: secondary", c & 0xFFFFFF);
        break;
    }
    case HIT_SLIDER:
        snprintf(out, cap, "Brush size ([ and ])");
        break;
    default:
        break;
    }
}

static void draw_status(App *a)
{
    SDL_Renderer *r = a->renderer;
    int y0 = a->win_h - UI_STATUS_H;
    SDL_Rect bar = { 0, y0, a->win_w, UI_STATUS_H };
    fill(r, bar, COLOR_STATUS);
    SDL_Rect edge = { 0, y0, a->win_w, 1 };
    fill(r, edge, COLOR_BAR_EDGE);
    int ty = y0 + (UI_STATUS_H - FONT_HEIGHT) / 2;

    char pos[48] = "";
    if (a->mouse_inside) {
        float cx, cy;
        view_to_canvas(&a->view, a->mouse_x + 0.5f, a->mouse_y + 0.5f, &cx, &cy);
        int ix = (int)SDL_floorf(cx), iy = (int)SDL_floorf(cy);
        if (canvas_contains(&a->canvas, ix, iy)) snprintf(pos, sizeof pos, "%d,%d", ix, iy);
    }
    char left[160];
    snprintf(left, sizeof left, "%s  %dpx  %d%%  %dx%d  %s", app_tool_name(a->tool), a->size,
             (int)(a->view.zoom * 100.0f + 0.5f), a->canvas.w, a->canvas.h, pos);
    font_draw(r, 10, ty, left, COLOR_TEXT);

    char right[300];
    hover_hint(a, right, sizeof right);
    uint32_t color = COLOR_DIM;
    if (!right[0] && a->message[0]) {
        snprintf(right, sizeof right, "%s", a->message);
        color = COLOR_ACCENT;
    }
    if (!right[0]) snprintf(right, sizeof right, "%s", app_modified(a) ? "Unsaved changes  F1 Help" : "F1 Help");
    int min_x = 10 + font_width(left) + 24;
    int room = a->win_w - 10 - min_x;
    if (font_width(right) > room) {
        int keep = room / FONT_ADVANCE - 3;
        if (keep < 0) keep = 0;
        snprintf(right + keep, sizeof right - (size_t)keep, "...");
        if (keep == 0) right[0] = 0;
    }
    font_draw(r, a->win_w - 10 - font_width(right), ty, right, color);
}

static void draw_help(App *a)
{
    SDL_Renderer *r = a->renderer;
    int n = (int)(sizeof help_lines / sizeof *help_lines);
    int w = 0;
    for (int i = 0; i < n; i++) {
        int lw = font_width(help_lines[i]);
        if (lw > w) w = lw;
    }
    int line = FONT_HEIGHT + 8;
    SDL_Rect panel = { 0, 0, w + 48, n * line + 40 };
    panel.x = (a->win_w - panel.w) / 2;
    panel.y = UI_TOOLBAR_H + (a->win_h - UI_TOOLBAR_H - UI_STATUS_H - panel.h) / 2;
    if (panel.y < UI_TOOLBAR_H + 4) panel.y = UI_TOOLBAR_H + 4;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    fill(r, panel, 0xF0202024);
    outline(r, panel, COLOR_ACTIVE);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    for (int i = 0; i < n; i++)
        font_draw(r, panel.x + 24, panel.y + 20 + i * line, help_lines[i], i == 0 ? COLOR_ACCENT : COLOR_TEXT);
}

void ui_draw(App *a)
{
    draw_toolbar(a);
    draw_status(a);
    if (a->show_help) draw_help(a);
}
