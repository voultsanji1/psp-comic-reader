#pragma once
#include "main.h"

void browserInit(const char* path);
void browserRefresh(Browser* b);
void browserMove(Browser* b, int delta);
int  browserSelect(Browser* b, char* out, int out_len);
void browserEnter(Browser* b, const char* path);
void browserUp(Browser* b);
