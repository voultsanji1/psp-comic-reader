#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include "main.h"
#include "archive.h"
#include "libs/miniz/miniz.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static int cmpStr(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

static bool isImageExt(const char* name) {
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[8]; int i = 0;
    for (const char* p = dot + 1; *p && i < 7; p++, i++)
        ext[i] = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
    ext[i] = '\0';
    return !strcmp(ext, "jpg")  || !strcmp(ext, "jpeg") ||
           !strcmp(ext, "png")  || !strcmp(ext, "gif")  ||
           !strcmp(ext, "bmp")  || !strcmp(ext, "webp") ||
           !strcmp(ext, "tga")  || !strcmp(ext, "ppm");
}

static ArchiveType detectType(const char* path) {
    // Check if it is a directory first
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return ARC_DIR;

    const char* dot = strrchr(path, '.');
    if (!dot) return ARC_NONE;
    char ext[8]; int i = 0;
    for (const char* p = dot + 1; *p && i < 7; p++, i++)
        ext[i] = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
    ext[i] = '\0';

    if (!strcmp(ext, "cbz") || !strcmp(ext, "zip")) return ARC_ZIP;
    if (!strcmp(ext, "cbr") || !strcmp(ext, "rar")) return ARC_RAR;
    if (!strcmp(ext, "cb7") || !strcmp(ext, "7z"))  return ARC_7Z;
    if (!strcmp(ext, "cbt") || !strcmp(ext, "tar")) return ARC_TAR;
    if (!strcmp(ext, "pdf"))                         return ARC_PDF;

    // Single image file — treat as a "directory" of one
    if (isImageExt(ext)) return ARC_DIR;

    return ARC_NONE;
}

// ── ZIP / CBZ ─────────────────────────────────────────────────────────────────

static bool openZip(Archive* arc) {
    mz_zip_archive* zip = (mz_zip_archive*)calloc(1, sizeof(mz_zip_archive));
    if (!mz_zip_reader_init_file(zip, arc->path, 0)) {
        free(zip);
        return false;
    }
    arc->handle      = zip;
    int n            = (int)mz_zip_reader_get_num_files(zip);
    arc->page_count  = 0;

    // Collect image entries
    char tmp[MAX_PAGES][MAX_FILENAME];
    int  tcnt = 0;
    for (int i = 0; i < n && tcnt < MAX_PAGES; i++) {
        mz_zip_archive_file_stat fs;
        if (!mz_zip_reader_file_stat(zip, i, &fs)) continue;
        if (mz_zip_reader_is_file_a_directory(zip, i)) continue;
        const char* name = fs.m_filename;
        const char* base = strrchr(name, '/');
        base = base ? base + 1 : name;
        if (!isImageExt(base)) continue;
        snprintf(tmp[tcnt], MAX_FILENAME, "%s", name);
        tcnt++;
    }
    // Sort pages
    qsort(tmp, tcnt, MAX_FILENAME, cmpStr);
    for (int i = 0; i < tcnt; i++)
        memcpy(arc->pages[i], tmp[i], MAX_FILENAME);
    arc->page_count = tcnt;
    return tcnt > 0;
}

// ── Directory / single image ──────────────────────────────────────────────────

static bool openDir(Archive* arc) {
    struct stat st;
    stat(arc->path, &st);

    if (S_ISREG(st.st_mode)) {
        // Single image
        const char* base = strrchr(arc->path, '/');
        base = base ? base + 1 : arc->path;
        snprintf(arc->pages[0], MAX_FILENAME, "%s", base);
        arc->page_count = 1;
        arc->handle     = NULL;
        return true;
    }

    DIR* d = opendir(arc->path);
    if (!d) return false;

    char tmp[MAX_PAGES][MAX_FILENAME];
    int  tcnt = 0;
    struct dirent* de;
    while ((de = readdir(d)) != NULL && tcnt < MAX_PAGES) {
        if (de->d_name[0] == '.') continue;
        if (!isImageExt(de->d_name)) continue;
        snprintf(tmp[tcnt], MAX_FILENAME, "%s", de->d_name);
        tcnt++;
    }
    closedir(d);
    qsort(tmp, tcnt, MAX_FILENAME, cmpStr);
    for (int i = 0; i < tcnt; i++)
        memcpy(arc->pages[i], tmp[i], MAX_FILENAME);
    arc->page_count = tcnt;
    arc->handle     = NULL;
    return tcnt > 0;
}

// ── RAR stub (unrar-free or system rar) ───────────────────────────────────────
// Full RAR support requires linking libunrar or calling system unrar.
// We provide a stub that extracts to /tmp and falls back to directory mode.

