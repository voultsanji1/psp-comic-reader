#include <stdio.h>
#include <string.h>
#include "main.h"
#include "settings.h"

#define SETTINGS_FILE "ms0:/PSP/SAVEDATA/PSPComicReader/settings.dat"
#define SETTINGS_MAGIC 0x43524452  // 'CRDR'
#define SETTINGS_VER   2

typedef struct {
    unsigned int magic;
    int          version;
    Settings     data;
} SettingsFile;

static void settingsDefault(Settings* s) {
    memset(s, 0, sizeof(Settings));
    snprintf(s->last_dir, MAX_PATH_LEN, "ms0:/COMICS");
    s->default_fit       = FIT_WIDTH;
    s->manga_mode        = false;
    s->show_clock        = true;
    s->show_battery      = true;
    s->pan_speed         = 8;
    s->zoom_step         = 0.25f;
    s->smooth_scroll     = false;
    s->preload_next      = true;
    s->bg_color          = 0xFF000000; // black
    s->ui_color          = 0xFFFFFFFF; // white
    s->remember_position = true;
    s->cpu_speed         = 333;
}

void settingsLoad(Settings* s) {
    settingsDefault(s);
    FILE* f = fopen(SETTINGS_FILE, "rb");
    if (!f) return;
    SettingsFile sf;
    if (fread(&sf, sizeof(sf), 1, f) == 1 &&
        sf.magic == SETTINGS_MAGIC && sf.version == SETTINGS_VER)
        *s = sf.data;
    fclose(f);
}

void settingsSave(const Settings* s) {
    sceIoMkdir("ms0:/PSP/SAVEDATA/PSPComicReader", 0777);
    FILE* f = fopen(SETTINGS_FILE, "wb");
    if (!f) return;
    SettingsFile sf = { SETTINGS_MAGIC, SETTINGS_VER, *s };
    fwrite(&sf, sizeof(sf), 1, f);
    fclose(f);
}
