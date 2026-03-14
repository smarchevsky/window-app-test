#pragma once

#include <Windows.h>
#include <windowsx.h>

#include <algorithm>

#define HEX_TO_RGB(hex) RGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF)

// darks  0x0A0E15 0x212631 0x373F4E 0x4E576A 0x667085
// lights 0xBFC6D4 0xD1d6E9 0xE0E4EB 0xF0F1F5 0xFFFFFF

class App {
protected:
    HINSTANCE _hInstance;
    HWND _hWnd;

private:
    bool _mouseTracking;

protected:
    virtual void onMouseLeave() = 0;
    virtual void onMouseMove(POINT cursorClientPos, bool justEntered) = 0;
    virtual void onMouseScroll(POINT cursorClientPos, float delta) = 0;

    virtual void onPaint(HDC hdc) = 0;
    virtual void onResize(RECT newRect) = 0;

public:
    virtual void initWindow(HINSTANCE instance, WNDPROC wndProc, RECT rc);
    virtual void destroyWindow(HWND hWnd);

    HINSTANCE getInstance() { return _hInstance; }

    virtual void handleResize(WPARAM wParam, LPARAM lParam);

    void handleMouseMove(WPARAM wParam, LPARAM lParam);
    void handleMouseLeave();
    void handleMouseScroll(WPARAM wParam, LPARAM lParam);

    void handlePaint();

    void setWindowAlpha(BYTE alpha);

public: // INHERIT THIS
    LRESULT handleNCAHitTest(HWND hWnd, LPARAM lParam);
};