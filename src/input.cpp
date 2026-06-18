#include <pspctrl.h>
#include <stdlib.h>
#include <math.h>
#include "main.h"
#include "input.h"
#include "archive.h"
#include "image.h"
#include "browser.h"
#include "bookmarks.h"

static SceCtrlData s_pad_prev;
static SceCtrlData s_pad_curr;

// How many frames a button must be held before repeating
#define REPEAT_DELAY  30
#define REPEAT_RATE   6

static int s_hold_frames[32];

void inputInit() {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    memset(&s_pad_prev, 0, sizeof(s_pad_prev));
    memset(&s_pad_curr, 0, sizeof(s_pad_curr));
    memset(s_hold_frames, 0, sizeof(s_hold_frames));
}

void inputUpdate() {
    s_pad_prev = s_pad_curr;
    sceCtrlReadBufferPositive(&s_pad_curr, 1);
}

bool inputPressed(u32 btn) {
    return (s_pad_curr.Buttons & btn) && !(s_pad_prev.Buttons & btn);
}

bool inputHeld(u32 btn) {
    return (s_pad_curr.Buttons & btn) != 0;
}

bool inputReleased(u32 btn) {
    return !(s_pad_curr.Buttons & btn) && (s_pad_prev.Buttons & btn);
}

// Returns true on first press and after repeat threshold
bool inputRepeat(u32 btn, int btn_idx) {
    if (!(s_pad_curr.Buttons & btn)) {
        s_hold_frames[btn_idx] = 0;
        return false;
    }
    s_hold_frames[btn_idx]++;
    if (s_hold_frames[btn_idx] == 1) return true;
    if (s_hold_frames[btn_idx] > REPEAT_DELAY &&
        (s_hold_frames[btn_idx] - REPEAT_DELAY) % REPEAT_RATE == 0)
        return true;
    return false;
}

// Analog stick with dead zone
float inputAnalogX() {
    int raw = (int)s_pad_curr.Lx - 128;
    if (abs(raw) < 20) return 0.0f;
    return raw / 128.0f;
}

float inputAnalogY() {
    int raw = (int)s_pad_curr.Ly - 128;
    if (abs(raw) < 20) return 0.0f;
    return raw / 128.0f;
}

// ── Open a comic / image file and populate Archive + Image ───────────────────
static void openFile(AppState* state, const char* path) {
    archiveClose(&state->arc);
    imageUnload(&state->img);
    imageUnload(&state->img_next);

    if (archiveOpen(&state->arc, path)) {
        state->page        = 0;
        state->total_pages = state->arc.page_count;
        state->pan_x = state->pan_y = 0;
        state->zoom = 1.0f;

        // Restore last position for this file if setting enabled
        if (state->settings.remember_position) {
            int saved = bookmarkGetPage(path);
            if (saved > 0 && saved < state->total_pages)
                state->page = saved;
        }

        imageLoadPage(&state->arc, state->page, &state->img);
        imageFitToScreen(&state->img, &state->zoom,
                         &state->pan_x, &state->pan_y,
                         state->fit_mode, state->rotation);

        if (state->settings.preload_next && state->total_pages > 1)
            imageLoadPage(&state->arc, 1, &state->img_next);

        state->screen = SCREEN_READER;
        snprintf(state->status_msg, sizeof(state->status_msg),
                 "Opened: %s", path + (strrchr(path, '/') ? strrchr(path,'/')-path+1 : 0));
        state->status_timer = 90;
    }
}

static void turnPage(AppState* state, int delta) {
    int next = state->page + delta;
    if (next < 0 || next >= state->total_pages) return;

    // Save position
    bookmarkUpdatePosition(state->arc.path, state->page);

    // Swap preloaded image if stepping forward
    if (delta == 1 && state->img_next.loaded) {
        imageUnload(&state->img);
        state->img = state->img_next;
        memset(&state->img_next, 0, sizeof(Image));
    } else {
        imageUnload(&state->img);
        imageLoadPage(&state->arc, next, &state->img);
    }

    state->page  = next;
    state->pan_x = state->pan_y = 0;
    imageFitToScreen(&state->img, &state->zoom,
                     &state->pan_x, &state->pan_y,
                     state->fit_mode, state->rotation);

    // Preload next
    if (state->settings.preload_next) {
        imageUnload(&state->img_next);
        int after = next + 1;
        if (after < state->total_pages)
            imageLoadPage(&state->arc, after, &state->img_next);
    }
}

