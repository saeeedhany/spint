#include "history.h"

#include <stdlib.h>
#include <string.h>

#define TILE HISTORY_TILE
#define TILE_BYTES ((size_t)TILE * TILE * sizeof(uint32_t))

static void tile_rect(const History *h, int index, int *x, int *y, int *w, int *th)
{
    *x = (index % h->cols) * TILE;
    *y = (index / h->cols) * TILE;
    *w = h->canvas->w - *x < TILE ? h->canvas->w - *x : TILE;
    *th = h->canvas->h - *y < TILE ? h->canvas->h - *y : TILE;
}

static void tile_load(const History *h, const HistoryTile *t)
{
    int x, y, w, th;
    tile_rect(h, t->index, &x, &y, &w, &th);
    Canvas *c = h->canvas;
    for (int r = 0; r < th; r++)
        memcpy(c->px + (size_t)(y + r) * c->w + x, t->data + (size_t)r * TILE, (size_t)w * sizeof(uint32_t));
    canvas_mark(c, x, y, x + w - 1, y + th - 1);
}

static void tile_store(const History *h, HistoryTile *t)
{
    int x, y, w, th;
    tile_rect(h, t->index, &x, &y, &w, &th);
    const Canvas *c = h->canvas;
    for (int r = 0; r < th; r++)
        memcpy(t->data + (size_t)r * TILE, c->px + (size_t)(y + r) * c->w + x, (size_t)w * sizeof(uint32_t));
}

static void tile_swap(const History *h, HistoryTile *t)
{
    int x, y, w, th;
    tile_rect(h, t->index, &x, &y, &w, &th);
    Canvas *c = h->canvas;
    uint32_t tmp[TILE];
    for (int r = 0; r < th; r++) {
        uint32_t *row = c->px + (size_t)(y + r) * c->w + x;
        uint32_t *saved = t->data + (size_t)r * TILE;
        size_t n = (size_t)w * sizeof(uint32_t);
        memcpy(tmp, row, n);
        memcpy(row, saved, n);
        memcpy(saved, tmp, n);
    }
    canvas_mark(c, x, y, x + w - 1, y + th - 1);
}

static size_t entry_bytes(const HistoryEntry *e)
{
    return (size_t)e->count * TILE_BYTES;
}

static void entry_free(HistoryEntry *e)
{
    for (int i = 0; i < e->count; i++) free(e->tiles[i].data);
    free(e->tiles);
    memset(e, 0, sizeof *e);
}

static bool stack_push(HistoryStack *s, HistoryEntry e)
{
    if (s->count == s->cap) {
        int cap = s->cap ? s->cap * 2 : 64;
        HistoryEntry *items = realloc(s->items, (size_t)cap * sizeof *items);
        if (!items) return false;
        s->items = items;
        s->cap = cap;
    }
    s->items[s->count++] = e;
    return true;
}

static void stack_clear(History *h, HistoryStack *s)
{
    for (int i = 0; i < s->count; i++) {
        h->bytes -= entry_bytes(&s->items[i]);
        entry_free(&s->items[i]);
    }
    s->count = 0;
}

static bool drop_oldest(History *h)
{
    HistoryStack *s = &h->undo;
    if (s->count == 0) return false;
    h->bytes -= entry_bytes(&s->items[0]);
    entry_free(&s->items[0]);
    memmove(s->items, s->items + 1, (size_t)(s->count - 1) * sizeof *s->items);
    s->count--;
    return true;
}

static void release_pending(History *h)
{
    for (int i = 0; i < h->pending.count; i++) h->slot[h->pending.tiles[i].index] = -1;
    entry_free(&h->pending);
    h->active = false;
    h->failed = false;
}

bool history_init(History *h, Canvas *c, size_t limit)
{
    memset(h, 0, sizeof *h);
    h->canvas = c;
    h->limit = limit;
    h->next_id = 1;
    c->hist = h;
    return history_reset(h);
}

