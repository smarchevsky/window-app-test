#pragma once

#include <Windows.h>
#include <windowsx.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE) & __ImageBase)

#ifndef WM_NCUAHDRAWCAPTION
#define WM_NCUAHDRAWCAPTION (0x00AE)
#endif
#ifndef WM_NCUAHDRAWFRAME
#define WM_NCUAHDRAWFRAME (0x00AF)
#endif

class Application {
    HWND _hWnd;
    int _width, _height;
    RECT _rgn;
    bool _themeEnabled, _compositionEnabled;

public:
    WPARAM init(WNDPROC proc);
    void updateRegion();
    bool themeEnabled() const { return _themeEnabled; }
    bool compositionEnabled() const { return _compositionEnabled; }
    bool handleKeydown(DWORD key);
    bool hasAutohideAppbar(UINT edge, RECT mon);
    void handlePaint();
    void handleThemeChanged();
    void handleWindowPosChanged(const WINDOWPOS* pos);

    void handleNCCalcSize(WPARAM wparam, LPARAM lparam);
    void handleNCCreate(HWND _hWnd, CREATESTRUCTW* cs) { SetWindowLongPtrW(_hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams); }
    LRESULT handleNCHitTest(int x, int y);
    void handleCompositionChanged();
};
