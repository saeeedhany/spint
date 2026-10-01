#include "io.h"
#include "canvas.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef enum { FMT_PNG, FMT_BMP, FMT_TGA, FMT_JPG, FMT_UNKNOWN } Format;

static bool ext_is(const char *ext, const char *want)
{
    while (*ext && *want) {
        if (tolower((unsigned char)*ext) != *want) return false;
        ext++;
        want++;
    }
    return *ext == 0 && *want == 0;
}

static Format format_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) return FMT_UNKNOWN;
    dot++;
    if (ext_is(dot, "png")) return FMT_PNG;
    if (ext_is(dot, "bmp")) return FMT_BMP;
    if (ext_is(dot, "tga")) return FMT_TGA;
    if (ext_is(dot, "jpg") || ext_is(dot, "jpeg")) return FMT_JPG;
    return FMT_UNKNOWN;
}

bool io_supported(const char *path)
{
    return format_of(path) != FMT_UNKNOWN;
}

uint32_t *io_load(const char *path, int *w, int *h, const char **error)
{
    int n;
    unsigned char *data = stbi_load(path, w, h, &n, 4);
    if (!data) {
        *error = stbi_failure_reason();
        return NULL;
    }
    if (*w > CANVAS_MAX_SIDE || *h > CANVAS_MAX_SIDE) {
        stbi_image_free(data);
        *error = "image is too large";
        return NULL;
    }
    size_t count = (size_t)*w * *h;
    uint32_t *px = malloc(count * sizeof *px);
    if (!px) {
        stbi_image_free(data);
        *error = "out of memory";
        return NULL;
    }
    const unsigned char *s = data;
    for (size_t i = 0; i < count; i++, s += 4) {
        unsigned a = s[3], ia = 255 - a;
        unsigned r = (s[0] * a + 255 * ia + 127) / 255;
        unsigned g = (s[1] * a + 255 * ia + 127) / 255;
        unsigned b = (s[2] * a + 255 * ia + 127) / 255;
        px[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
    stbi_image_free(data);
    return px;
}

bool io_pack(IoImage *img, const uint32_t *px, int w, int h)
{
    size_t count = (size_t)w * h;
    img->rgb = malloc(count * 3);
    if (!img->rgb) return false;
    img->w = w;
    img->h = h;
    uint8_t *d = img->rgb;
    for (size_t i = 0; i < count; i++, d += 3) {
        uint32_t p = px[i];
        d[0] = (uint8_t)(p >> 16);
        d[1] = (uint8_t)(p >> 8);
        d[2] = (uint8_t)p;
    }
    return true;
}

bool io_write(const char *path, const IoImage *img)
{
    switch (format_of(path)) {
    case FMT_BMP:
        return stbi_write_bmp(path, img->w, img->h, 3, img->rgb) != 0;
    case FMT_TGA:
        return stbi_write_tga(path, img->w, img->h, 3, img->rgb) != 0;
    case FMT_JPG:
        return stbi_write_jpg(path, img->w, img->h, 3, img->rgb, 92) != 0;
    default:
        stbi_write_png_compression_level = 5;
        return stbi_write_png(path, img->w, img->h, 3, img->rgb, img->w * 3) != 0;
    }
}

void io_release(IoImage *img)
{
    free(img->rgb);
    img->rgb = NULL;
}
