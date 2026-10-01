#ifndef SPINT_VIEW_H
#define SPINT_VIEW_H

#include <SDL.h>
#include <stdbool.h>

typedef struct {
    SDL_Rect area;
    float zoom;
    float ox, oy;
    int cw, ch;
} View;

void view_set_area(View *v, SDL_Rect area);
void view_set_canvas(View *v, int cw, int ch);
void view_fit(View *v);
void view_actual(View *v);
void view_zoom_at(View *v, float factor, float sx, float sy);
void view_pan(View *v, float dx, float dy);
void view_to_canvas(const View *v, float sx, float sy, float *cx, float *cy);
SDL_FRect view_canvas_rect(const View *v);
bool view_in_area(const View *v, int sx, int sy);

#endif
