#pragma once
#include <psptypes.h>

void inputInit();
void inputUpdate();
bool inputPressed(u32 btn);
bool inputHeld(u32 btn);
bool inputReleased(u32 btn);
bool inputRepeat(u32 btn, int btn_idx);
float inputAnalogX();
float inputAnalogY();
