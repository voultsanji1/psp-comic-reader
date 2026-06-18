/*
 * miniz.h — bundled via GitHub Actions (downloaded during build)
 * Source: https://github.com/richgel999/miniz
 * Single-header ZIP/DEFLATE library, public domain.
 *
 * The actual file is fetched by the CI workflow before compilation.
 */
#pragma once

#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES

// Re-enable the parts we need
#undef MINIZ_NO_ARCHIVE_APIS

#include "miniz.c"  // single-header style; included once from archive.cpp
