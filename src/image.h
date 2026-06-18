#pragma once
#include "main.h"

bool imageLoadPage(Archive* arc, int page, Image* img);
void imageUnload(Image* img);
void imageFitToScreen(Image* img, float* zoom, int* pan_x, int* pan_y,
                      FitMode fit, Rotation rot);
