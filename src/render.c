#include "render.h"
#include "draw.h"
#include "ui.h"

#include <math.h>

#define RING_POINTS 2048
#define GRID_ZOOM 12.0f

static void upload(App *a)
{
    Canvas *c = &a->canvas;
    if (box_is_empty(c->dirty)) return;
    SDL_Rect r = { c->dirty.x0, c->dirty.y0, c->dirty.x1 - c->dirty.x0 + 1, c->dirty.y1 - c->dirty.y0 + 1 };
    const uint32_t *src = c->px + (size_t)r.y * c->w + r.x;
    SDL_UpdateTexture(a->texture, &r, src, c->w * (int)sizeof(uint32_t));
    c->dirty = box_empty();
}

static void draw_canvas(App *a)
{
    SDL_Renderer *r = a->renderer;
    const View *v = &a->view;
    SDL_FRect full = view_canvas_rect(v);
    float z = v->zoom;
    int sx0 = (int)floorf((v->area.x - full.x) / z);
    int sy0 = (int)floorf((v->area.y - full.y) / z);
    int sx1 = (int)ceilf((v->area.x + v->area.w - full.x) / z);
    int sy1 = (int)ceilf((v->area.y + v->area.h - full.y) / z);
    if (sx0 < 0) sx0 = 0;
    if (sy0 < 0) sy0 = 0;
    if (sx1 > a->canvas.w) sx1 = a->canvas.w;
    if (sy1 > a->canvas.h) sy1 = a->canvas.h;
    if (sx1 <= sx0 || sy1 <= sy0) return;

    SDL_Rect shadow = { (int)full.x + 3, (int)full.y + 3, (int)ceilf(full.w), (int)ceilf(full.h) };
    SDL_SetRenderDrawColor(r, 0x22, 0x22, 0x26, 0xFF);
    SDL_RenderFillRect(r, &shadow);

    SDL_Rect src = { sx0, sy0, sx1 - sx0, sy1 - sy0 };
    SDL_FRect dst = { full.x + sx0 * z, full.y + sy0 * z, src.w * z, src.h * z };
    SDL_SetTextureScaleMode(a->texture, z >= 1.0f ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
    SDL_RenderCopyF(r, a->texture, &src, &dst);

    if (z >= GRID_ZOOM) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0x80, 0x80, 0x80, 0x60);
        for (int x = sx0; x <= sx1; x++) {
            float px = full.x + x * z;
            SDL_RenderDrawLineF(r, px, dst.y, px, dst.y + dst.h);
        }
        for (int y = sy0; y <= sy1; y++) {
            float py = full.y + y * z;
            SDL_RenderDrawLineF(r, dst.x, py, dst.x + dst.w, py);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

static void ring(SDL_Renderer *r, float cx, float cy, float radius, uint8_t shade)
{
    static SDL_FPoint pts[RING_POINTS];
    int n = (int)(radius * 6.3f);
    if (n < 16) n = 16;
    if (n > RING_POINTS) n = RING_POINTS;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n * 6.2831853f;
        pts[i].x = cx + cosf(t) * radius;
        pts[i].y = cy + sinf(t) * radius;
    }
    SDL_SetRenderDrawColor(r, shade, shade, shade, 0xFF);
    SDL_RenderDrawPointsF(r, pts, n);
}

static void draw_brush_cursor(App *a)
{
    if (!a->mouse_inside || a->show_help || a->drag == DRAG_PAN || a->space_held) return;
    if (a->tool != TOOL_BRUSH && a->tool != TOOL_ERASER && a->tool != TOOL_LINE) return;
    float cx, cy;
    view_to_canvas(&a->view, a->mouse_x + 0.5f, a->mouse_y + 0.5f, &cx, &cy);
    cx = draw_snap(cx, a->size);
    cy = draw_snap(cy, a->size);
    const View *v = &a->view;
    float sx = v->area.x + v->ox + cx * v->zoom;
    float sy = v->area.y + v->oy + cy * v->zoom;
    float radius = a->size * 0.5f * v->zoom;
    if (radius < 3.0f) return;
    ring(a->renderer, sx, sy, radius, 0x10);
    ring(a->renderer, sx, sy, radius + 1.0f, 0xF0);
}

void render_frame(App *a)
{
    SDL_Renderer *r = a->renderer;
    upload(a);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0x3A, 0x3A, 0x40, 0xFF);
    SDL_RenderClear(r);
    SDL_RenderSetClipRect(r, &a->view.area);
    draw_canvas(a);
    draw_brush_cursor(a);
    SDL_RenderSetClipRect(r, NULL);
    ui_draw(a);
    SDL_RenderPresent(r);
    a->redraw = false;
}
