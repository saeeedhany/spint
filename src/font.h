#ifndef SPINT_FONT_H
#define SPINT_FONT_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#define FONT_SCALE 2
#define FONT_ADVANCE (6 * FONT_SCALE)
#define FONT_HEIGHT (7 * FONT_SCALE)

bool font_init(SDL_Renderer *r);
void font_free(void);
int font_width(const char *text);
void font_draw(SDL_Renderer *r, int x, int y, const char *text, uint32_t color);

#endif
