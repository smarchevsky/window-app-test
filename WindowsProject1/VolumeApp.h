#pragma once

#include "App.h"

#include "Utils.h"

class VolumeApp : public App {
    SliderManager _sliderManager;
    SelectInfo _sliderInfoHovered;

public:
    void startup(WNDPROC winProc);
    void handleCloseWindow(HWND hWnd) override;
    virtual void onResize(RECT clientRect) override;

private:
    void setWindowSemiTransparent(bool semiTransparent);

    virtual void onPaint(HDC hdc) override;
    virtual void onMouseLeave() override;
    virtual void onMouseMove(POINT screenPos, bool justEntered) override;
};
