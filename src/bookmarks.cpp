#include <stdio.h>
#include <string.h>
#include "main.h"
#include "bookmarks.h"

#define BOOKMARK_FILE "ms0:/PSP/SAVEDATA/PSPComicReader/bookmarks.dat"

extern AppState g_state;

void bookmarksLoad() {
    g_state.bookmark_count = 0;
    FILE* f = fopen(BOOKMARK_FILE, "rb");
    if (!f) return;
    fread(&g_state.bookmark_count, sizeof(int), 1, f);
    if (g_state.bookmark_count > MAX_BOOKMARKS) g_state.bookmark_count = MAX_BOOKMARKS;
    fread(g_state.bookmarks, sizeof(Bookmark), g_state.bookmark_count, f);
    fclose(f);
}

void bookmarksSave() {
    // Create directory if needed
    sceIoMkdir("ms0:/PSP/SAVEDATA/PSPComicReader", 0777);
    FILE* f = fopen(BOOKMARK_FILE, "wb");
    if (!f) return;
    fwrite(&g_state.bookmark_count, sizeof(int), 1, f);
    fwrite(g_state.bookmarks, sizeof(Bookmark), g_state.bookmark_count, f);
    fclose(f);
}

void bookmarkAdd(const char* file, int page) {
    // Update if same file+page exists
    for (int i = 0; i < g_state.bookmark_count; i++) {
        if (strcmp(g_state.bookmarks[i].file, file) == 0 &&
            g_state.bookmarks[i].page == page) return;
    }
    if (g_state.bookmark_count >= MAX_BOOKMARKS) {
        // Evict oldest
        memmove(&g_state.bookmarks[0], &g_state.bookmarks[1],
                sizeof(Bookmark) * (MAX_BOOKMARKS - 1));
        g_state.bookmark_count = MAX_BOOKMARKS - 1;
    }
    Bookmark* bm = &g_state.bookmarks[g_state.bookmark_count++];
    snprintf(bm->file,  MAX_PATH_LEN, "%s", file);
    snprintf(bm->label, 64, "Page %d", page + 1);
    bm->page = page;
    bookmarksSave();
}

void bookmarkDelete(int idx) {
    if (idx < 0 || idx >= g_state.bookmark_count) return;
    memmove(&g_state.bookmarks[idx], &g_state.bookmarks[idx + 1],
            sizeof(Bookmark) * (g_state.bookmark_count - idx - 1));
    g_state.bookmark_count--;
    bookmarksSave();
}

// Returns saved page for a file (for resume), or -1
int bookmarkGetPage(const char* file) {
    // We use a separate "last position" mechanism: look for label "lastpos"
    for (int i = 0; i < g_state.bookmark_count; i++) {
        if (strcmp(g_state.bookmarks[i].file, file) == 0 &&
            strcmp(g_state.bookmarks[i].label, "lastpos") == 0)
            return g_state.bookmarks[i].page;
    }
    return -1;
}

void bookmarkUpdatePosition(const char* file, int page) {
    for (int i = 0; i < g_state.bookmark_count; i++) {
        if (strcmp(g_state.bookmarks[i].file, file) == 0 &&
            strcmp(g_state.bookmarks[i].label, "lastpos") == 0) {
            g_state.bookmarks[i].page = page;
            bookmarksSave();
            return;
        }
    }
    // Add new lastpos entry
    if (g_state.bookmark_count < MAX_BOOKMARKS) {
        Bookmark* bm = &g_state.bookmarks[g_state.bookmark_count++];
        snprintf(bm->file, MAX_PATH_LEN, "%s", file);
        bm->page = page;
        snprintf(bm->label, 64, "lastpos");
        bookmarksSave();
    }
}
