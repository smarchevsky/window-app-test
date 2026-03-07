#include "VolumeApp.h"

#include "Utils.h"

#include "cassert"

void VolumeApp::handlePreLoop(WNDPROC winProc)
{
    WNDCLASSEXW winParam {
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = winProc,
        .hInstance = HINST_THISCOMPONENT,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = L"borderless-window",
    };

    RECT winRect;
    FileManager::get().loadWindowRect(winRect);
    Application::initWindow(winParam, winRect);

    setWindowSemiTransparent(true);
}

void VolumeApp::shutDown(HWND hWnd)
{
    RECT winRect;
    assert(hWnd == _hWnd);
    if (GetWindowRect(hWnd, &winRect))
        FileManager::get().saveWindowRect(winRect);

    DestroyWindow(hWnd);
}

void VolumeApp::handleMouseMove(POINT cursorScreenPos)
{
    if (!fMouseTracking) {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = _hWnd;
        if (TrackMouseEvent(&tme)) {
            fMouseTracking = true;
            setWindowSemiTransparent(false); // MOUSE ENTER

            POINT cursorClientPos = cursorScreenPos;
            ScreenToClient(_hWnd, &cursorClientPos);
            // auto newHoverInfo = sliderManager.getSelectAtPosition(cursorClientPos);
            // if (auto pSlider = sliderManager.getSliderFromSelect(newHoverInfo)) {
            //     pSlider->_focused = true;
            //     InvalidateRect(_hWnd, NULL, FALSE);
            // }
        }
    }
}

void VolumeApp::handleMouseLeave()
{
    fMouseTracking = false;
    setWindowSemiTransparent(true);
    // if (auto pSlider = sliderManager.getSliderFromSelect(sliderInfoHovered)) {
    //     pSlider->_focused = false;
    //     InvalidateRect(hWnd, NULL, FALSE);
    // }
}

void VolumeApp::setWindowSemiTransparent(bool semiTransparent)
{
    // LONG_PTR exStyle = GetWindowLongPtr(_hWnd, GWL_EXSTYLE);
    // SetWindowLongPtr(_hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED); // no need, if already layered
    SetLayeredWindowAttributes(_hWnd, 0, semiTransparent ? 200 : 255, LWA_ALPHA);
    printf("set window %s\n", semiTransparent ? "transparent" : "opaque");
}
