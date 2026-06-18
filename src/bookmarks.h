#pragma once
#include "main.h"

void bookmarksLoad();
void bookmarksSave();
void bookmarkAdd(const char* file, int page);
void bookmarkDelete(int idx);
int  bookmarkGetPage(const char* file);
void bookmarkUpdatePosition(const char* file, int page);
