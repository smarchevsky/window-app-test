// WindowsProject1.cpp : Defines the entry point for the application.
//

#include "WindowsProject1.h"
#include "framework.h"

#include "AudioUtils.h"

#include <algorithm>
#include <iostream>
#include <string>

#define MAX_LOADSTRING 100

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

HINSTANCE hInst; // current instance
WCHAR szTitle[MAX_LOADSTRING]; // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

HBRUSH hBrushSlider = CreateSolidBrush(RGB(100, 100, 250));
HCURSOR cursorDefault = LoadCursor(nullptr, IDC_ARROW);
HCURSOR cursorHand = LoadCursor(nullptr, IDC_HAND);

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
        wcex.hCursor = cursorDefault;
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = szWindowClass;
        wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
        RegisterClassExW(&wcex);

        hInst = hInstance;
        // WS_POPUP: Removes the title bar
        // WS_THICKFRAME: Adds the invisible resizing borders
        // WS_SYSMENU: Keeps it on the taskbar/system integration
        // was WS_OVERLAPPEDWINDOW

        DWORD dwStyle = WS_POPUP | WS_THICKFRAME | WS_SYSMENU;
        hWnd = CreateWindowExW(WS_EX_TOPMOST, szWindowClass, szTitle, dwStyle,
            650, 200, 800, 400, nullptr, nullptr, hInstance, nullptr);
        if (!hWnd)
            return FALSE;

        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }

    // PostMessage(hWnd, WM_PAINT, 0, 0); // to draw square initially!

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

HDC memDC;
HBITMAP memBitmap;

HWND hLabel;

bool isDownLMB = false;
POINT cursorScreenPosCaptured;
LONG cursorOffsetAccumulatorY;

SliderManager sliderManager;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE: {
        memDC = CreateCompatibleDC(nullptr);
        // hLabel = CreateWindow(L"STATIC", L"Drag to move: X: 0, Y: 0",
        //     WS_VISIBLE | WS_CHILD, 20, 20, 300, 20, hWnd, NULL, NULL, NULL);

        AudioUpdateListener::get().init(hWnd);
        break;
    }

    case WM_LBUTTONDOWN: {
        SetCapture(hWnd);
        ShowCursor(FALSE);

        RECT rect;
        GetClientRect(hWnd, &rect);

        POINT cursorScreenPos, cursorClientPos;
        GetCursorPos(&cursorScreenPos);

        isDownLMB = true;
        cursorScreenPosCaptured = cursorScreenPos;

        cursorClientPos = cursorScreenPos;
        ScreenToClient(hWnd, &cursorClientPos);

        SetTimer(hWnd, IDT_TIMER_1, 25, (TIMERPROC)NULL);
        break;
    }

    case WM_LBUTTONUP: {
        if (isDownLMB) {
            isDownLMB = false;
            ReleaseCapture();
            ShowCursor(TRUE);
            KillTimer(hWnd, IDT_TIMER_1);
        }
        cursorOffsetAccumulatorY = 0;
        break;
    }

    case WM_MOUSEMOVE: {
        POINT cursorScreenPos;
        GetCursorPos(&cursorScreenPos);

        if (isDownLMB) {
            int dx = cursorScreenPos.x - cursorScreenPosCaptured.x;
            int dy = cursorScreenPos.y - cursorScreenPosCaptured.y;

            if (dx || dy) {
                cursorOffsetAccumulatorY -= dy;
                SetCursorPos(cursorScreenPosCaptured.x, cursorScreenPosCaptured.y);
            }

        } else {
            POINT cursorClientPos = cursorScreenPos;
            ScreenToClient(hWnd, &cursorClientPos);
            auto newHoverInfo = sliderManager.getHoveredSlider(cursorClientPos);
            SetCursor(newHoverInfo._valid ? cursorHand : cursorDefault);
        }
        break;
    }

    case WM_TIMER: {
        if (wParam == IDT_TIMER_1) {
            if (cursorOffsetAccumulatorY) {
                auto& masterSlider = sliderManager.getMaster();
                float sliderHeight = masterSlider.getHeight();
                float val = masterSlider.getValue() + (float)cursorOffsetAccumulatorY / sliderHeight;
                // ListenerAudio_MasterVolume::get().setValue(std::clamp(val, 0.f, 1.f));
                cursorOffsetAccumulatorY = 0;
            }
        }
    } break;

    case WM_APP_REGISTERED: {
        AudioUpdateInfo info(wParam, lParam);
        sliderManager.addAppSlider(info._pid, info._vol, info._isMuted);
        sliderManager.recalculateSliderRects(hWnd);
        InvalidateRect(hWnd, NULL, TRUE);
    } break;

    case WM_APP_UNREGISTERED: {
        AudioUpdateInfo info(wParam, lParam);
        sliderManager.removeAppSlider(info._pid);
        sliderManager.recalculateSliderRects(hWnd);
        InvalidateRect(hWnd, NULL, TRUE);
    } break;

    case WM_REFRESH_VOL: {
        AudioUpdateInfo info(wParam, lParam);
        if (info._type == VolumeType::Master)
            sliderManager.getMaster().setValue(info._vol);
        else if (info._type == VolumeType::App) {
            sliderManager.setSliderValue(info._pid, info._vol, info._isMuted);
        }

        InvalidateRect(hWnd, NULL, TRUE); // UpdateWindow(hWnd); // works without it
        return 0;
    }

    case WM_DESTROY: {
        AudioUpdateListener::get().uninit();
        IconManager::get().uninit();

        DeleteObject(memBitmap);
        DeleteDC(memDC);

        PostQuitMessage(0);
    } break;

    case WM_SIZE: {
        if (memBitmap)
            DeleteObject(memBitmap);

        HDC hdc = GetDC(hWnd);
        memBitmap = CreateCompatibleBitmap(hdc, LOWORD(lParam), HIWORD(lParam));
        ReleaseDC(hWnd, hdc);

        sliderManager.recalculateSliderRects(hWnd);
    } break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        FillRect(memDC, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        sliderManager.drawSliders(memDC);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBitmap);

        EndPaint(hWnd, &ps);
    } break;

    case WM_ERASEBKGND: {
        return 1; // no redraw bkg
    }

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
