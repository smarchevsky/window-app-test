#pragma once

#include "App.h"

class VolumeApp : public App {
public:
    void handlePreLoop(WNDPROC winProc);
    void handleCloseWindow(HWND hWnd) override;

private:
    void setWindowSemiTransparent(bool semiTransparent);

    virtual void onMouseLeave() override;
    virtual void onMouseMove(POINT screenPos, bool justEntered) override;
};
