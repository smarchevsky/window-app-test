#include "AudioUtils.h"

#include <endpointvolume.h> // IAudioEndpointVolume, IAudioEndpointVolumeCallback
#include <mmdeviceapi.h> // IMMDevice, IMMDeviceEnumerator

#include <audiopolicy.h>
#include <psapi.h>

// #include <stdio.h>
#include <cassert>
#include <string>

// S_OK, S_FALSE
CoinitializeWrapper::CoinitializeWrapper() { (void)CoInitialize(NULL); }
CoinitializeWrapper::~CoinitializeWrapper() { CoUninitialize(); }

//
// MASTER
//

namespace {
class VolumeChangeListener : public IAudioEndpointVolumeCallback {
public:
    HWND hNotify;

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
        printf("Master vol: %d\n", volumePercent);

        // masterVol = volumePercent;
        // PostMessage(GetParent(g_hEdit), WM_REFRESH_MASTER_VOL, 0, 0);

        PostMessage(hNotify, WM_REFRESH_MASTER_VOL, 0, 0);

        return S_OK;
    }

    static VolumeChangeListener& get()
    {
        static VolumeChangeListener instance;
        return instance;
    }
};
}

void ListenerAudio_MasterVolume::init(HWND callbackWnd)
{
    VolumeChangeListener::get().hNotify = callbackWnd;

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&g_pVolumeControl);

    // instant refresh ?
    PostMessage(callbackWnd, WM_REFRESH_MASTER_VOL, 0, 0);

    g_pVolumeControl->RegisterControlChangeNotify(&VolumeChangeListener::get());

    pDevice->Release();
    pEnumerator->Release();
}

void ListenerAudio_MasterVolume::uninit()
{
    if (g_pVolumeControl) {
        g_pVolumeControl->UnregisterControlChangeNotify(&VolumeChangeListener::get());
        g_pVolumeControl->Release();
    }
}

float ListenerAudio_MasterVolume::getValue()
{
    float currentVol = 0;
    if (g_pVolumeControl)
        g_pVolumeControl->GetMasterVolumeLevelScalar(&currentVol);
    return currentVol;
}

void ListenerAudio_MasterVolume::setValue(float val)
{
    if (g_pVolumeControl)
        g_pVolumeControl->SetMasterVolumeLevelScalar(val, NULL);
}

//
// APPLICATIONS
//

namespace {
class AudioObserver : public IAudioSessionEvents, public IAudioSessionNotification {
public:
    HWND hNotify;

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
        PostMessage(hNotify, WM_REFRESH_APP_VOLUMES, 0, 0);
        return S_OK;
    }

    // Triggered when an app volume changes in the mixer
    STDMETHODIMP OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext)
    {
        PostMessage(hNotify, WM_REFRESH_APP_VOLUMES, 0, 0);
        return S_OK;
    }

    // Boilerplate for IAudioSessionEvents
    STDMETHODIMP OnDisplayNameChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    STDMETHODIMP OnIconPathChanged(LPCWSTR, LPCGUID) { return S_OK; }
    STDMETHODIMP OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) { return S_OK; }
    STDMETHODIMP OnGroupingParamChanged(LPCGUID, LPCGUID) { return S_OK; }
    STDMETHODIMP OnStateChanged(AudioSessionState State)
    {
        PostMessage(hNotify, WM_REFRESH_APP_VOLUMES, 0, 0); // Handles app closing
        return S_OK;
    }
    STDMETHODIMP OnSessionDisconnected(AudioSessionDisconnectReason)
    {
        PostMessage(hNotify, WM_REFRESH_APP_VOLUMES, 0, 0);
        return S_OK;
    }

    static AudioObserver& get()
    {
        static AudioObserver instance;
        return instance;
    }
};

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (idObject == OBJID_WINDOW && idChild == INDEXID_CONTAINER) {
        PostMessage(AudioObserver::get().hNotify, WM_REFRESH_APP_VOLUMES, 0, 0);
    }
}

}

void ListenerAudio_AllApplications::init(HWND callbackWnd)
{
    AudioObserver::get().hNotify = callbackWnd;

    IMMDeviceEnumerator* pDevEnum = NULL;
    IMMDevice* pDev = NULL;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pDevEnum);
    pDevEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDev);
    pDev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&g_pSessionManager);

    g_pSessionManager->RegisterSessionNotification(&AudioObserver::get()); // Watch for NEW apps

    // instant refresh ?
    PostMessage(callbackWnd, WM_REFRESH_APP_VOLUMES, 0, 0);

    pDev->Release();
    pDevEnum->Release();

    // HOOK for windows destruct
    g_hook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, // Range of events (just destroy)
        NULL, // Handle to DLL (NULL for local hook)
        WinEventProc, // Our callback
        0, 0, // Process/Thread ID (0 = all)
        WINEVENT_OUTOFCONTEXT // Flags
    );
}

