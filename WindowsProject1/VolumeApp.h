#pragma once

#include "App.h"

#include "AudioUtils.h"
#include "Utils.h"

class VolumeApp : public App {
    AudioUpdateListener _audioAppListerner;
    SliderManager sliderManager;
    SelectInfo sliderInfoHovered;

    virtual void onPaint(HDC hdc) override;
    virtual void onResize(RECT newRect) override;

    virtual void onMouseLeave() override;
    virtual void onMouseMove(POINT cursorClientPos, bool justEntered) override;
    virtual void onMouseScroll(POINT cursorClientPos, float delta) override;

public:
    void handleMMAppRegistered(WPARAM wParam, LPARAM lParam);
    void handleMMAppUnegistered(WPARAM wParam, LPARAM lParam);
    void handleMMRefreshVol(WPARAM wParam, LPARAM lParam);

    virtual void construct(HINSTANCE instance, WNDPROC wndProc);
    virtual void destroyWindow(HWND hWnd) override;
    virtual void cleanup();
};
