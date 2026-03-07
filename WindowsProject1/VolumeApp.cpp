#include "VolumeApp.h"

#include "Utils.h"

#include "cassert"

WPARAM VolumeApp::create(WNDPROC winProc)
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
    return Application::initWindow(winParam, winRect);
}

void VolumeApp::shutDown(HWND hWnd)
{
    RECT winRect;
    assert(hWnd == _hWnd);
    if (GetWindowRect(hWnd, &winRect))
        FileManager::get().saveWindowRect(winRect);

    DestroyWindow(hWnd);
}