static bool openRar(Archive* arc) {
    // Extract to temp dir using unrar (must be available on firmware)
    char cmd[512], tmpdir[MAX_PATH_LEN];
    snprintf(tmpdir, sizeof(tmpdir), "ms0:/TMP/cbr_extract");
    snprintf(cmd, sizeof(cmd), "unrar e -o+ \"%s\" \"%s/\"", arc->path, tmpdir);
    system(cmd);

    // Reopen as directory
    char saved[MAX_PATH_LEN];
    snprintf(saved, sizeof(saved), "%s", arc->path);
    snprintf(arc->path, MAX_PATH_LEN, "%s", tmpdir);
    bool ok = openDir(arc);
    if (!ok) snprintf(arc->path, MAX_PATH_LEN, "%s", saved);
    return ok;
}

// ── TAR / CBT ─────────────────────────────────────────────────────────────────
// Minimal TAR reader (POSIX ustar, no compression) - handles uncompressed CBT

static bool openTar(Archive* arc) {
    FILE* f = fopen(arc->path, "rb");
    if (!f) return false;

    // We don't extract; we record offsets inline using tar headers
    // For simplicity: extract to temp and open as dir (same as RAR fallback)
    fclose(f);
    char cmd[512], tmpdir[MAX_PATH_LEN];
    snprintf(tmpdir, sizeof(tmpdir), "ms0:/TMP/cbt_extract");
    snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\"", arc->path, tmpdir);
    system(cmd);
    char saved[MAX_PATH_LEN];
    snprintf(saved, sizeof(saved), "%s", arc->path);
    snprintf(arc->path, MAX_PATH_LEN, "%s", tmpdir);
    bool ok = openDir(arc);
    if (!ok) snprintf(arc->path, MAX_PATH_LEN, "%s", saved);
    return ok;
}

// ── 7Z / CB7 ─────────────────────────────────────────────────────────────────

static bool open7z(Archive* arc) {
    char cmd[512], tmpdir[MAX_PATH_LEN];
    snprintf(tmpdir, sizeof(tmpdir), "ms0:/TMP/cb7_extract");
    snprintf(cmd, sizeof(cmd), "7z e \"%s\" -o\"%s\" -y", arc->path, tmpdir);
    system(cmd);
    char saved[MAX_PATH_LEN];
    snprintf(saved, sizeof(saved), "%s", arc->path);
    snprintf(arc->path, MAX_PATH_LEN, "%s", tmpdir);
    bool ok = openDir(arc);
    if (!ok) snprintf(arc->path, MAX_PATH_LEN, "%s", saved);
    return ok;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool archiveOpen(Archive* arc, const char* path) {
    memset(arc, 0, sizeof(Archive));
    snprintf(arc->path, MAX_PATH_LEN, "%s", path);
    arc->type = detectType(path);

    switch (arc->type) {
        case ARC_ZIP: return openZip(arc);
        case ARC_RAR: return openRar(arc);
        case ARC_7Z:  return open7z(arc);
        case ARC_TAR: return openTar(arc);
        case ARC_DIR: return openDir(arc);
        default:      return false;
    }
}

void archiveClose(Archive* arc) {
    if (!arc) return;
    if (arc->type == ARC_ZIP && arc->handle) {
        mz_zip_reader_end((mz_zip_archive*)arc->handle);
        free(arc->handle);
    }
    memset(arc, 0, sizeof(Archive));
}

// Extract page data into malloc'd buffer; caller must free()
u8* archiveGetPageData(Archive* arc, int page, size_t* out_size) {
    if (page < 0 || page >= arc->page_count) return NULL;

    if (arc->type == ARC_ZIP) {
        mz_zip_archive* zip = (mz_zip_archive*)arc->handle;
        // Find file index by name
        int idx = mz_zip_reader_locate_file(zip, arc->pages[page], NULL, 0);
        if (idx < 0) return NULL;
        size_t sz = 0;
        void* data = mz_zip_reader_extract_to_heap(zip, idx, &sz, 0);
        *out_size = sz;
        return (u8*)data;
    }

    if (arc->type == ARC_DIR) {
        // Single image or directory
        char full[MAX_PATH_LEN];
        struct stat st;
        stat(arc->path, &st);
        if (S_ISDIR(st.st_mode))
            snprintf(full, sizeof(full), "%s/%s", arc->path, arc->pages[page]);
        else
            snprintf(full, sizeof(full), "%s", arc->path);

        FILE* f = fopen(full, "rb");
        if (!f) return NULL;
        fseek(f, 0, SEEK_END);
        size_t sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        u8* buf = (u8*)malloc(sz);
        fread(buf, 1, sz, f);
        fclose(f);
        *out_size = sz;
        return buf;
    }

    // RAR/7Z/TAR were extracted to disk — read from extracted dir
    char full[MAX_PATH_LEN];
    snprintf(full, sizeof(full), "%s/%s", arc->path, arc->pages[page]);
    FILE* f = fopen(full, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8* buf = (u8*)malloc(sz);
    fread(buf, 1, sz, f);
    fclose(f);
    *out_size = sz;
    return buf;
}
