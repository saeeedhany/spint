#include "../src/canvas.h"
#include "../src/draw.h"
#include "../src/history.h"
#include "../src/io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WHITE 0xFFFFFFFFu
#define BLACK 0xFF000000u
#define RED 0xFFFF0000u

static int failures;
static int checks;

#define CHECK(cond)                                                       \
    do {                                                                  \
        checks++;                                                         \
        if (!(cond)) {                                                    \
            failures++;                                                   \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

static int count_color(const Canvas *c, uint32_t color)
{
    int n = 0;
    for (size_t i = 0; i < (size_t)c->w * c->h; i++) n += c->px[i] == color;
    return n;
}

static void bounds_of(const Canvas *c, uint32_t color, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = c->w;
    *y0 = c->h;
    *x1 = -1;
    *y1 = -1;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++)
            if (canvas_get(c, x, y) == color) {
                if (x < *x0) *x0 = x;
                if (y < *y0) *y0 = y;
                if (x > *x1) *x1 = x;
                if (y > *y1) *y1 = y;
            }
}

static void test_brush_sizes(void)
{
    for (int size = 1; size <= 40; size++) {
        Canvas c = { 0 };
        CHECK(canvas_init(&c, 100, 100, WHITE));
        draw_capsule(&c, 50.3f, 50.7f, 50.3f, 50.7f, size, BLACK);
        int x0, y0, x1, y1;
        bounds_of(&c, BLACK, &x0, &y0, &x1, &y1);
        CHECK(x1 - x0 + 1 == size);
        CHECK(y1 - y0 + 1 == size);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                CHECK(canvas_get(&c, x, y) == canvas_get(&c, x0 + x1 - x, y) &&
                      canvas_get(&c, x, y) == canvas_get(&c, x, y0 + y1 - y));
        canvas_free(&c);
    }
}

static void test_stroke_is_continuous(void)
{
    Canvas c = { 0 };
    CHECK(canvas_init(&c, 200, 200, WHITE));
    draw_capsule(&c, 10.5f, 10.5f, 190.5f, 150.5f, 1, BLACK);
    for (int y = 10; y <= 150; y++) {
        int found = 0;
        for (int x = 0; x < 200; x++) found |= canvas_get(&c, x, y) == BLACK;
        CHECK(found);
    }
    for (int x = 10; x <= 190; x++) {
        int found = 0;
        for (int y = 0; y < 200; y++) found |= canvas_get(&c, x, y) == BLACK;
        CHECK(found);
    }
    draw_capsule(&c, -50.0f, 100.0f, 400.0f, 100.0f, 9, RED);
    CHECK(canvas_get(&c, 0, 100) == RED);
    CHECK(canvas_get(&c, 199, 100) == RED);
    canvas_free(&c);
}

static void test_rect_and_ellipse(void)
{
    Canvas c = { 0 };
    CHECK(canvas_init(&c, 100, 100, WHITE));
    draw_rect(&c, 80, 70, 10, 20, 3, false, BLACK);
    int x0, y0, x1, y1;
    bounds_of(&c, BLACK, &x0, &y0, &x1, &y1);
    CHECK(x0 == 10 && y0 == 20 && x1 == 80 && y1 == 70);
    CHECK(canvas_get(&c, 45, 45) == WHITE);
    CHECK(canvas_get(&c, 12, 45) == BLACK);
    CHECK(canvas_get(&c, 13, 45) == WHITE);
    CHECK(count_color(&c, BLACK) == 71 * 51 - 65 * 45);
    canvas_fill(&c, WHITE);
    draw_ellipse(&c, 10, 20, 89, 79, 2, false, BLACK);
    bounds_of(&c, BLACK, &x0, &y0, &x1, &y1);
    CHECK(x0 == 10 && y0 == 20 && x1 == 89 && y1 == 79);
    CHECK(canvas_get(&c, 50, 50) == WHITE);
    draw_flood(&c, 50, 50, RED);
    CHECK(canvas_get(&c, 0, 0) == WHITE);
    CHECK(canvas_get(&c, 50, 22) == RED);
    canvas_fill(&c, WHITE);
    draw_ellipse(&c, 10, 20, 89, 79, 1, true, BLACK);
    CHECK(canvas_get(&c, 50, 50) == BLACK);
    canvas_free(&c);
}

static void test_flood(void)
{
    Canvas c = { 0 };
    CHECK(canvas_init(&c, 64, 64, WHITE));
    draw_rect(&c, 10, 10, 30, 30, 1, false, BLACK);
    CHECK(draw_flood(&c, 20, 20, RED));
    CHECK(count_color(&c, RED) == 19 * 19);
    CHECK(canvas_get(&c, 5, 5) == WHITE);
    CHECK(!draw_flood(&c, 20, 20, RED));
    CHECK(draw_flood(&c, 0, 0, RED));
    CHECK(count_color(&c, RED) == 64 * 64 - 80);
    canvas_fill(&c, WHITE);
    for (int x = 2; x < 62; x += 2) {
        if ((x / 2) & 1) draw_rect(&c, x, 0, x, 62, 1, true, BLACK);
        else draw_rect(&c, x, 1, x, 63, 1, true, BLACK);
    }
    CHECK(draw_flood(&c, 0, 0, RED));
    CHECK(canvas_get(&c, 63, 63) == RED);
    CHECK(count_color(&c, WHITE) == 0);
    canvas_free(&c);
}

static bool same(const uint32_t *a, const uint32_t *b, size_t n)
{
    return memcmp(a, b, n * sizeof *a) == 0;
}

static void test_history(void)
{
    Canvas c = { 0 };
    History h;
    CHECK(canvas_init(&c, 300, 200, WHITE));
    CHECK(history_init(&h, &c, 64u << 20));
    size_t n = (size_t)c.w * c.h;
    uint32_t *s0 = malloc(n * 4), *s1 = malloc(n * 4), *s2 = malloc(n * 4);
    memcpy(s0, c.px, n * 4);

    CHECK(history_state(&h) == 0);
    history_begin(&h);
    draw_capsule(&c, 5, 5, 290, 190, 12, BLACK);
    history_commit(&h);
    memcpy(s1, c.px, n * 4);
    unsigned id1 = history_state(&h);
    CHECK(id1 != 0);

    history_begin(&h);
    CHECK(draw_flood(&c, 290, 5, RED));
    history_commit(&h);
    memcpy(s2, c.px, n * 4);

    CHECK(history_undo(&h));
    CHECK(same(c.px, s1, n));
    CHECK(history_state(&h) == id1);
    CHECK(history_undo(&h));
    CHECK(same(c.px, s0, n));
    CHECK(!history_undo(&h));
    CHECK(history_redo(&h));
    CHECK(same(c.px, s1, n));
    CHECK(history_redo(&h));
    CHECK(same(c.px, s2, n));
    CHECK(!history_redo(&h));

    history_begin(&h);
    draw_rect(&c, 10, 10, 100, 100, 4, false, BLACK);
    history_revert(&h);
    CHECK(same(c.px, s2, n));
    draw_rect(&c, 20, 20, 50, 50, 4, false, BLACK);
    history_cancel(&h);
    CHECK(same(c.px, s2, n));

    CHECK(history_undo(&h));
    history_begin(&h);
    draw_capsule(&c, 0, 0, 0, 0, 3, BLACK);
    history_commit(&h);
    CHECK(!history_redo(&h));

    history_begin(&h);
    history_commit(&h);
    CHECK(h.undo.count == 2);

    free(s0);
    free(s1);
    free(s2);
    history_free(&h);
    canvas_free(&c);
}

static void test_history_limit(void)
{
    Canvas c = { 0 };
    History h;
    CHECK(canvas_init(&c, 640, 640, WHITE));
    CHECK(history_init(&h, &c, 4u << 20));
    for (int i = 0; i < 20; i++) {
        history_begin(&h);
        canvas_fill(&c, i & 1 ? BLACK : WHITE);
        history_commit(&h);
    }
    CHECK(h.bytes <= h.limit);
    CHECK(h.undo.count >= 1);
    while (history_undo(&h)) {
    }
    CHECK(h.undo.count == 0);
    history_free(&h);
    canvas_free(&c);
}

static void test_io(void)
{
    Canvas c = { 0 };
    CHECK(canvas_init(&c, 37, 23, WHITE));
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) c.px[y * c.w + x] = rgb((uint8_t)(x * 7), (uint8_t)(y * 11), (uint8_t)(x ^ y));
    const char *paths[] = { "build/test_io.png", "build/test_io.bmp", "build/test_io.tga" };
    for (int i = 0; i < 3; i++) {
        IoImage img;
        CHECK(io_pack(&img, c.px, c.w, c.h));
        CHECK(io_write(paths[i], &img));
        io_release(&img);
        int w = 0, h = 0;
        const char *err = NULL;
        uint32_t *px = io_load(paths[i], &w, &h, &err);
        CHECK(px != NULL);
        if (px) {
            CHECK(w == c.w && h == c.h);
            CHECK(same(px, c.px, (size_t)w * h));
            free(px);
        }
        remove(paths[i]);
    }
    CHECK(io_supported("a.PNG"));
    CHECK(io_supported("dir.v2/a.jpeg"));
    CHECK(!io_supported("dir.v2/name"));
    const char *err = NULL;
    int w, h;
    CHECK(io_load("build/missing.png", &w, &h, &err) == NULL && err != NULL);
    canvas_free(&c);
}

