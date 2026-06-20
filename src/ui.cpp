#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <psprtc.h>
#include <psppower.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include "main.h"
#include "ui.h"
#include "display.h"
#include "image.h"

// ── Colours ───────────────────────────────────────────────────────────────────
#define COL_BG        0xFF1A1A1A
#define COL_BAR       0xDD000000
#define COL_TEXT      0xFFFFFFFF
#define COL_TEXT_DIM  0xFF888888
#define COL_ACCENT    0xFF0099FF
#define COL_SELECT    0xFF003366
#define COL_DIR       0xFFFFCC44
#define COL_FILE      0xFFDDDDDD
#define COL_WARN      0xFFFF4444

// We use pspDebugScreenPrintf for text (no font loading needed — always works)
#define FONT_W 7
#define FONT_H 8

static void uiSetCursor(int x, int y) {
    pspDebugScreenSetXY(x / FONT_W, y / FONT_H);
}

static void uiPrint(int x, int y, u32 fg, const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    pspDebugScreenSetTextColor(fg);
    uiSetCursor(x, y);
    pspDebugScreenPuts(buf);
}

static void uiPrintCentered(int y, u32 fg, const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    int len = strlen(buf);
    int x = (SCR_W - len * FONT_W) / 2;
    if (x < 0) x = 0;
    uiPrint(x, y, fg, "%s", buf);
}

// ── Status bar (top) ──────────────────────────────────────────────────────────
static void uiDrawStatusBar(AppState* state, const char* title) {
    displayFillRect(0, 0, SCR_W, 16, COL_BAR);
    displayHLine(0, 16, SCR_W, COL_ACCENT);

    // Title (left)
    uiPrint(4, 4, COL_TEXT, "%s", title);

    // Clock (right)
    if (state->settings.show_clock) {
        pspTime t;
        sceRtcGetCurrentClockLocalTime(&t);
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d", t.hour, t.minutes);
        uiPrint(SCR_W - 36, 4, COL_TEXT_DIM, "%s", tbuf);
    }

    // Battery
    if (state->settings.show_battery) {
        int bat = scePowerGetBatteryLifePercent();
        u32 bc = bat > 30 ? COL_TEXT_DIM : COL_WARN;
        uiPrint(SCR_W - 70, 4, bc, "%3d%%", bat);
    }
}

// ── Bottom info bar ───────────────────────────────────────────────────────────
static void uiDrawInfoBar(AppState* state) {
    displayFillRect(0, SCR_H - 14, SCR_W, 14, COL_BAR);
    displayHLine(0, SCR_H - 14, SCR_W, COL_ACCENT);

    const char* fname = strrchr(state->arc.path, '/');
    fname = fname ? fname + 1 : state->arc.path;

    const char* fit_names[] = {"1:1","Wid","Hgt","Fit","Str"};
    uiPrint(4, SCR_H - 12, COL_TEXT,
            "%d/%d  %.0f%%  [%s]  %s",
            state->page + 1, state->total_pages,
            state->zoom * 100,
            fit_names[state->fit_mode],
            fname);
}

// ── File Browser ──────────────────────────────────────────────────────────────
void uiDrawBrowser(AppState* state) {
    Browser* b = &state->browser;

    displayFillRect(0, 0, SCR_W, SCR_H, COL_BG);
    uiDrawStatusBar(state, "PSP Comic Reader");
    displayHLine(0, SCR_H - 28, SCR_W, COL_ACCENT);

    // Path
    uiPrint(4, 18, COL_TEXT_DIM, "%s", b->cwd);
    displayHLine(0, 28, SCR_W, 0xFF333333);

    // Adjust items per page
    b->items_per_page = (SCR_H - 50) / (FONT_H + 2);

    int y = 32;
    for (int i = b->scroll_offset;
         i < b->count && i < b->scroll_offset + b->items_per_page; i++) {
        bool sel = (i == b->cursor);
        if (sel) displayFillRect(0, y - 1, SCR_W, FONT_H + 2, COL_SELECT);

        bool is_dir = (b->item_types[i] == 0);
        u32  col    = is_dir ? COL_DIR : COL_FILE;
        char prefix = is_dir ? '/' : ' ';

        uiPrint(8, y, col, "%c%s", prefix, b->items[i]);
        y += FONT_H + 2;
    }

    // Scroll indicator
    if (b->count > b->items_per_page) {
        int bar_h = (int)((float)b->items_per_page / b->count * (SCR_H - 50));
        int bar_y = 32 + (int)((float)b->scroll_offset / b->count * (SCR_H - 50));
        displayFillRect(SCR_W - 4, 32, 4, SCR_H - 50, 0xFF333333);
        displayFillRect(SCR_W - 4, bar_y, 4, bar_h, COL_ACCENT);
    }

    // Bottom hints
    displayFillRect(0, SCR_H - 28, SCR_W, 28, COL_BAR);
    uiPrint(4, SCR_H - 24, COL_TEXT_DIM,
            "X:Open  O:Up  T:Menu  Sel:Bookmarks");

    if (b->count == 0)
        uiPrintCentered(SCR_H / 2, COL_TEXT_DIM, "(empty — no comics found)");

    // Status message
    if (state->status_timer > 0)
        uiPrint(4, SCR_H - 14, COL_ACCENT, "%s", state->status_msg);
}

