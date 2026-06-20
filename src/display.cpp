#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <cstdlib>
#include <cstring>
#include "main.h"
#include "display.h"

// GU display list — aligned to 64 bytes for EDRAM
static unsigned int __attribute__((aligned(64))) s_displayList[256 * 1024];

// VRAM layout
#define VRAM_BASE       0x04000000
#define FRAME_SIZE      (SCR_W * SCR_H * SCR_BPP)
#define DRAW_BUFFER     ((void*)VRAM_BASE)
#define DISP_BUFFER     ((void*)(VRAM_BASE + FRAME_SIZE))
#define TEXTURE_MEMORY  ((void*)(VRAM_BASE + FRAME_SIZE * 2))

typedef struct {
    float u, v;
    float x, y, z;
} Vertex;

void displayInit() {
    sceGuInit();
    sceGuStart(GU_DIRECT, s_displayList);

    sceGuDrawBuffer(GU_PSM_8888, DRAW_BUFFER, SCR_W);
    sceGuDispBuffer(SCR_W, SCR_H, DISP_BUFFER, SCR_W);
    sceGuDepthBuffer((void*)(VRAM_BASE + FRAME_SIZE * 2), SCR_W);

    sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(0xc350, 0x2710);

    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void displayShutdown() {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void displayBegin() {
    sceGuStart(GU_DIRECT, s_displayList);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void displayEnd() {
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
}

// ── Draw a textured quad ──────────────────────────────────────────────────────
void displayDrawTexture(u32* tex, int tw, int th,
                        int dx, int dy, int dw, int dh) {
    sceGuTexImage(0, tw, th, tw, tex);
    sceGuTexScale(1.0f / tw, 1.0f / th);
    sceGuTexOffset(0.0f, 0.0f);

    // We tile in 128-px horizontal strips to stay within GU limits
    int x = dx;
    while (x < dx + dw) {
        int strip_w = MIN(128, dx + dw - x);
        float u0 = (float)(x - dx) / dw * tw;
        float u1 = (float)(x - dx + strip_w) / dw * tw;

        Vertex* v = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
        v[0].u = u0; v[0].v = 0;
        v[0].x = x;  v[0].y = dy;       v[0].z = 0;
        v[1].u = u1; v[1].v = (float)th;
        v[1].x = x + strip_w; v[1].y = dy + dh; v[1].z = 0;

        sceGumDrawArray(GU_SPRITES,
            GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            2, 0, v);
        x += strip_w;
    }
}

// ── Draw a filled rectangle ───────────────────────────────────────────────────
void displayFillRect(int x, int y, int w, int h, u32 color) {
    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    typedef struct { short x, y, z; } PV;
    PV* v = (PV*)sceGuGetMemory(2 * sizeof(PV));
    v[0].x = x;     v[0].y = y;     v[0].z = 0;
    v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
    sceGumDrawArray(GU_SPRITES,
        GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);

    sceGuEnable(GU_TEXTURE_2D);
}

// ── Draw a horizontal line ───────────────────────────────────────────────────
void displayHLine(int x, int y, int w, u32 color) {
    displayFillRect(x, y, w, 1, color);
}

// ── Upload pixel buffer to VRAM (returns VRAM pointer) ───────────────────────
u32* displayUploadTexture(u8* rgba, int w, int h) {
    // Next power-of-two dimensions required by GU
    int tw = 1; while (tw < w) tw <<= 1;
    int th = 1; while (th < h) th <<= 1;

    // Allocate from VRAM pool (simple bump allocator sufficient for our use)
    static u8* vram_ptr = (u8*)TEXTURE_MEMORY;
    u32* tex = (u32*)vram_ptr;
    vram_ptr += tw * th * 4;

    // Copy, padding rows
    for (int row = 0; row < h; row++) {
        memcpy(tex + row * tw, rgba + row * w * 4, w * 4);
        // Zero-fill pad columns
        if (tw > w) memset(tex + row * tw + w, 0, (tw - w) * 4);
    }
    // Zero-fill pad rows
    for (int row = h; row < th; row++)
        memset(tex + row * tw, 0, tw * 4);

    sceKernelDcacheWritebackRange(tex, tw * th * 4);
    return tex;
}
