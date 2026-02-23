// WindowsProject1.cpp : Defines the entry point for the application.
//

#include "WindowsProject1.h"
#include "framework.h"

#include "AudioUtils.h"

#include <endpointvolume.h> // IAudioEndpointVolume, IAudioEndpointVolumeCallback
#include <mmdeviceapi.h> // IMMDevice, IMMDeviceEnumerator

#include <audiopolicy.h>
#include <psapi.h>

// #include <fcntl.h>
// #include <io.h>
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

//
// AUDIO
//

HWND g_label;
HWND g_hEdit;
IAudioSessionManager2* g_pSessionManager;
class AudioObserver* g_Observer;

//
// AUDIO
//

std::wstring GetProcessName(DWORD pid)
{
    if (pid == 0)
        return L"System Sounds";
    TCHAR szName[MAX_PATH] = TEXT("<unknown>");
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc) {
        GetModuleBaseName(hProc, NULL, szName, MAX_PATH);
        CloseHandle(hProc);
    }
    return szName;
}

class AudioObserver : public IAudioSessionEvents, public IAudioSessionNotification {
public:
    HWND hNotify;
    AudioObserver(HWND hwnd)
        : hNotify(hwnd)
    {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) { return E_NOINTERFACE; }
    STDMETHODIMP_(ULONG)
    AddRef() { return 1; }
    STDMETHODIMP_(ULONG)
    Release() { return 1; }

    // Triggered when a NEW app starts playing audio
    STDMETHODIMP OnSessionCreated(IAudioSessionControl* pNewSession)
    {
        pNewSession->RegisterAudioSessionNotification(this); // Watch this new app too
        PostMessage(hNotify, WM_REFRESH_VOLUMES, 0, 0);
        return S_OK;
    }

    // Triggered when an app volume changes in the mixer
    STDMETHODIMP OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext)
    {
        PostMessage(hNotify, WM_REFRESH_VOLUMES, 0, 0);
        return S_OK;
    }

    // Boilerplate for IAudioSessionEvents
    STDMETHODIMP OnDisplayNameChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    STDMETHODIMP OnIconPathChanged(LPCWSTR, LPCGUID) { return S_OK; }
    STDMETHODIMP OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) { return S_OK; }
    STDMETHODIMP OnGroupingParamChanged(LPCGUID, LPCGUID) { return S_OK; }
    STDMETHODIMP OnStateChanged(AudioSessionState State)
    {
        PostMessage(hNotify, WM_REFRESH_VOLUMES, 0, 0); // Handles app closing
        return S_OK;
    }
    STDMETHODIMP OnSessionDisconnected(AudioSessionDisconnectReason)
    {
        PostMessage(hNotify, WM_REFRESH_VOLUMES, 0, 0);
        return S_OK;
    }
};

HWINEVENTHOOK g_hook = NULL;
void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (idObject == OBJID_WINDOW && idChild == INDEXID_CONTAINER) {
        PostMessage(GetParent(g_hEdit), WM_REFRESH_VOLUMES, 0, 0);
    }
}

// WM_REFRESH_MASTER_VOL
void RefreshMasterVol()
{
    float currentVol = 0;
    ListenerAudio_MasterVolume::get().getEndPointVolume()->GetMasterVolumeLevelScalar(&currentVol);
    std::wstring text = L"Volume: " + std::to_wstring((int)(currentVol * 100)) + L"%";
    SetWindowTextW(g_label, text.c_str());
}

void RefreshUI()
{
    if (!g_pSessionManager)
        return;

    IAudioSessionEnumerator* pEnum = NULL;
    if (SUCCEEDED(g_pSessionManager->GetSessionEnumerator(&pEnum))) {
        int count = 0;
        pEnum->GetCount(&count);

        std::wstring output = L"App Volumes:\r\n\r\n";

        for (int i = 0; i < count; i++) {
            IAudioSessionControl* pControl = NULL;
            if (SUCCEEDED(pEnum->GetSession(i, &pControl))) {

                IAudioSessionControl2* pControl2 = NULL;
                if (SUCCEEDED(pControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pControl2))) {

                    AudioSessionState state;
                    pControl2->GetState(&state);

                    if (state != AudioSessionStateExpired) {
                        DWORD pid = 0;
                        pControl2->GetProcessId(&pid);

                        ISimpleAudioVolume* pVol = NULL;
                        if (SUCCEEDED(pControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol))) {
                            float vol = 0;
                            pVol->GetMasterVolume(&vol);
                            output += GetProcessName(pid) + L": " + std::to_wstring((int)(vol * 100)) + L"%\r\n";
                            pVol->Release(); // Release Volume
                        }
                    }
                    pControl2->Release(); // Release Control2
                }
                pControl->Release(); // Release Control
            }
        }
        SetWindowTextW(g_hEdit, output.c_str());
        pEnum->Release(); // Release Enumerator
    }
}

HBRUSH hBrush;
void drawSquareCodeExample(HDC hdc)
{
    if (hBrush == nullptr)
        hBrush = CreateSolidBrush(RGB(100, 100, 250));
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

    (void)CoInitialize(NULL); // S_OK, S_FALSE

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

    g_hook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, // Range of events (just destroy)
        NULL, // Handle to DLL (NULL for local hook)
        WinEventProc, // Our callback
        0, 0, // Process/Thread ID (0 = all)
        WINEVENT_OUTOFCONTEXT // Flags
    );

    {
        g_label = CreateWindowW(L"STATIC", L"Volume: 0%",
            WS_VISIBLE | WS_CHILD, 350, 40, 200, 30,
            hWnd, nullptr, hInstance, nullptr);

        ListenerAudio_MasterVolume::get().init(hWnd);
    }
    {
        g_hEdit = CreateWindowW(L"STATIC", L"",
            WS_VISIBLE | WS_CHILD, 10, 10, 360, 440,
            hWnd, nullptr, hInstance, nullptr);

        IMMDeviceEnumerator* pDevEnum = NULL;
        IMMDevice* pDev = NULL;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pDevEnum);
        pDevEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDev);
        pDev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&g_pSessionManager);
        g_Observer = new AudioObserver(hWnd);
        g_pSessionManager->RegisterSessionNotification(g_Observer); // Watch for NEW apps
        RefreshUI();
        pDev->Release();
        pDevEnum->Release();
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CoUninitialize();
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_REFRESH_MASTER_VOL: {
        RefreshMasterVol();
        return 0;
    }
    case WM_REFRESH_VOLUMES:
        RefreshUI();
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

        if (g_hook)
            UnhookWinEvent(g_hook);
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
