#ifndef SPINT_APP_H
#define SPINT_APP_H

#include "canvas.h"
#include "history.h"
#include "view.h"

#include <SDL.h>

#define SPINT_VERSION "1.0.0"
#define PATH_CAP 4096
#define BRUSH_MIN 1
#define BRUSH_MAX 200
#define PALETTE_COUNT 20

typedef enum {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_FILL,
    TOOL_PICKER,
    TOOL_COUNT
} Tool;

typedef enum {
    DRAG_NONE,
    DRAG_PAINT,
    DRAG_PAN,
    DRAG_SLIDER
} Drag;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int max_texture;
    int win_w, win_h;

    Canvas canvas;
    History history;
    View view;

    Tool tool;
    Tool return_tool;
    bool filled;
    int size;
    uint32_t primary, secondary;
    uint32_t palette[PALETTE_COUNT];

    Drag drag;
    uint32_t paint_color;
    Uint8 drag_button;
    float anchor_x, anchor_y;
    float last_x, last_y;
    float cur_x, cur_y;
    bool has_last;
    float pan_x, pan_y;
    bool space_held;
    bool shift_held;

    int mouse_x, mouse_y;
    bool mouse_inside;

    char path[PATH_CAP];
    unsigned saved_state;
    unsigned document;
    bool saving;
    SDL_Thread *save_thread;
    Uint32 save_event;

    char message[256];
    Uint32 message_until;
    Uint32 quit_armed_until;
    char drop_path[PATH_CAP];
    Uint32 drop_armed_until;

    SDL_Cursor *cursor_arrow, *cursor_cross, *cursor_move, *cursor_hand;
    SDL_Cursor *cursor_current;

    bool show_help;
    bool running;
    bool redraw;
} App;

bool app_init(App *a, int cw, int ch, const char *path);
void app_quit(App *a);
void app_layout(App *a);
bool app_modified(const App *a);
void app_update_title(App *a);
void app_message(App *a, const char *fmt, ...);
void app_set_tool(App *a, Tool t);
void app_set_size(App *a, int size);
bool app_open(App *a, const char *path);
void app_save(App *a, bool new_name);
void app_save_done(App *a, SDL_Event *e);
void app_clear(App *a);
void app_undo(App *a);
void app_redo(App *a);
void app_request_quit(App *a);
const char *app_tool_name(Tool t);

#endif
