#pragma once
#include <psptypes.h>

void displayInit();
void displayShutdown();
void displayBegin();
void displayEnd();
void displayDrawTexture(u32* tex, int tw, int th, int dx, int dy, int dw, int dh);
void displayFillRect(int x, int y, int w, int h, u32 color);
void displayHLine(int x, int y, int w, u32 color);
u32* displayUploadTexture(u8* rgba, int w, int h);
