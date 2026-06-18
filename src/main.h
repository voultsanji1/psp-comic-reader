#pragma once
#include <psptypes.h>

// ── Screen resolution ───────────────────────────────────────────────────────
#define SCR_W   480
#define SCR_H   272
#define SCR_BPP 4

// ── Limits ───────────────────────────────────────────────────────────────────
#define MAX_PATH_LEN  256
#define MAX_FILENAME  64
#define MAX_PAGES     4096
#define MAX_BOOKMARKS 64
#define MAX_DIR_ITEMS 512

// ── Enumerations ─────────────────────────────────────────────────────────────
typedef enum {
    SCREEN_BROWSER = 0,
    SCREEN_READER,
    SCREEN_MENU,
    SCREEN_SETTINGS,
    SCREEN_BOOKMARKS,
    SCREEN_ABOUT
} ScreenMode;

typedef enum {
    FIT_NONE = 0,   // Original size
    FIT_WIDTH,      // Fit to screen width
    FIT_HEIGHT,     // Fit to screen height
    FIT_SCREEN,     // Fit entire page on screen
    FIT_STRETCH     // Stretch to fill screen
} FitMode;

typedef enum {
    ROT_0 = 0,
    ROT_90,
    ROT_180,
    ROT_270
} Rotation;

typedef enum {
    ARC_NONE = 0,
    ARC_ZIP,        // CBZ, ZIP
    ARC_RAR,        // CBR, RAR  (via unrar-free / libunrar)
    ARC_7Z,         // CB7, 7Z
    ARC_TAR,        // CBT, TAR
    ARC_DIR,        // Plain folder of images
    ARC_PDF         // PDF
} ArchiveType;

// ── Settings ─────────────────────────────────────────────────────────────────
typedef struct {
    char    last_dir[MAX_PATH_LEN];
    int     default_fit;        // FitMode
    bool    manga_mode;         // RTL page order
    bool    show_clock;
    bool    show_battery;
    int     pan_speed;          // 1-20
    float   zoom_step;          // 0.1 - 1.0
    bool    smooth_scroll;
    bool    preload_next;
    int     bg_color;           // RGBA packed
    int     ui_color;           // RGBA packed
    bool    remember_position;
    int     cpu_speed;          // 111 / 222 / 333
} Settings;

// ── Bookmark ──────────────────────────────────────────────────────────────────
typedef struct {
    char file[MAX_PATH_LEN];
    int  page;
    char label[64];
} Bookmark;

// ── Archive context ───────────────────────────────────────────────────────────
typedef struct {
    ArchiveType type;
    char        path[MAX_PATH_LEN];
    char        pages[MAX_PAGES][MAX_FILENAME];
    int         page_count;
    void*       handle;     // type-specific handle (mz_zip_archive*, etc.)
} Archive;

// ── Loaded image ──────────────────────────────────────────────────────────────
typedef struct {
    u8*     pixels;     // RGBA8888 buffer
    int     width;
    int     height;
    u32*    vram;       // Pointer into VRAM after upload
    int     tex_w;      // Texture width (power-of-2)
    int     tex_h;
    bool    loaded;
} Image;

// ── Browser state ─────────────────────────────────────────────────────────────
typedef struct {
    char    cwd[MAX_PATH_LEN];
    char    items[MAX_DIR_ITEMS][MAX_FILENAME];
    int     item_types[MAX_DIR_ITEMS]; // 0=dir, 1=file
    int     count;
    int     cursor;
    int     scroll_offset;
    int     items_per_page;
} Browser;

// ── Global application state ──────────────────────────────────────────────────
typedef struct {
    bool        running;
    ScreenMode  screen;
    ScreenMode  prev_screen;

    // Reader state
    Archive     arc;
    Image       img;
    Image       img_next;   // Preloaded next page
    int         page;
    int         total_pages;
    float       zoom;
    float       min_zoom;
    float       max_zoom;
    int         pan_x;
    int         pan_y;
    FitMode     fit_mode;
    Rotation    rotation;
    bool        show_info;
    bool        manga_mode;

    // Browser
    Browser     browser;

    // Bookmarks
    Bookmark    bookmarks[MAX_BOOKMARKS];
    int         bookmark_count;
    int         bookmark_cursor;

    // Settings
    Settings    settings;

    // Menu
    int         menu_cursor;
    int         settings_cursor;

    // UI
    char        status_msg[128];
    int         status_timer;   // frames to show status message
} AppState;

// ── Utility macros ────────────────────────────────────────────────────────────
#define CLAMP(v, lo, hi)  ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define MIN(a, b)         ((a) < (b) ? (a) : (b))
#define MAX(a, b)         ((a) > (b) ? (a) : (b))
#define RGBA(r,g,b,a)     (((a)<<24)|((b)<<16)|((g)<<8)|(r))

// Declarations for subsystem init (defined in respective .cpp files)
void handleInput(AppState* state);
void archiveClose(Archive* arc);
void imageUnload(Image* img);
void bookmarksLoad();
void bookmarksSave();
void settingsLoad(Settings* s);
void settingsSave(const Settings* s);
void browserInit(const char* path);