static double now(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void bench(void)
{
    Canvas c = { 0 };
    History h;
    CHECK(canvas_init(&c, 3840, 2160, WHITE));
    CHECK(history_init(&h, &c, 512u << 20));
    double t = now();
    history_begin(&h);
    for (int i = 0; i < 10000; i++) {
        float x = (float)(i * 37 % 3800), y = (float)(i * 53 % 2100);
        draw_capsule(&c, x, y, x + 25, y + 18, 50, BLACK);
    }
    history_commit(&h);
    printf("10000 stroke segments, size 50, 4K canvas: %.1f ms\n", (now() - t) * 1e3);
    canvas_fill(&c, WHITE);
    t = now();
    history_begin(&h);
    draw_flood(&c, 0, 0, RED);
    history_commit(&h);
    printf("flood fill of a full 4K canvas: %.1f ms\n", (now() - t) * 1e3);
    t = now();
    history_undo(&h);
    printf("undo of a full 4K fill: %.1f ms\n", (now() - t) * 1e3);
    history_free(&h);
    canvas_free(&c);
}

int main(int argc, char **argv)
{
    test_brush_sizes();
    test_stroke_is_continuous();
    test_rect_and_ellipse();
    test_flood();
    test_history();
    test_history_limit();
    test_io();
    if (argc > 1 && strcmp(argv[1], "--bench") == 0) bench();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