// ── Comic Reader ──────────────────────────────────────────────────────────────
void uiDrawReader(AppState* state) {
    if (!state->img.loaded) {
        displayFillRect(0, 0, SCR_W, SCR_H, 0xFF000000);
        uiPrintCentered(SCR_H / 2, COL_TEXT, "Loading...");
        return;
    }

    // Background
    displayFillRect(0, 0, SCR_W, SCR_H, state->settings.bg_color);

    // Compute draw rect
    int iw = state->img.width;
    int ih = state->img.height;
    int dw, dh;

    if (state->fit_mode == FIT_STRETCH) {
        dw = SCR_W; dh = SCR_H;
    } else {
        dw = (int)(iw * state->zoom);
        dh = (int)(ih * state->zoom);
    }

    int dx = -state->pan_x;
    int dy = -state->pan_y;

    // Centre if smaller than screen
    if (dw < SCR_W) dx = (SCR_W - dw) / 2;
    if (dh < SCR_H) dy = (SCR_H - dh) / 2;

    // Clamp to screen bounds for draw call
    int clip_x = MAX(0, dx);
    int clip_y = MAX(0, dy);
    int clip_w = MIN(dw, SCR_W - clip_x);
    int clip_h = MIN(dh, SCR_H - clip_y);

    if (clip_w > 0 && clip_h > 0)
        displayDrawTexture(state->img.vram, state->img.tex_w, state->img.tex_h,
                           clip_x, clip_y, clip_w, clip_h);

    // Info bar overlay
    if (state->show_info) {
        uiDrawStatusBar(state, "Reader");
        uiDrawInfoBar(state);
    }

    // Status flash
    if (state->status_timer > 0) {
        int alpha = (state->status_timer > 20) ? 0xFF :
                    (int)(0xFF * state->status_timer / 20.0f);
        u32 col = (alpha << 24) | 0x0099FF;
        uiPrint(4, SCR_H - 26, col, "%s", state->status_msg);
    }

    // Manga mode indicator
    if (state->manga_mode) {
        uiPrint(SCR_W - 20, SCR_H - 26, COL_ACCENT, "RL");
    }
}

// ── Menu ──────────────────────────────────────────────────────────────────────
static const char* MENU_ITEMS[] = {
    "  Resume Reading",
    "  Library (File Browser)",
    "  Bookmarks",
    "  Settings",
    "  About",
    "  Quit"
};
#define MENU_COUNT 6

void uiDrawMenu(AppState* state) {
    // Semi-transparent overlay
    displayFillRect(80, 40, 320, 200, 0xEE111111);
    displayFillRect(80, 40, 320, 2, COL_ACCENT);
    displayFillRect(80, 238, 320, 2, COL_ACCENT);
    displayFillRect(80, 40, 2, 200, COL_ACCENT);
    displayFillRect(398, 40, 2, 200, COL_ACCENT);

    uiPrintCentered(48, COL_ACCENT, "MENU");
    displayHLine(80, 58, 320, 0xFF333333);

    for (int i = 0; i < MENU_COUNT; i++) {
        int y = 64 + i * 22;
        bool sel = (i == state->menu_cursor);
        if (sel) displayFillRect(82, y - 2, 316, 18, COL_SELECT);
        uiPrint(96, y, sel ? COL_TEXT : COL_TEXT_DIM, "%s", MENU_ITEMS[i]);
    }

    uiPrint(96, 230, COL_TEXT_DIM, "X:Select  O:Back");
}

// ── Settings ──────────────────────────────────────────────────────────────────
static const char* FIT_NAMES[] = {"Original","Fit Width","Fit Height","Fit Screen","Stretch"};
static const char* YN[] = {"No","Yes"};

