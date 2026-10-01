#include "view.h"

#include <math.h>

#define ZOOM_MIN 0.02f
#define ZOOM_MAX 64.0f
#define FIT_MARGIN 24

static void clamp_pan(View *v)
{
    float w = v->cw * v->zoom, h = v->ch * v->zoom;
    float keep = 48.0f;
    float minx = keep - w, maxx = v->area.w - keep;
    float miny = keep - h, maxy = v->area.h - keep;
    if (v->ox < minx) v->ox = minx;
    if (v->ox > maxx) v->ox = maxx;
    if (v->oy < miny) v->oy = miny;
    if (v->oy > maxy) v->oy = maxy;
    v->ox = roundf(v->ox);
    v->oy = roundf(v->oy);
}

static void center(View *v)
{
    v->ox = (v->area.w - v->cw * v->zoom) * 0.5f;
    v->oy = (v->area.h - v->ch * v->zoom) * 0.5f;
    clamp_pan(v);
}

void view_set_area(View *v, SDL_Rect area)
{
    if (v->area.w > 0 && v->area.h > 0) {
        v->ox += (area.w - v->area.w) * 0.5f;
        v->oy += (area.h - v->area.h) * 0.5f;
    }
    v->area = area;
    clamp_pan(v);
}

void view_set_canvas(View *v, int cw, int ch)
{
    v->cw = cw;
    v->ch = ch;
    view_fit(v);
}

void view_fit(View *v)
{
    float zx = (float)(v->area.w - 2 * FIT_MARGIN) / v->cw;
    float zy = (float)(v->area.h - 2 * FIT_MARGIN) / v->ch;
    float z = zx < zy ? zx : zy;
    if (z > 1.0f) z = 1.0f;
    if (z < ZOOM_MIN) z = ZOOM_MIN;
    v->zoom = z;
    center(v);
}

void view_actual(View *v)
{
    view_zoom_at(v, 1.0f / v->zoom, v->area.x + v->area.w * 0.5f, v->area.y + v->area.h * 0.5f);
}

void view_zoom_at(View *v, float factor, float sx, float sy)
{
    float z = v->zoom * factor;
    if (z < ZOOM_MIN) z = ZOOM_MIN;
    if (z > ZOOM_MAX) z = ZOOM_MAX;
    if (fabsf(z - 1.0f) < 0.04f) z = 1.0f;
    float cx, cy;
    view_to_canvas(v, sx, sy, &cx, &cy);
    v->zoom = z;
    v->ox = sx - v->area.x - cx * z;
    v->oy = sy - v->area.y - cy * z;
    clamp_pan(v);
}

void view_pan(View *v, float dx, float dy)
{
    v->ox += dx;
    v->oy += dy;
    clamp_pan(v);
}

void view_to_canvas(const View *v, float sx, float sy, float *cx, float *cy)
{
    *cx = (sx - v->area.x - v->ox) / v->zoom;
    *cy = (sy - v->area.y - v->oy) / v->zoom;
}

SDL_FRect view_canvas_rect(const View *v)
{
    SDL_FRect r = { v->area.x + v->ox, v->area.y + v->oy, v->cw * v->zoom, v->ch * v->zoom };
    return r;
}

bool view_in_area(const View *v, int sx, int sy)
{
    SDL_Point p = { sx, sy };
    return SDL_PointInRect(&p, &v->area);
}