bool history_reset(History *h)
{
    if (h->active) release_pending(h);
    stack_clear(h, &h->undo);
    stack_clear(h, &h->redo);
    h->bytes = 0;
    free(h->slot);
    h->cols = (h->canvas->w + TILE - 1) / TILE;
    h->rows = (h->canvas->h + TILE - 1) / TILE;
    size_t n = (size_t)h->cols * h->rows;
    h->slot = malloc(n * sizeof *h->slot);
    if (!h->slot) return false;
    for (size_t i = 0; i < n; i++) h->slot[i] = -1;
    return true;
}

void history_free(History *h)
{
    if (h->active) release_pending(h);
    stack_clear(h, &h->undo);
    stack_clear(h, &h->redo);
    free(h->undo.items);
    free(h->redo.items);
    free(h->slot);
    if (h->canvas) h->canvas->hist = NULL;
    memset(h, 0, sizeof *h);
}

void history_begin(History *h)
{
    if (h->active) history_commit(h);
    h->active = true;
    h->failed = false;
}

static bool save_tile(History *h, int index)
{
    HistoryEntry *e = &h->pending;
    if (e->count == e->cap) {
        int cap = e->cap ? e->cap * 2 : 16;
        HistoryTile *tiles = realloc(e->tiles, (size_t)cap * sizeof *tiles);
        if (!tiles) return false;
        e->tiles = tiles;
        e->cap = cap;
    }
    uint32_t *data = malloc(TILE_BYTES);
    while (!data && drop_oldest(h)) data = malloc(TILE_BYTES);
    if (!data) return false;
    HistoryTile *t = &e->tiles[e->count];
    t->index = index;
    t->data = data;
    tile_store(h, t);
    h->slot[index] = e->count++;
    return true;
}

void history_save(History *h, int x0, int y0, int x1, int y1)
{
    if (!h->active || h->failed) return;
    int tx0 = x0 / TILE, tx1 = x1 / TILE;
    int ty0 = y0 / TILE, ty1 = y1 / TILE;
    for (int ty = ty0; ty <= ty1; ty++) {
        int base = ty * h->cols;
        for (int tx = tx0; tx <= tx1; tx++) {
            if (h->slot[base + tx] >= 0) continue;
            if (!save_tile(h, base + tx)) {
                h->failed = true;
                return;
            }
        }
    }
}

void history_revert(History *h)
{
    if (!h->active) return;
    for (int i = 0; i < h->pending.count; i++) tile_load(h, &h->pending.tiles[i]);
}

void history_commit(History *h)
{
    if (!h->active) return;
    if (h->failed) {
        release_pending(h);
        stack_clear(h, &h->undo);
        stack_clear(h, &h->redo);
        h->next_id++;
        return;
    }
    if (h->pending.count == 0) {
        release_pending(h);
        return;
    }
    for (int i = 0; i < h->pending.count; i++) h->slot[h->pending.tiles[i].index] = -1;
    HistoryEntry e = h->pending;
    e.id = h->next_id++;
    memset(&h->pending, 0, sizeof h->pending);
    h->active = false;
    stack_clear(h, &h->redo);
    if (!stack_push(&h->undo, e)) {
        entry_free(&e);
        return;
    }
    h->bytes += entry_bytes(&e);
    while (h->bytes > h->limit && h->undo.count > 1) drop_oldest(h);
}

void history_cancel(History *h)
{
    if (!h->active) return;
    history_revert(h);
    release_pending(h);
}

static bool move_top(History *h, HistoryStack *from, HistoryStack *to)
{
    if (h->active || from->count == 0) return false;
    HistoryEntry e = from->items[from->count - 1];
    if (!stack_push(to, e)) return false;
    from->count--;
    for (int i = 0; i < e.count; i++) tile_swap(h, &e.tiles[i]);
    return true;
}

bool history_undo(History *h)
{
    return move_top(h, &h->undo, &h->redo);
}

bool history_redo(History *h)
{
    return move_top(h, &h->redo, &h->undo);
}

unsigned history_state(const History *h)
{
    return h->undo.count ? h->undo.items[h->undo.count - 1].id : 0;
}
