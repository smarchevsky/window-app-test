#include "Application.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

WPARAM Application::init(WNDPROC proc)
{
    WNDCLASSEXW winParam {
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = proc,
        .hInstance = HINST_THISCOMPONENT,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = L"borderless-window",
    };

    ATOM cls = RegisterClassExW(&winParam);

    _hWnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_LAYERED,
        (LPWSTR)MAKEINTATOM(cls),
        L"Borderless Window",
        WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 200, 200,
        NULL, NULL, HINST_THISCOMPONENT, NULL);

    SetLayeredWindowAttributes(_hWnd, RGB(255, 0, 255), 0, LWA_COLORKEY);

    handleCompositionChanged();
    handleThemeChanged();
    ShowWindow(_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(_hWnd);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnregisterClassW((LPWSTR)MAKEINTATOM(cls), HINST_THISCOMPONENT);
    return message.wParam;
}

void Application::updateRegion()
{
    RECT old_rgn = _rgn;
    if (IsMaximized(_hWnd)) {
        WINDOWINFO wi = { .cbSize = sizeof wi };
        GetWindowInfo(_hWnd, &wi);
        _rgn = {
            .left = wi.rcClient.left - wi.rcWindow.left,
            .top = wi.rcClient.top - wi.rcWindow.top,
            .right = wi.rcClient.right - wi.rcWindow.left,
            .bottom = wi.rcClient.bottom - wi.rcWindow.top,
        };
    } else if (!_compositionEnabled) {
        _rgn = { .left = 0, .top = 0, .right = 32767, .bottom = 32767 };
    } else {
        _rgn = {};
    }

    if (EqualRect(&_rgn, &old_rgn))
        return;

    static const RECT zeroRect = {};
    if (EqualRect(&_rgn, &zeroRect)) {
        SetWindowRgn(_hWnd, NULL, TRUE);
    } else {
        SetWindowRgn(_hWnd, CreateRectRgnIndirect(&_rgn), TRUE);
    }
}

bool Application::handleKeydown(DWORD key)
{
    switch (key) {
    case 'I': {
        static bool icon_toggle;
        static HICON iconError = LoadIcon(NULL, IDI_ERROR);
        static HICON iconExclam = LoadIcon(NULL, IDI_EXCLAMATION);
        icon_toggle = !icon_toggle;
        SendMessageW(_hWnd, WM_SETICON, ICON_BIG, (LPARAM)(icon_toggle ? iconError : iconExclam));
        return true;
    }
    case 'T': {
        static bool text_toggle;
        if (text_toggle)
            SetWindowTextW(_hWnd, L"window text");
        else
            SetWindowTextW(_hWnd, L"txet wodniw");
        text_toggle = !text_toggle;

        return true;
    }
    case 'M': {
        static bool menu_toggle;
        HMENU menu = GetSystemMenu(_hWnd, FALSE);
        if (menu_toggle)
            EnableMenuItem(menu, SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
        else
            EnableMenuItem(menu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
        menu_toggle = !menu_toggle;

        return true;
    }
    default:
        return false;
    }
}

bool Application::hasAutohideAppbar(UINT edge, RECT mon)
{
    APPBARDATA tmp;
    tmp = { .cbSize = sizeof(APPBARDATA), .uEdge = edge, .rc = mon };
    return SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &tmp);
}

void Application::handlePaint()
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(_hWnd, &ps);
    HBRUSH bb = CreateSolidBrush(RGB(0, 255, 0));

    RECT rc = { 0, 0, 1, _height };
    FillRect(dc, &rc, bb);
    rc = { 0, 0, _width, 1 };
    FillRect(dc, &rc, bb);
    rc = { _width - 1, 0, _width, _height };
    FillRect(dc, &rc, bb);
    rc = { 0, _height - 1, _width, _height };
    FillRect(dc, &rc, bb);

    DeleteObject(bb);
    EndPaint(_hWnd, &ps);
}

void Application::handleThemeChanged()
{
    _themeEnabled = IsThemeActive();
}

