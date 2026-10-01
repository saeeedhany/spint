#ifndef SPINT_CANVAS_H
#define SPINT_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CANVAS_MAX_SIDE 16384

typedef struct {
    int x0, y0, x1, y1;
} Box;

struct History;

typedef struct {
    int w, h;
    uint32_t *px;
    Box dirty;
    struct History *hist;
} Canvas;

static inline Box box_empty(void)
{
    Box b = { 0, 0, -1, -1 };
    return b;
}

static inline bool box_is_empty(Box b)
{
    return b.x1 < b.x0 || b.y1 < b.y0;
}

static inline Box box_union(Box a, Box b)
{
    if (box_is_empty(a)) return b;
    if (box_is_empty(b)) return a;
    Box r = {
        a.x0 < b.x0 ? a.x0 : b.x0,
        a.y0 < b.y0 ? a.y0 : b.y0,
        a.x1 > b.x1 ? a.x1 : b.x1,
        a.y1 > b.y1 ? a.y1 : b.y1,
    };
    return r;
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

bool canvas_init(Canvas *c, int w, int h, uint32_t fill);
void canvas_adopt(Canvas *c, uint32_t *px, int w, int h);
void canvas_free(Canvas *c);
void canvas_mark(Canvas *c, int x0, int y0, int x1, int y1);
void canvas_span(Canvas *c, int y, int x0, int x1, uint32_t color);
void canvas_fill(Canvas *c, uint32_t color);

static inline bool canvas_contains(const Canvas *c, int x, int y)
{
    return x >= 0 && y >= 0 && x < c->w && y < c->h;
}

static inline uint32_t canvas_get(const Canvas *c, int x, int y)
{
    return c->px[(size_t)y * c->w + x];
}

#endif
