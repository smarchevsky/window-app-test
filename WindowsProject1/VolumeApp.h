#pragma once

#include "Application.h"

class VolumeApp : public Application {
    bool fMouseTracking;

public:
    void handlePreLoop(WNDPROC winProc);
    void shutDown(HWND hWnd);

    void handleMouseMove(POINT cursorScreenPos);
    void handleMouseLeave();
    void setWindowSemiTransparent(bool semiTransparent);
};
