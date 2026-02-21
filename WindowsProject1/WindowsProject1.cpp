// WindowsProject1.cpp : Defines the entry point for the application.
//

#include "WindowsProject1.h"
#include "framework.h"

#include <endpointvolume.h> // IAudioEndpointVolume, IAudioEndpointVolumeCallback
#include <mmdeviceapi.h> // IMMDevice, IMMDeviceEnumerator

#include <string>

#define MAX_LOADSTRING 100

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

HINSTANCE hInst; // current instance
WCHAR szTitle[MAX_LOADSTRING]; // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

//
// AUDIO
//

HWND g_label;
IAudioEndpointVolume* g_pVolumeControl;

class VolumeChangeListener : public IAudioEndpointVolumeCallback {
public:
    // COM boilerplate
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG)
    AddRef() { return 1; }
    STDMETHODIMP_(ULONG)
    Release() { return 1; }

    // This triggers when volume changes
    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pData)
    {
        int volumePercent = (int)(pData->fMasterVolume * 100 + 0.5f);
        std::wstring text = L"Volume: " + std::to_wstring(volumePercent) + L"%";
        SetWindowTextW(g_label, text.c_str());
        return S_OK;
    }
} g_VolumeCallback;

// --- 2. Initialize WASAPI ---
void initAudio()
{
    CoInitialize(NULL);
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&g_pVolumeControl);

    // Get initial volume
    float currentVol = 0;
    g_pVolumeControl->GetMasterVolumeLevelScalar(&currentVol);
    std::wstring text = L"Volume: " + std::to_wstring((int)(currentVol * 100)) + L"%";
    SetWindowTextW(g_label, text.c_str());

    // Register our listener
    g_pVolumeControl->RegisterControlChangeNotify(&g_VolumeCallback);

    pDevice->Release();
    pEnumerator->Release();
}

//
// AUDIO
//

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
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
        hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
        if (!hWnd)
            return FALSE;

        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }
    {
        g_label = CreateWindowW(L"STATIC", L"Volume: 0%",
            WS_VISIBLE | WS_CHILD, 50, 40, 200, 30,
            hWnd, nullptr, hInstance, nullptr);

        initAudio();
    }

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
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(hWnd, &ps);
    } break;
    case WM_DESTROY:
        if (g_pVolumeControl) {
            g_pVolumeControl->UnregisterControlChangeNotify(&g_VolumeCallback);
            g_pVolumeControl->Release();
        }
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
