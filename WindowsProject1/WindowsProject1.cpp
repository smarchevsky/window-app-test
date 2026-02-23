// WindowsProject1.cpp : Defines the entry point for the application.
//

#include "WindowsProject1.h"
#include "framework.h"

#include "AudioUtils.h"

#include <iostream>
#include <string>

#define MAX_LOADSTRING 100

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

HINSTANCE hInst; // current instance
WCHAR szTitle[MAX_LOADSTRING]; // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

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

HWND g_labelVolMaster;
HWND g_labelVolApps;
HBRUSH hBrush = CreateSolidBrush(RGB(100, 100, 250));

void drawSquareCodeExample(HDC hdc)
{
    RECT rect { 000, 000, 600, 600 };
    FillRect(hdc, &rect, hBrush);

    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, L"Button", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    CreateConsole();

    CoinitializeWrapper coinitializeRAII;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSPROJECT1, szWindowClass, MAX_LOADSTRING);

    // CREATING WINDOW
    HWND hWnd;
    {
        WNDCLASSEXW wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
        wcex.lpszClassName = szWindowClass;
        wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
        RegisterClassExW(&wcex);

        hInst = hInstance;
        hWnd = CreateWindowExW(WS_EX_TOPMOST, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
            650, 200, 800, 600, nullptr, nullptr, hInstance, nullptr);
        if (!hWnd)
            return FALSE;

        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }

    PostMessage(hWnd, WM_PAINT, 0, 0); // to draw square initially!

    g_labelVolMaster = CreateWindowW(L"STATIC", L"Volume: 0%", WS_VISIBLE | WS_CHILD,
        10, 10, 600, 100, hWnd, nullptr, hInstance, nullptr);

    g_labelVolApps = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD,
        10, 140, 600, 600, hWnd, nullptr, hInstance, nullptr);

    ListenerAudio_MasterVolume::get().init(hWnd);
    ListenerAudio_AllApplications::get().init(hWnd);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_REFRESH_MASTER_VOL: {
        SetWindowTextW(g_labelVolMaster, ListenerAudio_MasterVolume::get().getInfo().c_str());
        return 0;
    }

    case WM_REFRESH_VOLUMES:
        SetWindowTextW(g_labelVolApps, ListenerAudio_AllApplications::get().getInfo().c_str());
        return 0;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId) {

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        drawSquareCodeExample(hdc);
        EndPaint(hWnd, &ps);
    } break;

    case WM_DESTROY:
        ListenerAudio_MasterVolume::get().uninit();
        ListenerAudio_AllApplications::get().uninit();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
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