// ── Per-screen input handlers ─────────────────────────────────────────────────

static void handleBrowserInput(AppState* state) {
    if (inputRepeat(PSP_CTRL_DOWN,  0)) browserMove(&state->browser,  1);
    if (inputRepeat(PSP_CTRL_UP,    1)) browserMove(&state->browser, -1);
    if (inputRepeat(PSP_CTRL_RIGHT, 2)) browserMove(&state->browser,  state->browser.items_per_page);
    if (inputRepeat(PSP_CTRL_LEFT,  3)) browserMove(&state->browser, -state->browser.items_per_page);

    if (inputPressed(PSP_CTRL_CROSS)) {
        char path[MAX_PATH_LEN];
        int type = browserSelect(&state->browser, path, sizeof(path));
        if (type == 0) {
            // Directory — navigate into it
            browserEnter(&state->browser, path);
        } else if (type == 1) {
            // File — open it
            openFile(state, path);
        }
    }

    if (inputPressed(PSP_CTRL_CIRCLE)) {
        browserUp(&state->browser);
    }

    if (inputPressed(PSP_CTRL_TRIANGLE)) {
        state->prev_screen = state->screen;
        state->screen = SCREEN_MENU;
        state->menu_cursor = 0;
    }

    if (inputPressed(PSP_CTRL_SELECT)) {
        state->prev_screen = state->screen;
        state->screen = SCREEN_BOOKMARKS;
        state->bookmark_cursor = 0;
    }
}

static void handleReaderInput(AppState* state) {
    int speed = state->settings.pan_speed;
    bool lr   = inputHeld(PSP_CTRL_LTRIGGER) && inputHeld(PSP_CTRL_RTRIGGER);

    // L+R → fit to screen
    if (lr) {
        state->fit_mode = FIT_SCREEN;
        imageFitToScreen(&state->img, &state->zoom,
                         &state->pan_x, &state->pan_y,
                         state->fit_mode, state->rotation);
        return;
    }

    // Zoom with Square (+) and analog trigger combo
    if (inputPressed(PSP_CTRL_SQUARE)) {
        // Cycle fit modes
        state->fit_mode = (FitMode)((state->fit_mode + 1) % 5);
        imageFitToScreen(&state->img, &state->zoom,
                         &state->pan_x, &state->pan_y,
                         state->fit_mode, state->rotation);
    }

    // L / R triggers → page turn (manga-aware)
    bool prev_page = state->manga_mode ? inputPressed(PSP_CTRL_RTRIGGER)
                                       : inputPressed(PSP_CTRL_LTRIGGER);
    bool next_page = state->manga_mode ? inputPressed(PSP_CTRL_LTRIGGER)
                                       : inputPressed(PSP_CTRL_RTRIGGER);

    if (prev_page) { turnPage(state, -1); return; }
    if (next_page) { turnPage(state,  1); return; }

    // Triangle → menu
    if (inputPressed(PSP_CTRL_TRIANGLE)) {
        state->prev_screen = state->screen;
        state->screen = SCREEN_MENU;
        state->menu_cursor = 0;
        return;
    }

    // Circle → back to browser
    if (inputPressed(PSP_CTRL_CIRCLE)) {
        bookmarkUpdatePosition(state->arc.path, state->page);
        state->screen = SCREEN_BROWSER;
        return;
    }

    // Start → toggle info bar
    if (inputPressed(PSP_CTRL_START)) {
        state->show_info = !state->show_info;
    }

    // Select → bookmarks
    if (inputPressed(PSP_CTRL_SELECT)) {
        state->prev_screen = state->screen;
        state->screen = SCREEN_BOOKMARKS;
        state->bookmark_cursor = 0;
    }

    // Cross → add bookmark for current page
    if (inputPressed(PSP_CTRL_CROSS)) {
        bookmarkAdd(state->arc.path, state->page);
        snprintf(state->status_msg, sizeof(state->status_msg),
                 "Bookmark saved: page %d", state->page + 1);
        state->status_timer = 60;
    }

    // ── Pan with D-Pad and analog stick ──────────────────────────────────────
    float ax = inputAnalogX();
    float ay = inputAnalogY();
    state->pan_x += (int)(ax * speed * 1.5f);
    state->pan_y += (int)(ay * speed * 1.5f);

    if (inputHeld(PSP_CTRL_RIGHT)) state->pan_x += speed;
    if (inputHeld(PSP_CTRL_LEFT))  state->pan_x -= speed;
    if (inputHeld(PSP_CTRL_DOWN))  state->pan_y += speed;
    if (inputHeld(PSP_CTRL_UP))    state->pan_y -= speed;

    // Clamp pan to image bounds
    int img_w = (int)(state->img.width  * state->zoom);
    int img_h = (int)(state->img.height * state->zoom);
    int max_x = MAX(0, img_w - SCR_W);
    int max_y = MAX(0, img_h - SCR_H);
    state->pan_x = CLAMP(state->pan_x, 0, max_x);
    state->pan_y = CLAMP(state->pan_y, 0, max_y);
}

