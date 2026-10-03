#ifndef SPINT_HISTORY_H
#define SPINT_HISTORY_H

#include "canvas.h"

#include <stddef.h>

#define HISTORY_TILE 64

typedef struct {
    int index;
    uint32_t *data;
} HistoryTile;

typedef struct {
    HistoryTile *tiles;
    int count, cap;
    unsigned id;
} HistoryEntry;

typedef struct {
    HistoryEntry *items;
    int count, cap;
} HistoryStack;

typedef struct History {
    Canvas *canvas;
    int cols, rows;
    int *slot;
    HistoryEntry pending;
    bool active;
    bool failed;
    HistoryStack undo, redo;
    size_t bytes, limit;
    unsigned base_id;
    unsigned next_id;
} History;

bool history_init(History *h, Canvas *c, size_t limit);
bool history_reset(History *h);
void history_free(History *h);

void history_begin(History *h);
void history_save(History *h, int x0, int y0, int x1, int y1);
void history_revert(History *h);
void history_commit(History *h);
void history_cancel(History *h);

bool history_undo(History *h);
bool history_redo(History *h);
unsigned history_state(const History *h);

#endif
