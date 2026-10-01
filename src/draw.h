#ifndef SPINT_DRAW_H
#define SPINT_DRAW_H

#include "canvas.h"

float draw_snap(float v, int size);
void draw_capsule(Canvas *c, float ax, float ay, float bx, float by, int size, uint32_t color);
void draw_rect(Canvas *c, int x0, int y0, int x1, int y1, int thickness, bool filled, uint32_t color);
void draw_ellipse(Canvas *c, int x0, int y0, int x1, int y1, int thickness, bool filled, uint32_t color);
bool draw_flood(Canvas *c, int x, int y, uint32_t color);

#endif