static void handleMenuInput(AppState* state) {
    const int MENU_ITEMS = 6;
    if (inputRepeat(PSP_CTRL_DOWN, 0)) state->menu_cursor = (state->menu_cursor + 1) % MENU_ITEMS;
    if (inputRepeat(PSP_CTRL_UP,   1)) state->menu_cursor = (state->menu_cursor - 1 + MENU_ITEMS) % MENU_ITEMS;

    if (inputPressed(PSP_CTRL_CROSS)) {
        switch (state->menu_cursor) {
            case 0: state->screen = SCREEN_READER;    break; // Resume
            case 1: state->screen = SCREEN_BROWSER;   break; // Library
            case 2: state->screen = SCREEN_BOOKMARKS; break; // Bookmarks
            case 3: state->screen = SCREEN_SETTINGS;
                    state->settings_cursor = 0;        break; // Settings
            case 4: state->screen = SCREEN_ABOUT;     break; // About
            case 5: state->running = false;            break; // Quit
        }
    }
    if (inputPressed(PSP_CTRL_CIRCLE) || inputPressed(PSP_CTRL_TRIANGLE)) {
        state->screen = state->prev_screen;
    }
}

static void handleSettingsInput(AppState* state) {
    const int SETTINGS_ITEMS = 10;
    if (inputRepeat(PSP_CTRL_DOWN, 0))
        state->settings_cursor = (state->settings_cursor + 1) % SETTINGS_ITEMS;
    if (inputRepeat(PSP_CTRL_UP, 1))
        state->settings_cursor = (state->settings_cursor - 1 + SETTINGS_ITEMS) % SETTINGS_ITEMS;

    if (inputPressed(PSP_CTRL_RIGHT) || inputPressed(PSP_CTRL_CROSS)) {
        switch (state->settings_cursor) {
            case 0: state->settings.default_fit    = (state->settings.default_fit + 1) % 5; break;
            case 1: state->settings.manga_mode     = !state->settings.manga_mode; break;
            case 2: state->settings.show_clock     = !state->settings.show_clock; break;
            case 3: state->settings.show_battery   = !state->settings.show_battery; break;
            case 4: state->settings.pan_speed      = CLAMP(state->settings.pan_speed + 1, 1, 20); break;
            case 5: state->settings.smooth_scroll  = !state->settings.smooth_scroll; break;
            case 6: state->settings.preload_next   = !state->settings.preload_next; break;
            case 7: state->settings.remember_position = !state->settings.remember_position; break;
            case 8: {
                int speeds[] = {111, 222, 333};
                int cur = 0;
                for (int i = 0; i < 3; i++) if (speeds[i] == state->settings.cpu_speed) { cur = i; break; }
                state->settings.cpu_speed = speeds[(cur + 1) % 3];
                scePowerSetClockFrequency(state->settings.cpu_speed,
                                          state->settings.cpu_speed,
                                          state->settings.cpu_speed / 2);
                break;
            }
            case 9: state->settings.zoom_step = CLAMP(state->settings.zoom_step + 0.05f, 0.1f, 1.0f); break;
        }
    }
    if (inputPressed(PSP_CTRL_LEFT)) {
        switch (state->settings_cursor) {
            case 0: state->settings.default_fit    = (state->settings.default_fit + 4) % 5; break;
            case 4: state->settings.pan_speed      = CLAMP(state->settings.pan_speed - 1, 1, 20); break;
            case 9: state->settings.zoom_step      = CLAMP(state->settings.zoom_step - 0.05f, 0.1f, 1.0f); break;
        }
    }

    if (inputPressed(PSP_CTRL_CIRCLE) || inputPressed(PSP_CTRL_TRIANGLE)) {
        settingsSave(&state->settings);
        state->manga_mode = state->settings.manga_mode;
        state->fit_mode   = (FitMode)state->settings.default_fit;
        state->screen     = state->prev_screen;
    }
}