void Application::handleWindowPosChanged(const WINDOWPOS* pos)
{
    RECT client;
    GetClientRect(_hWnd, &client);
    int old_width = _width;
    int old_height = _height;
    _width = client.right;
    _height = client.bottom;
    bool client_changed = _width != old_width || _height != old_height;

    if (client_changed || (pos->flags & SWP_FRAMECHANGED))
        updateRegion();

    if (client_changed) {
        RECT rc;
        if (_width > old_width) {
            rc = { old_width - 1, 0, old_width, old_height };
            InvalidateRect(_hWnd, &rc, TRUE);
        } else {
            rc = { _width - 1, 0, _width, _height };
            InvalidateRect(_hWnd, &rc, TRUE);
        }
        if (_height > old_height) {
            rc = { 0, old_height - 1, old_width, old_height };
            InvalidateRect(_hWnd, &rc, TRUE);
        } else {
            rc = { 0, _height - 1, _width, _height };
            InvalidateRect(_hWnd, &rc, TRUE);
        }
    }
}

LRESULT Application::handleMessageInvisible(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LONG_PTR old_style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    SetWindowLongPtrW(hWnd, GWL_STYLE, old_style & ~WS_VISIBLE);
    LRESULT result = DefWindowProcW(hWnd, msg, wparam, lparam);
    SetWindowLongPtrW(hWnd, GWL_STYLE, old_style);
    return result;
}

void Application::handleNCCalcSize(WPARAM wparam, LPARAM lparam)
{
    union {
        LPARAM lparam;
        RECT* rect;
    } params = { .lparam = lparam };

    if (params.rect == nullptr)
        return;

    RECT nonclient = *params.rect;
    DefWindowProcW(_hWnd, WM_NCCALCSIZE, wparam, params.lparam);
    RECT client = *params.rect;

    if (IsMaximized(_hWnd)) {
        WINDOWINFO wi = { .cbSize = sizeof wi };
        GetWindowInfo(_hWnd, &wi);

        *params.rect = { .left = client.left, .top = nonclient.top + (int)wi.cyWindowBorders, .right = client.right, .bottom = client.bottom };

        HMONITOR mon = MonitorFromWindow(_hWnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { .cbSize = sizeof mi };
        GetMonitorInfoW(mon, &mi);

        if (EqualRect(params.rect, &mi.rcMonitor)) {
            if (hasAutohideAppbar(ABE_BOTTOM, mi.rcMonitor))
                params.rect->bottom--;
            else if (hasAutohideAppbar(ABE_LEFT, mi.rcMonitor))
                params.rect->left++;
            else if (hasAutohideAppbar(ABE_TOP, mi.rcMonitor))
                params.rect->top++;
            else if (hasAutohideAppbar(ABE_RIGHT, mi.rcMonitor))
                params.rect->right--;
        }
    } else {
        *params.rect = nonclient;
    }
}

LRESULT Application::handleNCHitTest(int x, int y)
{
    if (IsMaximized(_hWnd))
        return HTCLIENT;

    POINT mouse = { x, y };
    ScreenToClient(_hWnd, &mouse);

    int frame_size = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
    int diagonal_width = frame_size * 2 + GetSystemMetrics(SM_CXBORDER);

    if (mouse.y < frame_size) {
        if (mouse.x < diagonal_width)
            return HTTOPLEFT;
        if (mouse.x >= _width - diagonal_width)
            return HTTOPRIGHT;
        return HTTOP;
    }

    if (mouse.y >= _height - frame_size) {
        if (mouse.x < diagonal_width)
            return HTBOTTOMLEFT;
        if (mouse.x >= _width - diagonal_width)
            return HTBOTTOMRIGHT;
        return HTBOTTOM;
    }

    if (mouse.x < frame_size)
        return HTLEFT;
    if (mouse.x >= _width - frame_size)
        return HTRIGHT;
    return HTCLIENT;
}

void Application::handleCompositionChanged()
{
    BOOL enabled = FALSE;
    DwmIsCompositionEnabled(&enabled);
    _compositionEnabled = enabled;

    if (enabled) {
        static const MARGINS margins { 0, 0, 1, 0 };
        static const DWORD pvAttribute = DWMNCRP_ENABLED;
        DwmExtendFrameIntoClientArea(_hWnd, &margins);
        DwmSetWindowAttribute(_hWnd, DWMWA_NCRENDERING_POLICY,
            &pvAttribute, sizeof(DWORD));
    }

    updateRegion();
}
