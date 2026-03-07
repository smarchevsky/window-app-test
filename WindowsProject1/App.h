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

class App {
protected:
    HWND _hWnd;

private:
    ATOM _cls;
    int _width, _height;
    RECT _rgn;
    bool _themeEnabled, _compositionEnabled;
    bool _mouseTracking;

protected:
    virtual void onMouseLeave() { }
    virtual void onMouseMove(POINT screenPos, bool justEntered) { }

public:
    bool compositionEnabled() const { return _compositionEnabled; }
    bool themeEnabled() const { return _themeEnabled; }
    void initWindow(const WNDCLASSEXW& winParam, RECT winRect);
    virtual void handleCloseWindow(HWND hWnd);
    virtual void shutDown();
    void updateRegion();
    bool handleKeydown(DWORD key);
    void handleMouseMove(POINT cursorScreenPos);
    void handleMouseLeave();

    bool hasAutohideAppbar(UINT edge, RECT mon);
    void handlePaint();
    void handleThemeChanged();
    void handleWindowPosChanged(const WINDOWPOS* pos);
    LRESULT handleMessageInvisible(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);

    void handleNCCalcSize(WPARAM wparam, LPARAM lparam);
    void handleNCCreate(HWND _hWnd, CREATESTRUCTW* cs) { SetWindowLongPtrW(_hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams); }
    LRESULT handleNCHitTest(int x, int y);
    void handleCompositionChanged();
    void setWindowAlpha(BYTE alpha);
};