void uiDrawSettings(AppState* state) {
    Settings* s = &state->settings;
    displayFillRect(0, 0, SCR_W, SCR_H, COL_BG);
    uiDrawStatusBar(state, "Settings");
    displayHLine(0, 28, SCR_W, 0xFF333333);

    const int Y0 = 32, DY = 20;
    int y = Y0;

#define SROW(idx, label, value_fmt, ...) { \
    bool sel = (state->settings_cursor == idx); \
    if (sel) displayFillRect(0, y-1, SCR_W, DY-2, COL_SELECT); \
    uiPrint(8,  y, sel ? COL_ACCENT : COL_TEXT, label); \
    uiPrint(220, y, sel ? COL_TEXT : COL_TEXT_DIM, value_fmt, ##__VA_ARGS__); \
    y += DY; \
}

    SROW(0, "Default Zoom Mode",    "%s",   FIT_NAMES[s->default_fit]);
    SROW(1, "Manga Mode (RTL)",     "%s",   YN[s->manga_mode]);
    SROW(2, "Show Clock",           "%s",   YN[s->show_clock]);
    SROW(3, "Show Battery",         "%s",   YN[s->show_battery]);
    SROW(4, "Pan Speed",            "%d",   s->pan_speed);
    SROW(5, "Smooth Scroll",        "%s",   YN[s->smooth_scroll]);
    SROW(6, "Preload Next Page",    "%s",   YN[s->preload_next]);
    SROW(7, "Remember Position",    "%s",   YN[s->remember_position]);
    SROW(8, "CPU Speed",            "%dMHz",s->cpu_speed);
    SROW(9, "Zoom Step",            "%.0f%%",s->zoom_step * 100);

#undef SROW

    displayFillRect(0, SCR_H - 18, SCR_W, 18, COL_BAR);
    uiPrint(4, SCR_H - 14, COL_TEXT_DIM,
            "Left/Right: Change  O/T: Save & Back");
}

// ── Bookmarks ─────────────────────────────────────────────────────────────────
void uiDrawBookmarks(AppState* state) {
    displayFillRect(0, 0, SCR_W, SCR_H, COL_BG);
    uiDrawStatusBar(state, "Bookmarks");
    displayHLine(0, 28, SCR_W, 0xFF333333);

    if (state->bookmark_count == 0) {
        uiPrintCentered(SCR_H / 2 - 8, COL_TEXT_DIM, "(no bookmarks yet)");
        uiPrintCentered(SCR_H / 2 + 8, COL_TEXT_DIM, "Press X while reading to add one");
    } else {
        int items_per_page = (SCR_H - 50) / 22;
        int scroll = 0;
        if (state->bookmark_cursor >= scroll + items_per_page)
            scroll = state->bookmark_cursor - items_per_page + 1;

        int y = 32;
        for (int i = scroll;
             i < state->bookmark_count && i < scroll + items_per_page; i++) {
            Bookmark* bm = &state->bookmarks[i];
            if (!strcmp(bm->label, "lastpos")) continue; // skip internal
            bool sel = (i == state->bookmark_cursor);
            if (sel) displayFillRect(0, y-1, SCR_W, 20, COL_SELECT);

            const char* fname = strrchr(bm->file, '/');
            fname = fname ? fname + 1 : bm->file;
            uiPrint(8,  y,   sel ? COL_ACCENT : COL_FILE, "%s", fname);
            uiPrint(8,  y+10, sel ? COL_TEXT : COL_TEXT_DIM, "Page %d", bm->page + 1);
            y += 22;
        }
    }

    displayFillRect(0, SCR_H - 18, SCR_W, 18, COL_BAR);
    uiPrint(4, SCR_H - 14, COL_TEXT_DIM,
            "X:Jump  Sq:Delete  O:Back");
}

// ── About ─────────────────────────────────────────────────────────────────────
void uiDrawAbout(AppState* state) {
    displayFillRect(0, 0, SCR_W, SCR_H, COL_BG);
    uiDrawStatusBar(state, "About");

    int y = 30;
    uiPrintCentered(y,      COL_ACCENT,   "PSP Comic Reader v1.0"); y += 18;
    uiPrintCentered(y,      COL_TEXT_DIM, "A lightweight comic reader for PSP"); y += 28;

    uiPrint(16, y, COL_TEXT, "Supported formats:"); y += 14;
    uiPrint(16, y, COL_FILE, "CBZ / ZIP   CBR / RAR"); y += 12;
    uiPrint(16, y, COL_FILE, "CB7 / 7Z    CBT / TAR"); y += 12;
    uiPrint(16, y, COL_FILE, "Image folders  (JPG PNG BMP GIF)"); y += 20;

    uiPrint(16, y, COL_TEXT, "Controls:"); y += 14;
    uiPrint(16, y, COL_FILE, "L/R Trigger : Prev / Next page"); y += 12;
    uiPrint(16, y, COL_FILE, "D-Pad/Analog: Pan  Square: Zoom"); y += 12;
    uiPrint(16, y, COL_FILE, "Triangle: Menu  L+R: Fit screen"); y += 12;
    uiPrint(16, y, COL_FILE, "Cross: Bookmark  Select: List"); y += 20;

    uiPrint(16, y, COL_TEXT, "Libraries:"); y += 14;
    uiPrint(16, y, COL_TEXT_DIM, "miniz (ZIP)  stb_image (decode)"); y += 12;
    uiPrint(16, y, COL_TEXT_DIM, "PSPSDK  pspgu"); y += 20;

    uiPrintCentered(y, COL_TEXT_DIM, "O / X to go back");
}
