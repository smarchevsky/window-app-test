#include "App.h"

#include "Resource.h"
// #include <cassert>

HCURSOR cursorDefault = LoadCursor(nullptr, IDC_ARROW);
HCURSOR cursorHand = LoadCursor(nullptr, IDC_HAND);

#define MAX_LOADSTRING 100
WCHAR szTitle[MAX_LOADSTRING]; // title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // main window class name

void App::initWindow(HINSTANCE instance, WNDPROC wndProc, RECT rc)
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
    // wcex.hbrBackground = hBrushBackground;
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    RegisterClassExW(&wcex);

    _hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        szWindowClass, szTitle,
        WS_POPUP | WS_VISIBLE,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, _hInstance, nullptr);

    setWindowAlpha(200);
}

void App::destroyWindow(HWND hWnd)
{
    if (_hWnd == hWnd) {
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
    // printf("cursorScreenPos %s %d, %d\n", (justEntered ? "entered" : ""), cursorScreenPos.x, cursorScreenPos.y);
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

    // SetLayeredWindowAttributes(_hWnd, RGB(255, 0, 255), alpha, LWA_COLORKEY | LWA_ALPHA); // get rid of white line at the top
     SetLayeredWindowAttributes(_hWnd, 0, alpha, LWA_ALPHA);
}

// INHERIT BELOW

void App::handleResize(WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    GetClientRect(_hWnd, &rc);
    onResize(rc);
}

// clang-format off
const int BORDER = 8;
LRESULT App::handleNCAHitTest(HWND hWnd, LPARAM lParam)
{
    if(hWnd == _hWnd) {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc;
        GetWindowRect(_hWnd, &rc);
        bool left = pt.x < rc.left + BORDER;
        bool right = pt.x >= rc.right - BORDER;
        bool top = pt.y < rc.top + BORDER;
        bool bottom = pt.y >= rc.bottom - BORDER;
        if (top && left)     return HTTOPLEFT;
        if (top && right)    return HTTOPRIGHT;
        if (bottom && left)  return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (top)             return HTTOP;
        if (bottom)          return HTBOTTOM;
        if (left)            return HTLEFT;
        if (right)           return HTRIGHT;
        return HTCLIENT; // drag to move anywhere else
    }
}