void ListenerAudio_AllApplications::uninit()
{
    if (g_hook)
        UnhookWinEvent(g_hook);

    if (g_pSessionManager)
        g_pSessionManager->UnregisterSessionNotification(&AudioObserver::get()), g_pSessionManager = nullptr;
    AudioObserver::get().hNotify = nullptr;
}

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
};

bool ListenerAudio_AllApplications::getInfo(std::vector<AppAudioInfo>& appInfos)
{
    appInfos.resize(0);

    if (!g_pSessionManager)
        return false;

    IAudioSessionEnumerator* pEnum = NULL;
    if (SUCCEEDED(g_pSessionManager->GetSessionEnumerator(&pEnum))) {
        // printf("sessionEnumerator %p\n", pEnum);
        int count = 0;
        pEnum->GetCount(&count);

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

                            AppAudioInfo appInfo {};
                            appInfo.pid = pid;
                            appInfo.currentVol = vol;
                            appInfo.appName = GetProcessName(pid);
                            appInfos.emplace_back(appInfo);

                            // outInfo += GetProcessName(pid) + L": " + std::to_wstring((int)(vol * 100)) + L"%\r\n";
                            pVol->Release();
                        }
                    }
                    pControl2->Release();
                }
                pControl->Release();
            }
        }

        pEnum->Release();
    }

    return true;
}

//
// NON AUDIO
//

//
// ICON
//

void IconManager::uninit()
{
    for (auto& pair : cachedProcessIcons) {
        auto& iconInfo = pair.second;
        if (iconInfo.hLarge)
            DestroyIcon(iconInfo.hLarge);
    }
    DestroyIcon(iiMasterSpeaker.hLarge);
    DestroyIcon(iiMasterHeadphones.hLarge);
    DestroyIcon(iiSystemSounds.hLarge);
}

COLORREF getIconColor(BITMAP bmp, ICONINFO iconInfo)
{
    // 1. Setup the bitmap info header
    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight; // Negative for top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    // 2. Allocate memory for pixels
    int pixelCount = bmp.bmWidth * bmp.bmHeight;
    std::vector<DWORD> pixels(pixelCount);

    // 3. Get the raw bits
    HDC hdc = GetDC(NULL);
    GetDIBits(hdc, iconInfo.hbmColor, 0, bmp.bmHeight, &pixels[0], (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);

    // 4. Calculate Average
    long long totalR = 0, totalG = 0, totalB = 0;
    int opaquePixels = 0;

    for (int i = 0; i < pixelCount; i++) {
        BYTE a = (pixels[i] >> 24) & 0xFF;
        BYTE r = (pixels[i] >> 16) & 0xFF;
        BYTE g = (pixels[i] >> 8) & 0xFF;
        BYTE b = pixels[i] & 0xFF;

        if (a > 127) {
            totalR += r, totalG += g, totalB += b, opaquePixels++;
        }
    }

    if (opaquePixels > 0) {
        BYTE avgR = BYTE(totalR / opaquePixels);
        BYTE avgG = BYTE(totalG / opaquePixels);
        BYTE avgB = BYTE(totalB / opaquePixels);
        return RGB(avgR, avgG, avgB);
    }
    return RGB(160, 160, 160);
}

static IconInfo createIconInfo(HICON icon)
{
    ICONINFO iconInfo;
    GetIconInfo(icon, &iconInfo);

    BITMAP bmp;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    IconInfo info {};
    info.hBrush = CreateSolidBrush(getIconColor(bmp, iconInfo));
    info.hLarge = icon;
    info.width = bmp.bmWidth;
    return info;
}

IconManager::IconManager()
{
    HICON hIcon = nullptr;
    wchar_t dllPathSource[MAX_PATH];
    GetSystemDirectoryW(dllPathSource, MAX_PATH);

    auto loadIcon = [&](const wchar_t* path, int index) -> IconInfo {
        wchar_t dllPath[MAX_PATH];
        wcscpy_s(dllPath, MAX_PATH, dllPathSource);
        wcscat_s(dllPath, path);
        ExtractIconExW(dllPath, index, &hIcon, nullptr, 1);
        return createIconInfo(hIcon);
    };

    iiMasterSpeaker = loadIcon(L"\\mmres.dll", 0);
    iiMasterHeadphones = loadIcon(L"\\mmres.dll", 2);
    iiSystemSounds = loadIcon(L"\\imageres.dll", 104);
}

IconInfo IconManager::getIconFromProcess(DWORD pid)
{
    IconInfo result = {};

    auto foundIconIt = cachedProcessIcons.find(pid);
    if (foundIconIt == cachedProcessIcons.end()) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) {
            result = iiSystemSounds;
            return result;
        }

        wchar_t exePath[MAX_PATH];
        DWORD size = MAX_PATH;

        if (!QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
            CloseHandle(hProcess);
            return result;
        }

        CloseHandle(hProcess);

        HICON icon;
        UINT icons = ExtractIconExW(exePath, 0, &icon, nullptr, 1);
        if (icons == 0)
            return result;

        result = createIconInfo(icon);

        cachedProcessIcons[pid] = result;

        printf("Icon width %d\n", result.width);
        wprintf(L"Stored new icon for: %s\n", exePath);
    } else {
        result = foundIconIt->second;
    }

    return result;
}

