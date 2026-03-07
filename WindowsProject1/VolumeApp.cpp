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

    _sliderManager.appSliderAdd(1, 0.5f, false);
    _sliderManager.appSliderAdd(2, 0.5f, false);
    _sliderManager.appSliderAdd(3, 1.0f, false);
    _sliderManager.getSliderFromSelect(SelectInfo(VolumeType::Master, 0))->_val = 0.5f;
}

void VolumeApp::handleCloseWindow(HWND hWnd)
{
    assert(hWnd == _hWnd);

    RECT winRect;
    if (GetWindowRect(hWnd, &winRect))
        FileManager::get().saveWindowRect(winRect);

    App::handleCloseWindow(hWnd);
}

void VolumeApp::onResize(RECT newRect)
{
    // printf("onResize %d, %d, %d, %d\n", newRect.left, newRect.top, newRect.right, newRect.bottom);
    _sliderManager.recalculateSliderRects(newRect);
    InvalidateRect(_hWnd, NULL, FALSE);
}

void VolumeApp::setWindowSemiTransparent(bool semiTransparent)
{
    setWindowAlpha(semiTransparent ? 200 : 245);
    printf("set window %s\n", semiTransparent ? "transparent" : "opaque");
}

void VolumeApp::onPaint(HDC hdc)
{
    RECT windowRect;
    GetClientRect(_hWnd, &windowRect);
    FillRect(hdc, &windowRect, hBrushBackground);
    _sliderManager.drawSliders(hdc);
}

void VolumeApp::onMouseLeave()
{
    setWindowSemiTransparent(true);
    if (auto pSlider = _sliderManager.getSliderFromSelect(_sliderInfoHovered)) {
        pSlider->_focused = false;
        InvalidateRect(_hWnd, NULL, FALSE);
    }
}

void VolumeApp::onMouseMove(POINT screenPos, bool justEntered)
{
    if (justEntered)
        setWindowSemiTransparent(false);

    POINT clientPos = screenPos;
    ScreenToClient(_hWnd, &clientPos);
    auto newHoverInfo = _sliderManager.getSelectAtPosition(clientPos);
    if (newHoverInfo != _sliderInfoHovered) {
        if (auto pSlider = _sliderManager.getSliderFromSelect(newHoverInfo))
            pSlider->_focused = true;
        if (auto pSlider = _sliderManager.getSliderFromSelect(_sliderInfoHovered))
            pSlider->_focused = false;
        InvalidateRect(_hWnd, NULL, FALSE);
        // printf("changed from %d to %d\n", sliderInfoHovered._type, newHoverInfo._type);
        _sliderInfoHovered = newHoverInfo;
    }
}
