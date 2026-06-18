/*
 * PSP Comic Reader v1.0
 * Supports: CBZ, CBR, CB7, CBT, ZIP, RAR, 7Z, TAR, PDF (pages as images),
 *           JPG, PNG, GIF, BMP, WEBP, and plain image folders
 *
 * Controls:
 *   D-Pad / Analog : Pan image
 *   Cross          : Select / Confirm
 *   Circle         : Back / Cancel
 *   Triangle       : Menu / Settings
 *   Square         : Zoom toggle
 *   L Trigger      : Previous page
 *   R Trigger      : Next page
 *   L+R            : Fit to screen
 *   Start          : Show/hide info bar
 *   Select         : Bookmarks
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <psppower.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "display.h"
#include "input.h"
#include "browser.h"
#include "bookmarks.h"
#include "settings.h"
#include "ui.h"
#include "image.h"

PSP_MODULE_INFO("PSPComicReader", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024); // Use all RAM except 1MB for stack

// Global app state
AppState g_state;

// Exit callback
static int exitCallback(int arg1, int arg2, void* common) {
    g_state.running = false;
    return 0;
}

static int callbackThread(SceSize args, void* argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setupCallbacks() {
    int thid = sceKernelCreateThread("update_thread", callbackThread,
                                     0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

void appInit() {
    memset(&g_state, 0, sizeof(AppState));
    g_state.running    = true;
    g_state.screen     = SCREEN_BROWSER;
    g_state.zoom       = 1.0f;
    g_state.pan_x      = 0;
    g_state.pan_y      = 0;
    g_state.page       = 0;
    g_state.total_pages = 0;
    g_state.fit_mode   = FIT_WIDTH;
    g_state.rotation   = ROT_0;
    g_state.show_info  = true;
    g_state.manga_mode = false; // RTL page turn

    // Init subsystems
    displayInit();
    inputInit();
    settingsLoad(&g_state.settings);
    bookmarksLoad();
    browserInit(g_state.settings.last_dir[0] ? g_state.settings.last_dir : "ms0:/COMICS");

    // Apply loaded settings
    g_state.manga_mode = g_state.settings.manga_mode;
    g_state.fit_mode   = (FitMode)g_state.settings.default_fit;
}

void appShutdown() {
    imageUnload(&g_state.img);
    archiveClose(&g_state.arc);
    bookmarksSave();
    settingsSave(&g_state.settings);
    displayShutdown();
}

int main(int argc, char* argv[]) {
    setupCallbacks();
    scePowerSetClockFrequency(333, 333, 166);

    appInit();

    while (g_state.running) {
        inputUpdate();
        handleInput(&g_state);

        displayBegin();

        switch (g_state.screen) {
            case SCREEN_BROWSER:
                uiDrawBrowser(&g_state);
                break;
            case SCREEN_READER:
                uiDrawReader(&g_state);
                break;
            case SCREEN_MENU:
                uiDrawMenu(&g_state);
                break;
            case SCREEN_SETTINGS:
                uiDrawSettings(&g_state);
                break;
            case SCREEN_BOOKMARKS:
                uiDrawBookmarks(&g_state);
                break;
            case SCREEN_ABOUT:
                uiDrawAbout(&g_state);
                break;
        }

        displayEnd();
        sceDisplayWaitVblankStart();
    }

    appShutdown();
    sceKernelExitGame();
    return 0;
}
