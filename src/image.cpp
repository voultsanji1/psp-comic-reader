#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_ONLY_TGA
#define STBI_ONLY_PNM
#include "libs/stb/stb_image.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "main.h"
#include "image.h"
#include "archive.h"
#include "display.h"

// ── Nearest-neighbour scale (fast, low RAM) ───────────────────────────────────
static u8* scaleImage(const u8* src, int sw, int sh,
                      int dw, int dh) {
    u8* dst = (u8*)malloc(dw * dh * 4);
    if (!dst) return NULL;
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            const u8* p = src + (sy * sw + sx) * 4;
            u8* q = dst + (y * dw + x) * 4;
            q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; q[3] = p[3];
        }
    }
    return dst;
}

// ── Rotate RGBA buffer 90° CW ─────────────────────────────────────────────────
static u8* rotateImage90(const u8* src, int w, int h) {
    u8* dst = (u8*)malloc(h * w * 4); // output is h×w
    if (!dst) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const u8* p = src + (y * w + x) * 4;
            u8* q = dst + (x * h + (h - 1 - y)) * 4;
            q[0]=p[0]; q[1]=p[1]; q[2]=p[2]; q[3]=p[3];
        }
    return dst;
}

// ── Public: load one page ─────────────────────────────────────────────────────
bool imageLoadPage(Archive* arc, int page, Image* img) {
    memset(img, 0, sizeof(Image));
    size_t data_size = 0;
    u8* data = archiveGetPageData(arc, page, &data_size);
    if (!data) return false;

    int w, h, ch;
    u8* raw = stbi_load_from_memory(data, (int)data_size, &w, &h, &ch, 4);
    free(data);
    if (!raw) return false;

    img->width  = w;
    img->height = h;
    img->pixels = raw;

    // Upload to VRAM
    img->tex_w = 1; while (img->tex_w < w) img->tex_w <<= 1;
    img->tex_h = 1; while (img->tex_h < h) img->tex_h <<= 1;

    // If image is larger than max texture size (512x512 on PSP), scale down
    // PSP GU supports up to 512×512 textures
    const int MAX_TEX = 512;
    if (img->tex_w > MAX_TEX || img->tex_h > MAX_TEX) {
        float sx = (float)MAX_TEX / w;
        float sy = (float)MAX_TEX / h;
        float s  = sx < sy ? sx : sy;
        int nw = (int)(w * s);
        int nh = (int)(h * s);
        u8* scaled = scaleImage(raw, w, h, nw, nh);
        stbi_image_free(raw);
        if (!scaled) return false;
        img->pixels = scaled;
        img->width  = nw;
        img->height = nh;
        img->tex_w  = 1; while (img->tex_w < nw) img->tex_w <<= 1;
        img->tex_h  = 1; while (img->tex_h < nh) img->tex_h <<= 1;
    }

    img->vram   = displayUploadTexture(img->pixels, img->width, img->height);
    img->loaded = true;
    return true;
}

void imageUnload(Image* img) {
    if (!img) return;
    if (img->pixels) {
        stbi_image_free(img->pixels);
        img->pixels = NULL;
    }
    // Note: VRAM is managed by a bump allocator; not individually freed
    memset(img, 0, sizeof(Image));
}

// ── Compute zoom and pan for a given fit mode ─────────────────────────────────
void imageFitToScreen(Image* img, float* zoom, int* pan_x, int* pan_y,
                      FitMode fit, Rotation rot) {
    if (!img || !img->loaded) return;

    int iw = (rot == ROT_90 || rot == ROT_270) ? img->height : img->width;
    int ih = (rot == ROT_90 || rot == ROT_270) ? img->width  : img->height;

    switch (fit) {
        case FIT_NONE:
            *zoom = 1.0f;
            break;
        case FIT_WIDTH:
            *zoom = (float)SCR_W / iw;
            break;
        case FIT_HEIGHT:
            *zoom = (float)SCR_H / ih;
            break;
        case FIT_SCREEN: {
            float zx = (float)SCR_W / iw;
            float zy = (float)SCR_H / ih;
            *zoom = zx < zy ? zx : zy;
            break;
        }
        case FIT_STRETCH:
            // Stretch handled at draw time
            *zoom = 1.0f;
            break;
    }

    *zoom = CLAMP(*zoom, 0.05f, 8.0f);

    // Centre the page
    int dw = (int)(iw * *zoom);
    int dh = (int)(ih * *zoom);
    *pan_x = (dw > SCR_W) ? (dw - SCR_W) / 2 : 0;
    *pan_y = (dh > SCR_H) ? (dh - SCR_H) / 2 : 0;
}
