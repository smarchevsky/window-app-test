#include "App.h"

#include "Resource.h"
#include <cassert>

HCURSOR cursorDefault = LoadCursor(nullptr, IDC_ARROW);
HCURSOR cursorHand = LoadCursor(nullptr, IDC_HAND);

#define MAX_LOADSTRING 100
WCHAR szTitle[MAX_LOADSTRING]; // title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // main window class name

void App::initWindow(HINSTANCE instance, WNDPROC wndProc, int nCmdShow)
{
    _hInstance = instance;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    LoadStringW(_hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(_hInstance, IDC_WINDOWSPROJECT1, szWindowClass, MAX_LOADSTRING);

    WNDCLASSEXW wcex {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = wndProc;
    wcex.hInstance = _hInstance;
    wcex.hIcon = LoadIcon(_hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
    wcex.hCursor = cursorDefault;
    wcex.hbrBackground = hBrushBackground;
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    RegisterClassExW(&wcex);

    // WS_POPUP: Removes the title bar
    // WS_THICKFRAME: Adds the invisible resizing borders
    // WS_SYSMENU: Keeps it on the taskbar/system integration
    // was WS_OVERLAPPEDWINDOW

    // DWORD dwStyle = WS_POPUP | WS_THICKFRAME | WS_SYSMENU;

    RECT rc;
    FileManager::get().loadWindowRect(rc);

    _hWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, _hInstance, nullptr);

    setWindowAlpha(200);

    _audioAppListerner.init(_hWnd);

    // ShowWindow(_hWnd, SW_SHOWDEFAULT);
    ShowWindow(_hWnd, nCmdShow);
    UpdateWindow(_hWnd);
}

void App::handleDestroy(HWND hWnd)
{
    if (_hWnd) {
        _audioAppListerner.uninit();
        assert(hWnd == _hWnd);
        RECT winRect;
        if (GetWindowRect(_hWnd, &winRect))
            FileManager::get().saveWindowRect(winRect);

        DestroyWindow(_hWnd);
        _hWnd = nullptr;
    }
}

void App::handleMouseLeave()
{
    _mouseTracking = false;
    onMouseLeave();
}

void App::handleMouseScroll(WPARAM wParam, LPARAM lParam)
{
    int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    float wheelSteps = (float)zDelta / WHEEL_DELTA;
    POINT cursorScreenPos { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ScreenToClient(_hWnd, &cursorScreenPos);
    onMouseScroll(cursorScreenPos, wheelSteps);
}

void App::handleMouseMove(WPARAM wParam, LPARAM lParam)
{
    bool justEntered = false;
    if (!_mouseTracking) {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = _hWnd;
        if (TrackMouseEvent(&tme)) {
            _mouseTracking = true;
            justEntered = true;
        }
    }

    POINT cursorScreenPos { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ScreenToClient(_hWnd, &cursorScreenPos);
    onMouseMove(cursorScreenPos, justEntered);
}

void App::handlePaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(_hWnd, &ps);
    onPaint(hdc);
    EndPaint(_hWnd, &ps);
}

void App::setWindowAlpha(BYTE alpha)
{
    // LONG_PTR exStyle = GetWindowLongPtr(_hWnd, GWL_EXSTYLE);
    // SetWindowLongPtr(_hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED); // no need, if already layered

    SetLayeredWindowAttributes(_hWnd, RGB(255, 0, 255), alpha, LWA_COLORKEY | LWA_ALPHA); // get rid of white line at the top
    // SetLayeredWindowAttributes(_hWnd, 0, alpha, LWA_ALPHA);
}

// INHERIT BELOW

void App::onMouseLeave()
{
    setWindowAlpha(200);
    if (auto pSlider = sliderManager.getSliderFromSelect(sliderInfoHovered)) {
        pSlider->_focused = false;
        InvalidateRect(_hWnd, NULL, FALSE);
    }
}

void App::onMouseMove(POINT cursorClientPos, bool justEntered)
{
    if (justEntered)
        setWindowAlpha(255);

    SelectInfo newHoverInfo = sliderManager.getSelectAtPosition(cursorClientPos);
    if (newHoverInfo != sliderInfoHovered) {
        if (auto pSlider = sliderManager.getSliderFromSelect(newHoverInfo))
            pSlider->_focused = true;
        if (auto pSlider = sliderManager.getSliderFromSelect(sliderInfoHovered))
            pSlider->_focused = false;
        InvalidateRect(_hWnd, NULL, FALSE);
        // printf("changed from %d to %d\n", sliderInfoHovered._type, newHoverInfo._type);
        sliderInfoHovered = newHoverInfo;
    }
}

void App::onMouseScroll(POINT cursorClientPos, float delta)
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

void App::handleResize(WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    GetClientRect(_hWnd, &rc);
    sliderManager.recalculateSliderRects(rc);
    InvalidateRect(_hWnd, NULL, FALSE);
}

void App::handleMMAppRegistered(WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    GetClientRect(_hWnd, &rc);

    AudioUpdateInfo info(wParam, lParam);
    sliderManager.appSliderAdd(info._pid, info._vol, info._isMuted);
    sliderManager.recalculateSliderRects(rc);

    InvalidateRect(_hWnd, NULL, FALSE);
}

void App::handleMMAppUnegistered(WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    GetClientRect(_hWnd, &rc);

    AudioUpdateInfo info(wParam, lParam);
    sliderManager.appSliderRemove(info._pid);
    sliderManager.recalculateSliderRects(rc);

    InvalidateRect(_hWnd, NULL, FALSE);
    _audioAppListerner.cleanupExpiredSessions();
}

void App::handleMMRefreshVol(WPARAM wParam, LPARAM lParam)
{
    AudioUpdateInfo info(wParam, lParam);
    SelectInfo si(info._type, info._pid);
    if (auto slider = sliderManager.getSliderFromSelect(si))
        slider->_val = info._vol;
    InvalidateRect(_hWnd, NULL, FALSE); // UpdateWindow(hWnd); // works without it
}
