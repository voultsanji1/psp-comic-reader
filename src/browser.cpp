#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "browser.h"

extern AppState g_state;

static bool isComicFile(const char* name) {
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[8]; int i = 0;
    for (const char* p = dot + 1; *p && i < 7; p++, i++)
        ext[i] = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
    ext[i] = '\0';
    return !strcmp(ext,"cbz")||!strcmp(ext,"cbr")||!strcmp(ext,"cb7")||
           !strcmp(ext,"cbt")||!strcmp(ext,"zip")||!strcmp(ext,"rar")||
           !strcmp(ext,"7z") ||!strcmp(ext,"tar")||!strcmp(ext,"pdf")||
           !strcmp(ext,"jpg")||!strcmp(ext,"jpeg")||!strcmp(ext,"png")||
           !strcmp(ext,"bmp")||!strcmp(ext,"gif")||!strcmp(ext,"webp");
}

static int cmpItem(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

void browserRefresh(Browser* b) {
    DIR* d = opendir(b->cwd);
    b->count = 0;

    if (!d) {
        // Fallback to ms0:/
        snprintf(b->cwd, MAX_PATH_LEN, "ms0:/");
        d = opendir(b->cwd);
        if (!d) return;
    }

    char dirs[MAX_DIR_ITEMS][MAX_FILENAME];
    char files[MAX_DIR_ITEMS][MAX_FILENAME];
    int  ndirs = 0, nfiles = 0;
    struct dirent* de;

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s/%s", b->cwd, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode) && ndirs < MAX_DIR_ITEMS) {
            snprintf(dirs[ndirs++], MAX_FILENAME, "%s", de->d_name);
        } else if (isComicFile(de->d_name) && nfiles < MAX_DIR_ITEMS) {
            snprintf(files[nfiles++], MAX_FILENAME, "%s", de->d_name);
        }
    }
    closedir(d);

    qsort(dirs,  ndirs,  MAX_FILENAME, cmpItem);
    qsort(files, nfiles, MAX_FILENAME, cmpItem);

    for (int i = 0; i < ndirs  && b->count < MAX_DIR_ITEMS; i++) {
        memcpy(b->items[b->count], dirs[i], MAX_FILENAME);
        b->item_types[b->count] = 0;
        b->count++;
    }
    for (int i = 0; i < nfiles && b->count < MAX_DIR_ITEMS; i++) {
        memcpy(b->items[b->count], files[i], MAX_FILENAME);
        b->item_types[b->count] = 1;
        b->count++;
    }

    b->cursor = 0;
    b->scroll_offset = 0;
    b->items_per_page = 16; // approximate; UI will adjust
}

void browserInit(const char* path) {
    Browser* b = &g_state.browser;
    snprintf(b->cwd, MAX_PATH_LEN, "%s", path);
    browserRefresh(b);
}

void browserMove(Browser* b, int delta) {
    b->cursor += delta;
    if (b->cursor < 0) b->cursor = 0;
    if (b->cursor >= b->count) b->cursor = b->count - 1;

    // Scroll to keep cursor visible
    if (b->cursor < b->scroll_offset)
        b->scroll_offset = b->cursor;
    if (b->cursor >= b->scroll_offset + b->items_per_page)
        b->scroll_offset = b->cursor - b->items_per_page + 1;
}

// Returns: 0=dir, 1=file, -1=error; writes full path to out
int browserSelect(Browser* b, char* out, int out_len) {
    if (b->count == 0) return -1;
    snprintf(out, out_len, "%s/%s", b->cwd, b->items[b->cursor]);
    return b->item_types[b->cursor];
}

void browserEnter(Browser* b, const char* path) {
    snprintf(b->cwd, MAX_PATH_LEN, "%s", path);
    browserRefresh(b);
    snprintf(g_state.settings.last_dir, MAX_PATH_LEN, "%s", path);
}

void browserUp(Browser* b) {
    char* slash = strrchr(b->cwd, '/');
    if (!slash || slash == b->cwd) return;
    *slash = '\0';
    if (b->cwd[0] == '\0') snprintf(b->cwd, MAX_PATH_LEN, "/");
    browserRefresh(b);
}
