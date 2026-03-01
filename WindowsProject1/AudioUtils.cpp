#include "AudioUtils.h"

#include <endpointvolume.h> // IAudioEndpointVolume, IAudioEndpointVolumeCallback
#include <mmdeviceapi.h> // IMMDevice, IMMDeviceEnumerator

#include <audiopolicy.h>
#include <psapi.h>

// #include <cassert>
#include <iostream>
#include <mutex>
#include <string>

// S_OK, S_FALSE
CoinitializeWrapper::CoinitializeWrapper() { (void)CoInitialize(NULL); }
CoinitializeWrapper::~CoinitializeWrapper() { CoUninitialize(); }

namespace {
std::vector<IAudioSessionControl*> g_trackedSessions;
std::mutex g_mutex;

class CVolumeNotification : public IAudioEndpointVolumeCallback {
    HWND _hWnd;
    LONG _cRef;

public:
    CVolumeNotification(HWND hWnd)
        : _hWnd(hWnd)
        , _cRef(1)
    {
    }

    // IUnknown methods
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_cRef); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ref = InterlockedDecrement(&_cRef);
        if (ref == 0)
            delete this;
        return ref;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // Called when volume/mute changes
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override
    {
        if (!pNotify)
            return E_INVALIDARG;
        AudioUpdateInfo info(AudioUpdateInfo::Master, (PID)0, pNotify->fMasterVolume, pNotify->bMuted);
        PostMessage(_hWnd, WM_REFRESH_VOL, info._wp, info._lp);
        return S_OK;
    }
};

class AudioSessionEvents : public IAudioSessionEvents {
    LONG _cRef;
    DWORD _pid;
    IAudioSessionControl* _pCtrl; // back-reference to owning session
    HWND _hWnd;

public:
    AudioSessionEvents(DWORD pid, IAudioSessionControl* pCtrl, HWND hWnd)
        : _cRef(1)
        , _pid(pid)
        , _pCtrl(pCtrl)
        , _hWnd(hWnd)
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&_cRef); }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG r = InterlockedDecrement(&_cRef);
        if (!r) {
            wprintf(L"Delete this called (PID %lu)\n", _pid);
            delete this;
        }
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv)
    {
        if (riid == IID_IUnknown || riid == __uuidof(IAudioSessionEvents)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(
        float newVol, BOOL newMute, LPCGUID) override
    {
        AudioUpdateInfo info(AudioUpdateInfo::App, _pid, newVol, newMute);
        PostMessage(_hWnd, WM_REFRESH_VOL, info._wp, info._lp);

        wprintf(L"[PID %lu] Volume: %.2f | Muted: %s\n",
            _pid, newVol, newMute ? L"Yes" : L"No");
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState newState) override
    {
        if (newState == AudioSessionStateExpired) {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = std::find(g_trackedSessions.begin(), g_trackedSessions.end(), _pCtrl);
            if (it != g_trackedSessions.end()) {
                g_trackedSessions.erase(it);
                Release();
            }
            AudioUpdateInfo info(AudioUpdateInfo::App, _pid, 0, false);
            PostMessage(_hWnd, WM_APP_UNREGISTERED, info._wp, info._lp);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason) override
    {
        wprintf(L"OnSessionDisconnected (PID %lu)\n", _pid);
        return S_OK;
    }
};

class SessionNotification : public IAudioSessionNotification {
    LONG _cRef;
    HWND _hWnd;

public:
    SessionNotification(HWND hWnd)
        : _cRef(1)
        , _hWnd(hWnd)
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&_cRef); }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG r = InterlockedDecrement(&_cRef);
        if (!r)
            delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv)
    {
        if (riid == IID_IUnknown || riid == __uuidof(IAudioSessionNotification)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // Fired when ANY new audio session (new app) starts playing
    HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl* pNewSession) override
    {
        // QI to IAudioSessionControl2 for a stable reference
        IAudioSessionControl2* pCtrl2 = nullptr;
        pNewSession->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2);

        DWORD pid = 0;
        if (pCtrl2)
            pCtrl2->GetProcessId(&pid);

        ISimpleAudioVolume* pVol = NULL;
        if (SUCCEEDED(pNewSession->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol))) {
            BOOL bMute;
            float vol;
            pVol->GetMasterVolume(&vol);
            pVol->GetMute(&bMute);
            AudioUpdateInfo info(AudioUpdateInfo::App, pid, vol, bMute);
            PostMessage(_hWnd, WM_APP_REGISTERED, info._wp, info._lp);
            wprintf(L"New session PID %lu, vol: %d\n", pid, (int)(info._vol * 100));
            pVol->Release();
        }

        AudioSessionEvents* pEvents = new AudioSessionEvents(pid, pNewSession, _hWnd);

        // Register on the ORIGINAL pNewSession, but also AddRef it
        pNewSession->AddRef(); // Keep it alive
        pNewSession->RegisterAudioSessionNotification(pEvents); // IAudioSessionEvents

        // Store pNewSession somewhere so it doesn't get released!
        g_trackedSessions.push_back(pNewSession); // example

        pEvents->Release();
        if (pCtrl2)
            pCtrl2->Release();
        return S_OK;
    }
};

