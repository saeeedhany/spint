#ifndef SPINT_IO_H
#define SPINT_IO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int w, h;
    uint8_t *rgb;
} IoImage;

bool io_supported(const char *path);
uint32_t *io_load(const char *path, int *w, int *h, const char **error);
bool io_pack(IoImage *img, const uint32_t *px, int w, int h);
bool io_write(const char *path, const IoImage *img);
void io_release(IoImage *img);

#endif
