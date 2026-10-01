#include "app.h"
#include "input.h"
#include "render.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_W 1280
#define DEFAULT_H 720

static void usage(FILE *out)
{
    fprintf(out,
            "Usage: spint [options] [file]\n"
            "\n"
            "Opens file if it exists, otherwise starts a blank canvas that saves to file.\n"
            "\n"
            "Options:\n"
            "  -s, --size WxH   size of a new canvas (default %dx%d)\n"
            "  -h, --help       show this help\n"
            "  -v, --version    show the version\n",
            DEFAULT_W, DEFAULT_H);
}

static bool parse_size(const char *s, int *w, int *h)
{
    char *end;
    long x = strtol(s, &end, 10);
    if (end == s || (*end != 'x' && *end != 'X')) return false;
    const char *rest = end + 1;
    long y = strtol(rest, &end, 10);
    if (end == rest || *end) return false;
    if (x < 1 || y < 1 || x > CANVAS_MAX_SIDE || y > CANVAS_MAX_SIDE) return false;
    *w = (int)x;
    *h = (int)y;
    return true;
}

static void loop(App *a)
{
    while (a->running) {
        SDL_Event e;
        int timeout = -1;
        if (a->message[0]) {
            Sint32 left = (Sint32)(a->message_until - SDL_GetTicks());
            if (left <= 0) {
                a->message[0] = 0;
                a->redraw = true;
            } else {
                timeout = left;
            }
        }
        if (!a->redraw) {
            int got = timeout < 0 ? SDL_WaitEvent(&e) : SDL_WaitEventTimeout(&e, timeout);
            if (got) input_event(a, &e);
        }
        while (a->running && SDL_PollEvent(&e)) input_event(a, &e);
        if (a->running && a->redraw) render_frame(a);
    }
}

int main(int argc, char **argv)
{
    int w = DEFAULT_W, h = DEFAULT_H;
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(stdout);
            return 0;
        } else if (!strcmp(arg, "-v") || !strcmp(arg, "--version")) {
            printf("spint %s\n", SPINT_VERSION);
            return 0;
        } else if (!strcmp(arg, "-s") || !strcmp(arg, "--size")) {
            if (i + 1 >= argc || !parse_size(argv[++i], &w, &h)) {
                fprintf(stderr, "spint: --size expects WxH, for example 1920x1080 (max %d)\n", CANVAS_MAX_SIDE);
                return 2;
            }
        } else if (arg[0] == '-' && arg[1]) {
            fprintf(stderr, "spint: unknown option %s\n", arg);
            usage(stderr);
            return 2;
        } else if (path) {
            fprintf(stderr, "spint: only one file can be opened\n");
            return 2;
        } else {
            path = arg;
        }
    }

    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "spint: cannot initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    static App app;
    int status = 0;
    if (app_init(&app, w, h, path)) loop(&app);
    else status = 1;
    app_quit(&app);
    SDL_Quit();
    return status;
}
