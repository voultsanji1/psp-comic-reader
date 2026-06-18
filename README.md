# PSP Comic Reader

A lightweight, fast comic reader for Sony PSP (1000/2000/3000/Go).  
Supports every common comic archive format and displays images directly with the PSP's hardware 2D engine.

---

## Features

| Feature | Detail |
|---------|--------|
| **Formats** | CBZ · CBR · CB7 · CBT · ZIP · RAR · 7Z · TAR · PDF · JPG · PNG · BMP · GIF · WEBP · plain image folders |
| **Zoom modes** | Original (1:1), Fit Width, Fit Height, Fit Screen, Stretch |
| **Pan** | D-Pad + analog stick with configurable speed |
| **Manga mode** | Reverses L/R trigger page-turn direction (RTL) |
| **Bookmarks** | Unlimited; auto-saved to Memory Stick |
| **Resume** | Remembers last page per file |
| **Preload** | Optionally preloads the next page in background |
| **Rotation** | 0 / 90 / 180 / 270° (via menu) |
| **HUD** | Toggle-able info bar with page/zoom/filename, clock, battery |
| **CPU speed** | 111 / 222 / 333 MHz selectable (save battery or boost speed) |
| **Settings** | All preferences persisted to Memory Stick |

---

## Controls

| Button | Action |
|--------|--------|
| **L Trigger** | Previous page (or Next in Manga mode) |
| **R Trigger** | Next page (or Previous in Manga mode) |
| **D-Pad** | Pan image |
| **Analog stick** | Pan image (smoother) |
| **Square** | Cycle zoom mode |
| **L + R** | Fit entire page to screen immediately |
| **Triangle** | Open menu |
| **Circle** | Back / cancel |
| **Cross** | Open file (browser) · Add bookmark (reader) |
| **Select** | Bookmarks list |
| **Start** | Toggle info bar on/off |

---

## Installation

### From a release (recommended)

1. Download `PSPComicReader.zip` from the [Releases](../../releases) page.
2. Extract and copy the `PSPComicReader/` folder to `PSP/GAME/` on your Memory Stick.
3. Launch from XMB → Game → Memory Stick.

### Put your comics here

```
ms0:/COMICS/
├── Superman Vol 1/
│   ├── Superman_001.cbz
│   └── Superman_002.cbz
└── Akira/
    └── Akira_01.cbr
```

The reader defaults to `ms0:/COMICS/` but you can browse anywhere on the card.

---

## Building from source

### GitHub Actions (recommended)

Push to `main` → a build runs automatically.  
Tag a commit with `v1.x.x` → a GitHub Release is created with the ZIP attached.

```bash
git tag v1.0.0
git push origin v1.0.0
```

### Local build

Requirements: [pspdev toolchain](https://github.com/pspdev/pspdev) in `$PATH`.

```bash
# Download vendor libraries
wget -O src/libs/miniz/miniz.h \
  https://raw.githubusercontent.com/richgel999/miniz/master/miniz.h
wget -O src/libs/miniz/miniz.c \
  https://raw.githubusercontent.com/richgel999/miniz/master/miniz.c
wget -O src/libs/stb/stb_image.h \
  https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

make
# → EBOOT.PBP
```

---

## Project structure

```
psp-comic-reader/
├── src/
│   ├── main.cpp/h        — app entry point, global state
│   ├── display.cpp/h     — PSP GU (GPU) rendering
│   ├── input.cpp/h       — controller + analog handling
│   ├── archive.cpp/h     — format detection, ZIP/RAR/7Z/TAR/dir
│   ├── image.cpp/h       — decode (stb_image), scale, rotate, VRAM upload
│   ├── browser.cpp/h     — file system browser
│   ├── bookmarks.cpp/h   — persistent bookmarks
│   ├── settings.cpp/h    — persistent settings
│   ├── ui.cpp/h          — all screen renderers
│   └── libs/
│       ├── miniz/        — single-header ZIP (downloaded at build time)
│       └── stb/          — stb_image (downloaded at build time)
├── cmake/
│   └── psp-toolchain.cmake
├── .github/workflows/
│   └── build.yml         — CI/CD pipeline
├── CMakeLists.txt
├── Makefile
└── README.md
```

---

## Third-party libraries

| Library | License | Purpose |
|---------|---------|---------|
| [miniz](https://github.com/richgel999/miniz) | MIT/Public Domain | CBZ/ZIP decompression |
| [stb_image](https://github.com/nothings/stb) | MIT/Public Domain | JPG, PNG, BMP, GIF decode |
| PSPSDK | BSD | PSP system headers + runtime |

---

## License

MIT — see [LICENSE](LICENSE).
