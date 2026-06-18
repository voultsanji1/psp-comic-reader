#pragma once
#include "main.h"

bool  archiveOpen(Archive* arc, const char* path);
void  archiveClose(Archive* arc);
u8*   archiveGetPageData(Archive* arc, int page, size_t* out_size);
