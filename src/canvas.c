#include "canvas.h"
#include "history.h"

#include <stdlib.h>

bool canvas_init(Canvas *c, int w, int h, uint32_t fill)
{
    if (w < 1 || h < 1 || w > CANVAS_MAX_SIDE || h > CANVAS_MAX_SIDE) return false;
    uint32_t *px = malloc((size_t)w * h * sizeof *px);
    if (!px) return false;
    canvas_adopt(c, px, w, h);
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) px[i] = fill;
    return true;
}

void canvas_adopt(Canvas *c, uint32_t *px, int w, int h)
{
    free(c->px);
    c->px = px;
    c->w = w;
    c->h = h;
    c->dirty = box_empty();
    canvas_mark(c, 0, 0, w - 1, h - 1);
}

void canvas_free(Canvas *c)
{
    free(c->px);
    c->px = NULL;
    c->w = c->h = 0;
}

void canvas_mark(Canvas *c, int x0, int y0, int x1, int y1)
{
    Box b = { x0, y0, x1, y1 };
    c->dirty = box_union(c->dirty, b);
}

void canvas_span(Canvas *c, int y, int x0, int x1, uint32_t color)
{
    if (y < 0 || y >= c->h) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= c->w) x1 = c->w - 1;
    if (x0 > x1) return;
    if (c->hist) history_save(c->hist, x0, y, x1, y);
    uint32_t *row = c->px + (size_t)y * c->w;
    for (int x = x0; x <= x1; x++) row[x] = color;
    canvas_mark(c, x0, y, x1, y);
}

void canvas_fill(Canvas *c, uint32_t color)
{
    for (int y = 0; y < c->h; y++) canvas_span(c, y, 0, c->w - 1, color);
}
