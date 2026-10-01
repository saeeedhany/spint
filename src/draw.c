#include "draw.h"

#include <math.h>
#include <stdlib.h>

static void order(int *a, int *b)
{
    if (*a > *b) {
        int t = *a;
        *a = *b;
        *b = t;
    }
}

static void put_span(Canvas *c, int y, float left, float right, uint32_t color)
{
    if (left > right) return;
    int x0 = (int)ceilf(left - 0.5f);
    int x1 = (int)floorf(right - 0.5f);
    if (x0 <= x1) canvas_span(c, y, x0, x1, color);
}

float draw_snap(float v, int size)
{
    return (size & 1) ? floorf(v) + 0.5f : floorf(v + 0.5f);
}

static void disc_extent(float cx, float cy, float r, float yc, float *l, float *rr)
{
    float dy = yc - cy;
    float q = r * r - dy * dy;
    if (q < 0.0f) return;
    float h = sqrtf(q);
    if (cx - h < *l) *l = cx - h;
    if (cx + h > *rr) *rr = cx + h;
}

void draw_capsule(Canvas *c, float ax, float ay, float bx, float by, int size, uint32_t color)
{
    if (size < 1) size = 1;
    ax = draw_snap(ax, size);
    ay = draw_snap(ay, size);
    bx = draw_snap(bx, size);
    by = draw_snap(by, size);
    float r = size * 0.5f;
    float dx = bx - ax, dy = by - ay;
    float len = sqrtf(dx * dx + dy * dy);
    float px[4], py[4];
    bool band = len > 0.0f;
    if (band) {
        float nx = -dy / len * r, ny = dx / len * r;
        px[0] = ax + nx; py[0] = ay + ny;
        px[1] = bx + nx; py[1] = by + ny;
        px[2] = bx - nx; py[2] = by - ny;
        px[3] = ax - nx; py[3] = ay - ny;
    }
    float top = (ay < by ? ay : by) - r;
    float bottom = (ay > by ? ay : by) + r;
    int y0 = (int)floorf(top);
    int y1 = (int)ceilf(bottom);
    if (y0 < 0) y0 = 0;
    if (y1 > c->h - 1) y1 = c->h - 1;
    for (int y = y0; y <= y1; y++) {
        float yc = y + 0.5f;
        float l = INFINITY, rr = -INFINITY;
        disc_extent(ax, ay, r, yc, &l, &rr);
        if (band) {
            disc_extent(bx, by, r, yc, &l, &rr);
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) & 3;
                float ya = py[i], yb = py[j];
                if (ya == yb) continue;
                if ((yc < ya && yc < yb) || (yc > ya && yc > yb)) continue;
                float x = px[i] + (yc - ya) * (px[j] - px[i]) / (yb - ya);
                if (x < l) l = x;
                if (x > rr) rr = x;
            }
        }
        put_span(c, y, l, rr, color);
    }
}

void draw_rect(Canvas *c, int x0, int y0, int x1, int y1, int thickness, bool filled, uint32_t color)
{
    order(&x0, &x1);
    order(&y0, &y1);
    if (thickness < 1) thickness = 1;
    if (2 * thickness >= x1 - x0 + 1 || 2 * thickness >= y1 - y0 + 1) filled = true;
    int ys = y0 < 0 ? 0 : y0;
    int ye = y1 >= c->h ? c->h - 1 : y1;
    for (int y = ys; y <= ye; y++) {
        if (filled || y < y0 + thickness || y > y1 - thickness) {
            canvas_span(c, y, x0, x1, color);
        } else {
            canvas_span(c, y, x0, x0 + thickness - 1, color);
            canvas_span(c, y, x1 - thickness + 1, x1, color);
        }
    }
}

static bool ellipse_half(float a, float b, float dy, float *half)
{
    if (a <= 0.0f || b <= 0.0f) return false;
    float t = dy / b;
    float q = 1.0f - t * t;
    if (q < 0.0f) return false;
    *half = a * sqrtf(q);
    return true;
}

