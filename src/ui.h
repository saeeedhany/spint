#ifndef SPINT_UI_H
#define SPINT_UI_H

#include "app.h"

#include <SDL.h>
#include <stdbool.h>

#define UI_TOOLBAR_H 52
#define UI_STATUS_H 28
#define UI_MIN_W 880
#define UI_MIN_H 520

typedef enum {
    HIT_NONE,
    HIT_TOOL,
    HIT_FILLED,
    HIT_PRIMARY,
    HIT_SECONDARY,
    HIT_PALETTE,
    HIT_SLIDER,
    HIT_BAR,
    HIT_CANVAS
} HitKind;

typedef struct {
    HitKind kind;
    int index;
} Hit;

bool ui_init(SDL_Renderer *r);
void ui_free(void);
Hit ui_hit(int win_w, int win_h, int x, int y);
int ui_slider_value(int x);
const char *ui_tool_key(int tool);
void ui_draw(App *a);

#endif