void RegisterAllExistingSessions(IAudioSessionManager2* pMgr, HWND hWnd)
{
    IAudioSessionEnumerator* pEnum = nullptr;
    pMgr->GetSessionEnumerator(&pEnum);

    int count = 0;
    pEnum->GetCount(&count);

    for (int i = 0; i < count; i++) {
        IAudioSessionControl* pCtrl = nullptr;
        pEnum->GetSession(i, &pCtrl);

        IAudioSessionControl2* pCtrl2 = nullptr;
        pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2);

        DWORD pid = 0;
        if (pCtrl2) {
            pCtrl2->GetProcessId(&pid);
            pCtrl2->Release();
        }

        // extract default vol
        ISimpleAudioVolume* pVol = NULL;
        if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol))) {

            BOOL bMute;
            float vol;
            pVol->GetMasterVolume(&vol);
            pVol->GetMute(&bMute);
            AudioUpdateInfo info(AudioUpdateInfo::App, pid, vol, bMute);
            PostMessage(hWnd, WM_APP_REGISTERED, info._wp, info._lp);

            wprintf(L"Registering PID %lu, vol: %d\n", pid, (int)(info._vol * 100));
            pVol->Release();
        }

        // create event listener
        AudioSessionEvents* pEvents = new AudioSessionEvents(pid, pCtrl, hWnd);
        pCtrl->RegisterAudioSessionNotification(pEvents);
        pEvents->Release();

        pCtrl->AddRef();
        g_trackedSessions.push_back(pCtrl); // keep alive!
        pCtrl->Release();
    }

    pEnum->Release();
}
}

void AudioUpdateListener::init(HWND hWnd)
{
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

    // master
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pEndpointVolume);

    // extract default vol
    BOOL bMute;
    float vol;
    pEndpointVolume->GetMasterVolumeLevelScalar(&vol);
    pEndpointVolume->GetMute(&bMute);
    AudioUpdateInfo info(AudioUpdateInfo::Master, (PID)0, vol, bMute);
    PostMessage(hWnd, WM_REFRESH_VOL, info._wp, info._lp);

    // create event listener
    pCallback = new CVolumeNotification(hWnd);
    pEndpointVolume->RegisterControlChangeNotify(pCallback);

    // apps
    pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pMgr);
    RegisterAllExistingSessions(pMgr, hWnd);
    pNotif = new SessionNotification(hWnd);
    pMgr->RegisterSessionNotification(pNotif);
}

void AudioUpdateListener::uninit()
{
    // apps
    pMgr->UnregisterSessionNotification(pNotif);
    pNotif->Release();
    pMgr->Release();

    // master
    pEndpointVolume->UnregisterControlChangeNotify(pCallback);
    pCallback->Release();
    pEndpointVolume->Release();

    pDevice->Release();
    pEnumerator->Release();
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

// std::optional<PID> SliderManager::getHoveredSlider(POINT mousePos)
//{
//     std::optional<PID> result;
//     if (sliderMasterVol.intersects(mousePos))
//         return {};
//
//     for (int i = 0; i < slidersAppVol.size(); ++i)
//         if (slidersAppVol.at(i).intersects(mousePos))
//             return slidersAppVol.at(i).getPID();
//
//     return SliderId::None;
// }

void SliderManager::addAppSlider(PID pid, float vol, bool muted)
{
    auto it = std::find_if(slidersAppVol.begin(), slidersAppVol.end(), [&](const CustomSlider& s) { return s.getPID() == pid; });
    slidersAppVol.push_back(CustomSlider(pid, vol));
    // return &slidersAppVol.back();
}

void SliderManager::removeAppSlider(PID pid)
{
    auto it = std::find_if(slidersAppVol.begin(), slidersAppVol.end(), [&](const CustomSlider& s) { return s.getPID() == pid; });
    slidersAppVol.erase(it);
}

void SliderManager::setSliderValue(PID pid, float vol, bool muted)
{
    auto it = std::find_if(slidersAppVol.begin(), slidersAppVol.end(), [&](const CustomSlider& s) { return s.getPID() == pid; });
    if (it != slidersAppVol.end()) {
        it->setValue(vol);
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