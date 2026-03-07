#pragma once

#include "Application.h"

#define MAX_LOADSTRING 100
#define HEX_TO_RGB(hex) RGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF)

class VolumeApp : public Application {
public:
    WPARAM create(WNDPROC winProc);
    void shutDown(HWND hWnd);
};
