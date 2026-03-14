#include "VolumeApp.h"

HBRUSH hBrushBackground = CreateSolidBrush(HEX_TO_RGB(0x373F4E));

void VolumeApp::construct(HINSTANCE instance, WNDPROC wndProc)
{
    RECT rc;
    FileManager::get().loadWindowRect(rc);

    initWindow(instance, wndProc, rc);

    _audioAppListerner.init(_hWnd);
}

void VolumeApp::destroyWindow(HWND hWnd)
{
    if (_hWnd == hWnd) {
        _audioAppListerner.uninit();

        RECT rc;
        if (GetWindowRect(_hWnd, &rc)) {
            FileManager::get().saveWindowRect(rc);
        }

        DestroyWindow(_hWnd);
        _hWnd = nullptr;
    }
}

void VolumeApp::handleMMAppRegistered(WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    GetClientRect(_hWnd, &rc);

    AudioUpdateInfo info(wParam, lParam);
    sliderManager.appSliderAdd(info._pid, info._vol, info._isMuted);
    sliderManager.recalculateSliderRects(rc);

    InvalidateRect(_hWnd, NULL, FALSE);
}

void VolumeApp::handleMMAppUnegistered(WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    GetClientRect(_hWnd, &rc);

    AudioUpdateInfo info(wParam, lParam);
    sliderManager.appSliderRemove(info._pid);
    sliderManager.recalculateSliderRects(rc);

    InvalidateRect(_hWnd, NULL, FALSE);
    _audioAppListerner.cleanupExpiredSessions();
}

void VolumeApp::handleMMRefreshVol(WPARAM wParam, LPARAM lParam)
{
    AudioUpdateInfo info(wParam, lParam);
    SelectInfo si(info._type, info._pid);
    if (auto slider = sliderManager.getSliderFromSelect(si))
        slider->_val = info._vol;
    InvalidateRect(_hWnd, NULL, FALSE); // UpdateWindow(hWnd); // works without it
}

void VolumeApp::onPaint(HDC hdc)
{
    RECT windowRect;
    GetClientRect(_hWnd, &windowRect);

    FillRect(hdc, &windowRect, hBrushBackground);
    sliderManager.drawSliders(hdc);
}

void VolumeApp::onResize(RECT rc)
{
    sliderManager.recalculateSliderRects(rc);
    InvalidateRect(_hWnd, NULL, FALSE);
}

void VolumeApp::onMouseScroll(POINT cursorClientPos, float delta)
{
    auto hoverInfo = sliderManager.getSelectAtPosition(cursorClientPos);
    if (auto slider = sliderManager.getSliderFromSelect(hoverInfo)) {
        float sliderHeight = slider->getHeight();

        float oldVal = pow(slider->_val, .5f);
        float newVal = std::clamp(oldVal + delta / 16, 0.f, 1.f);
        newVal = pow(newVal, 2.f);

        if (newVal != oldVal) {
            _audioAppListerner.setVol(hoverInfo, newVal);
        }
    }
}

void VolumeApp::onMouseLeave()
{
    setWindowAlpha(200);
    if (auto pSlider = sliderManager.getSliderFromSelect(sliderInfoHovered)) {
        pSlider->_focused = false;
        InvalidateRect(_hWnd, NULL, FALSE);
    }
    sliderInfoHovered = {};
}

void VolumeApp::onMouseMove(POINT cursorClientPos, bool justEntered)
{
    if (justEntered)
        setWindowAlpha(255);

    SelectInfo newHoverInfo = sliderManager.getSelectAtPosition(cursorClientPos);
    if (newHoverInfo != sliderInfoHovered) {
        if (auto pSlider = sliderManager.getSliderFromSelect(newHoverInfo))
            pSlider->_focused = true;
        if (auto pSlider = sliderManager.getSliderFromSelect(sliderInfoHovered))
            pSlider->_focused = false;
        sliderInfoHovered = newHoverInfo;

        InvalidateRect(_hWnd, NULL, FALSE);
        // printf("changed from %d to %d\n", sliderInfoHovered._type, newHoverInfo._type);
    }
}