static void handleBookmarksInput(AppState* state) {
    if (inputRepeat(PSP_CTRL_DOWN, 0))
        state->bookmark_cursor = (state->bookmark_cursor + 1) % MAX(1, state->bookmark_count);
    if (inputRepeat(PSP_CTRL_UP, 1))
        state->bookmark_cursor = (state->bookmark_cursor - 1 + MAX(1, state->bookmark_count))
                                 % MAX(1, state->bookmark_count);

    if (inputPressed(PSP_CTRL_CROSS) && state->bookmark_count > 0) {
        Bookmark* bm = &state->bookmarks[state->bookmark_cursor];
        if (strcmp(bm->file, state->arc.path) == 0) {
            // Same file — just jump to page
            if (bm->page != state->page) {
                imageUnload(&state->img);
                imageLoadPage(&state->arc, bm->page, &state->img);
                state->page  = bm->page;
                state->pan_x = state->pan_y = 0;
                imageFitToScreen(&state->img, &state->zoom,
                                 &state->pan_x, &state->pan_y,
                                 state->fit_mode, state->rotation);
            }
        } else {
            openFile(state, bm->file);
            // Jump to page after open
            if (bm->page > 0 && bm->page < state->total_pages) {
                imageUnload(&state->img);
                imageLoadPage(&state->arc, bm->page, &state->img);
                state->page  = bm->page;
                imageFitToScreen(&state->img, &state->zoom,
                                 &state->pan_x, &state->pan_y,
                                 state->fit_mode, state->rotation);
            }
        }
        state->screen = SCREEN_READER;
    }

    // Square → delete bookmark
    if (inputPressed(PSP_CTRL_SQUARE) && state->bookmark_count > 0) {
        bookmarkDelete(state->bookmark_cursor);
        if (state->bookmark_cursor >= state->bookmark_count)
            state->bookmark_cursor = MAX(0, state->bookmark_count - 1);
    }

    if (inputPressed(PSP_CTRL_CIRCLE) || inputPressed(PSP_CTRL_TRIANGLE)) {
        state->screen = state->prev_screen;
    }
}

void handleInput(AppState* state) {
    if (state->status_timer > 0) state->status_timer--;

    switch (state->screen) {
        case SCREEN_BROWSER:   handleBrowserInput(state);   break;
        case SCREEN_READER:    handleReaderInput(state);    break;
        case SCREEN_MENU:      handleMenuInput(state);      break;
        case SCREEN_SETTINGS:  handleSettingsInput(state);  break;
        case SCREEN_BOOKMARKS: handleBookmarksInput(state); break;
        case SCREEN_ABOUT:
            if (inputPressed(PSP_CTRL_CIRCLE) || inputPressed(PSP_CTRL_CROSS))
                state->screen = state->prev_screen;
            break;
    }
}
