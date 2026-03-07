// WindowsProject1.cpp : Defines the entry point for the application.
//

#include "WindowsProject1.h"
#include "framework.h"

#include "Application.h"

#include <algorithm>
#include <iostream>
#include <string>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#define MAX_LOADSTRING 100
#define HEX_TO_RGB(hex) RGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF)

void CreateConsole()
{
    AllocConsole();
    SetConsoleTitleW(L"Console title");
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    std::ios::sync_with_stdio();
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

Application app;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    CreateConsole();
    return (int)app.init(WndProc);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DWMCOMPOSITIONCHANGED:
        app.handleCompositionChanged();
        return 0;
    case WM_KEYDOWN:
        if (app.handleKeydown((DWORD)wParam))
            return 0;
        break;
    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_NCACTIVATE:
        return DefWindowProcW(hWnd, msg, wParam, -1);
    case WM_NCCALCSIZE:
        app.handleNCCalcSize(wParam, lParam);
        return 0;
    case WM_NCHITTEST:
        return app.handleNCHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    case WM_NCPAINT:
        if (!app.compositionEnabled())
            return 0;
        break;
    case WM_NCUAHDRAWCAPTION: // These undocumented messages are sent to draw themed hWnd borders.
    case WM_NCUAHDRAWFRAME: //   Block them to prevent drawing borders over the client area.
        return 0;
    case WM_PAINT:
        app.handlePaint();
        return 0;
    case WM_THEMECHANGED:
        app.handleThemeChanged();
        break;
    case WM_WINDOWPOSCHANGED:
        app.handleWindowPosChanged((WINDOWPOS*)lParam);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