void draw_ellipse(Canvas *c, int x0, int y0, int x1, int y1, int thickness, bool filled, uint32_t color)
{
    order(&x0, &x1);
    order(&y0, &y1);
    if (thickness < 1) thickness = 1;
    float cx = (x0 + x1 + 1) * 0.5f;
    float cy = (y0 + y1 + 1) * 0.5f;
    float a = (x1 - x0 + 1) * 0.5f;
    float b = (y1 - y0 + 1) * 0.5f;
    float ia = a - thickness, ib = b - thickness;
    if (ia <= 0.0f || ib <= 0.0f) filled = true;
    int ys = y0 < 0 ? 0 : y0;
    int ye = y1 >= c->h ? c->h - 1 : y1;
    for (int y = ys; y <= ye; y++) {
        float dy = y + 0.5f - cy;
        float ho, hi;
        if (!ellipse_half(a, b, dy, &ho)) continue;
        int ox0 = (int)ceilf(cx - ho - 0.5f);
        int ox1 = (int)floorf(cx + ho - 0.5f);
        if (ox0 > ox1) continue;
        if (filled || !ellipse_half(ia, ib, dy, &hi)) {
            canvas_span(c, y, ox0, ox1, color);
            continue;
        }
        int ix0 = (int)ceilf(cx - hi - 0.5f);
        int ix1 = (int)floorf(cx + hi - 0.5f);
        if (ix0 > ix1) {
            canvas_span(c, y, ox0, ox1, color);
            continue;
        }
        int le = ix0 - 1 > ox0 ? ix0 - 1 : ox0;
        int rs = ix1 + 1 < ox1 ? ix1 + 1 : ox1;
        if (rs <= le + 1) {
            canvas_span(c, y, ox0, ox1, color);
        } else {
            canvas_span(c, y, ox0, le, color);
            canvas_span(c, y, rs, ox1, color);
        }
    }
}

typedef struct {
    int x, y;
} Seed;

typedef struct {
    Seed *items;
    size_t count, cap;
} SeedStack;

static bool seed_push(SeedStack *s, int x, int y)
{
    if (s->count == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 1024;
        Seed *items = realloc(s->items, cap * sizeof *items);
        if (!items) return false;
        s->items = items;
        s->cap = cap;
    }
    s->items[s->count].x = x;
    s->items[s->count].y = y;
    s->count++;
    return true;
}

static bool seed_row(SeedStack *s, const Canvas *c, int y, int xl, int xr, uint32_t target)
{
    if (y < 0 || y >= c->h) return true;
    const uint32_t *row = c->px + (size_t)y * c->w;
    int x = xl;
    while (x <= xr) {
        while (x <= xr && row[x] != target) x++;
        if (x > xr) break;
        if (!seed_push(s, x, y)) return false;
        while (x <= xr && row[x] == target) x++;
    }
    return true;
}

bool draw_flood(Canvas *c, int x, int y, uint32_t color)
{
    if (!canvas_contains(c, x, y)) return false;
    uint32_t target = canvas_get(c, x, y);
    if (target == color) return false;
    SeedStack s = { 0 };
    bool ok = seed_push(&s, x, y);
    while (ok && s.count) {
        Seed p = s.items[--s.count];
        uint32_t *row = c->px + (size_t)p.y * c->w;
        if (row[p.x] != target) continue;
        int xl = p.x, xr = p.x;
        while (xl > 0 && row[xl - 1] == target) xl--;
        while (xr < c->w - 1 && row[xr + 1] == target) xr++;
        canvas_span(c, p.y, xl, xr, color);
        ok = seed_row(&s, c, p.y - 1, xl, xr, target) && seed_row(&s, c, p.y + 1, xl, xr, target);
    }
    free(s.items);
    return true;
}
