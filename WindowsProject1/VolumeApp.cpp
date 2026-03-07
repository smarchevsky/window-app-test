#include "VolumeApp.h"

#include "Utils.h"

#include "cassert"

#define HEX_TO_RGB(hex) RGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF)
HBRUSH hBrushBackground = CreateSolidBrush(HEX_TO_RGB(0x373F4E));

void VolumeApp::startup(WNDPROC winProc)
{
    WNDCLASSEXW winParam {
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = winProc,
        .hInstance = HINST_THISCOMPONENT,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = hBrushBackground,
        .lpszClassName = L"borderless-window",
    };

    RECT winRect;
    FileManager::get().loadWindowRect(winRect);
    App::initWindow(winParam, winRect);

    setWindowSemiTransparent(true);
}

void VolumeApp::handleCloseWindow(HWND hWnd)
{
    assert(hWnd == _hWnd);

    RECT winRect;
    if (GetWindowRect(hWnd, &winRect))
        FileManager::get().saveWindowRect(winRect);

    App::handleCloseWindow(hWnd);
}

void VolumeApp::setWindowSemiTransparent(bool semiTransparent)
{
    setWindowAlpha(semiTransparent ? 200 : 245);
    printf("set window %s\n", semiTransparent ? "transparent" : "opaque");
}

void VolumeApp::onMouseLeave()
{
    setWindowSemiTransparent(true);
    // if (auto pSlider = sliderManager.getSliderFromSelect(sliderInfoHovered)) {
    //     pSlider->_focused = false;
    //     InvalidateRect(hWnd, NULL, FALSE);
    // }
}

void VolumeApp::onMouseMove(POINT screenPos, bool justEntered)
{
    if (justEntered)
        setWindowSemiTransparent(false);

    // POINT cursorClientPos = cursorScreenPos;
    // ScreenToClient(_hWnd, &cursorClientPos);
    // auto newHoverInfo = sliderManager.getSelectAtPosition(cursorClientPos);
    // if (auto pSlider = sliderManager.getSliderFromSelect(newHoverInfo)) {
    //     pSlider->_focused = true;
    //     InvalidateRect(_hWnd, NULL, FALSE);
    // }
}
