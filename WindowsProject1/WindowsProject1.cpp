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

HBRUSH hBrushSlider = CreateSolidBrush(RGB(100, 100, 250));

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

std::vector<AppAudioInfo> appInfos;

CustomSlider sliderMasterVol;
std::vector<CustomSlider> slidersAppVol;

HDC memDC;
HBITMAP memBitmap;

int totalX = 0, totalY = 0;
HWND hLabel;

bool isDragging = false;
POINT capturedCursorScreenPos;

void recalculateSliderRects(HWND hWnd)
{
    RECT windowRect;
    GetClientRect(hWnd, &windowRect);
    LONG windowHeight = windowRect.bottom;

    sliderMasterVol.setRect({ 0, 0, sliderWidth, windowHeight });
    int offset = sliderWidth + 30;
    for (auto& slider : slidersAppVol) {
        slider.setRect({ offset, 0, offset + sliderWidth, windowHeight });
        offset += sliderWidth;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE: {
        memDC = CreateCompatibleDC(nullptr);
        hLabel = CreateWindow(L"STATIC", L"Drag to move: X: 0, Y: 0",
            WS_VISIBLE | WS_CHILD, 20, 20, 300, 20, hWnd, NULL, NULL, NULL);
        ListenerAudio_MasterVolume::get().init(hWnd);
        ListenerAudio_AllApplications::get().init(hWnd);
        break;
    }

    case WM_LBUTTONDOWN: {
        SetCapture(hWnd);
        ShowCursor(FALSE);

        RECT rect;
        GetClientRect(hWnd, &rect);

        POINT cursorScreenPos, cursorClientPos;
        GetCursorPos(&cursorScreenPos);

        isDragging = true;
        capturedCursorScreenPos = cursorScreenPos;

        cursorClientPos = cursorScreenPos;
        ScreenToClient(hWnd, &cursorClientPos);

        float param;
        if (sliderMasterVol.intersects(cursorClientPos, param)) {
            SetWindowText(hLabel, (L"Master captured: " + std::to_wstring(param)).c_str());
            ListenerAudio_MasterVolume::get().setValue(param);
        } else {
            SetWindowText(hLabel, L"Captured another");
        }

        break;
    }

    case WM_LBUTTONUP: {
        if (isDragging) {
            isDragging = false;
            ReleaseCapture();
            ShowCursor(TRUE);
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (isDragging) {
            POINT currentPos;
            GetCursorPos(&currentPos);

            int dx = currentPos.x - capturedCursorScreenPos.x;
            int dy = currentPos.y - capturedCursorScreenPos.y;

            if (dx != 0 || dy != 0) {
                totalX += dx;
                totalY += dy;

                std::wstring text = L"X: " + std::to_wstring(totalX) + L", Y: " + std::to_wstring(totalY);
                SetWindowText(hLabel, text.c_str());

                SetCursorPos(capturedCursorScreenPos.x, capturedCursorScreenPos.y);
            }
        }
        break;
    }

    case WM_REFRESH_MASTER_VOL: {
        sliderMasterVol.setValue(ListenerAudio_MasterVolume::get().getValue());
        InvalidateRect(hWnd, NULL, TRUE); // UpdateWindow(hWnd); // works without it
        return 0;
    }

    case WM_REFRESH_APP_VOLUMES: {
        ListenerAudio_AllApplications::get().getInfo(appInfos);

        slidersAppVol.resize(0);
        for (auto& appInfo : appInfos) {
            CustomSlider slider { appInfo.currentVol, appInfo.pid };
            slidersAppVol.emplace_back(slider);
        }

        recalculateSliderRects(hWnd);

        InvalidateRect(hWnd, NULL, TRUE);
    } break;

    case WM_DESTROY: {
        ListenerAudio_MasterVolume::get().uninit();
        ListenerAudio_AllApplications::get().uninit();
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

        recalculateSliderRects(hWnd);
    } break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        FillRect(memDC, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        sliderMasterVol.Draw(memDC, hBrushSlider, true);
        for (auto& slider : slidersAppVol)
            slider.Draw(memDC, hBrushSlider);

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