IconInfo IconManager::getIconMasterVol() { return iiMasterSpeaker; }

//
// SLIDER
//

// #include <shellapi.h>
//  #pragma comment(lib, "Shell32.lib")

void CustomSlider::draw(HDC hdc, bool isSystem) const
{
    float drawHeight = (m_rect.bottom - m_rect.top) * (1.f - m_value);

    RECT drawRect {
        m_rect.left + margin, m_rect.top + LONG(drawHeight),
        m_rect.right - margin, m_rect.bottom
    };

    auto& im = IconManager::get();
    IconInfo iconInfo {};
    if (isSystem) {
        iconInfo = im.getIconMasterVol();
    } else {
        iconInfo = im.getIconFromProcess(m_pid);
    }

    if (drawRect.right > drawRect.left && drawRect.bottom > drawRect.top)
        FillRect(hdc, &drawRect, iconInfo.hBrush);

    if (iconInfo.hLarge)
        DrawIconEx(hdc,
            m_rect.left + sliderWidth / 2 - iconInfo.width / 2,
            drawRect.bottom - sliderWidth / 2 - iconInfo.width / 4,
            iconInfo.hLarge, 0, 0, 0, NULL, DI_NORMAL);
}

//
// USER INTERFACE MANAGER
//

inline SliderId IndexToSelect(int index) { return (SliderId)((int)SliderId::App_0 + index); }
inline int SelectToIndex(SliderId select) { return (int)select - (int)SliderId::App_0; }

CustomSlider& SliderManager::getSlider(SliderId select)
{
    if (select == SliderId::Master)
        return sliderMasterVol;

    else if ((int)select >= (int)SliderId::App_0)
        return slidersAppVol.at(SelectToIndex(select));

    static CustomSlider nullSlider;
    return nullSlider;
}

SliderId SliderManager::getHoveredSlider(POINT mousePos)
{
    if (sliderMasterVol.intersects(mousePos))
        return SliderId::Master;

    for (int i = 0; i < slidersAppVol.size(); ++i)
        if (slidersAppVol.at(i).intersects(mousePos))
            return IndexToSelect(i);

    return SliderId::None;
}

void SliderManager::updateApplicationInfo(std::vector<AppAudioInfo>& appInfos)
{
    slidersAppVol.resize(0);
    for (auto& appInfo : appInfos) {
        CustomSlider slider { appInfo.currentVol, appInfo.pid };
        slidersAppVol.emplace_back(slider);
    }
}

void SliderManager::recalculateSliderRects(HWND hWnd)
{
    RECT windowRect;
    GetClientRect(hWnd, &windowRect);
    LONG windowHeight = windowRect.bottom;

    sliderMasterVol.setRect({ 0, margin, sliderWidth, windowHeight - margin });
    int offset = sliderWidth + 30;
    for (auto& slider : slidersAppVol) {
        slider.setRect({ offset, margin, offset + sliderWidth, windowHeight - margin });
        offset += sliderWidth;
    }
}

void SliderManager::drawSliders(HDC hdc)
{
    sliderMasterVol.draw(hdc, true);
    for (auto& slider : slidersAppVol)
        slider.draw(hdc);
}