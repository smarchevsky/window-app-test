#include "AudioUtils.h"

#include <endpointvolume.h> // IAudioEndpointVolume, IAudioEndpointVolumeCallback
#include <mmdeviceapi.h> // IMMDevice, IMMDeviceEnumerator

#include <audiopolicy.h>
#include <psapi.h>
#include <shlobj.h>

// #include <cassert>
#include <iostream>
#include <mutex>
#include <string>

struct IAudioSessionControl;
struct IAudioSessionEvents;

// S_OK, S_FALSE
CoinitializeWrapper::CoinitializeWrapper() { (void)CoInitialize(NULL); }
CoinitializeWrapper::~CoinitializeWrapper() { CoUninitialize(); }

namespace {
struct ExpiredSession {
    IAudioSessionControl* pCtrl;
    IAudioSessionEvents* pEvents;
};
std::vector<ExpiredSession> g_expiredSessions;
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
        AudioUpdateInfo info(VolumeType::Master, (PID)0, pNotify->fMasterVolume, pNotify->bMuted);
        PostMessage(_hWnd, WM_REFRESH_VOL, info._wp, info._lp);
        return S_OK;
    }
};

class AudioSessionEvents : public IAudioSessionEvents {
    LONG _cRef;
    PID _pid;
    IAudioSessionControl* _pCtrl; // back-reference to owning session
    HWND _hWnd;

public:
    AudioSessionEvents(PID pid, IAudioSessionControl* pCtrl, HWND hWnd)
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
            wprintf(L"Delete this called [PID %u]\n", _pid);
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
        AudioUpdateInfo info(VolumeType::App, _pid, newVol, newMute);
        PostMessage(_hWnd, WM_REFRESH_VOL, info._wp, info._lp);
        // wprintf(L"[PID %u] Volume: %.2f | Muted: %s\n", _pid, newVol, newMute ? L"Yes" : L"No");
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
                g_expiredSessions.push_back({ _pCtrl, this });
                AddRef();
            }
            AudioUpdateInfo info(VolumeType::App, _pid, 0, false);
            PostMessage(_hWnd, WM_APP_UNREGISTERED, info._wp, info._lp);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason) override
    {
        wprintf(L"OnSessionDisconnected [PID %u]\n", _pid);
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

        PID pid = 0;
        if (pCtrl2)
            pCtrl2->GetProcessId(&pid);

        ISimpleAudioVolume* pVol = NULL;
        if (SUCCEEDED(pNewSession->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol))) {
            BOOL bMute;
            float vol;
            pVol->GetMasterVolume(&vol);
            pVol->GetMute(&bMute);
            AudioUpdateInfo info(VolumeType::App, pid, vol, bMute);
            PostMessage(_hWnd, WM_APP_REGISTERED, info._wp, info._lp);
            wprintf(L"New session [PID %u], vol: %d\n", pid, (int)(info._vol * 100));
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

        PID pid = 0;
        if (pCtrl2) {
            pCtrl2->GetProcessId(&pid);
            pCtrl2->Release();
        }

        // extract master vol
        ISimpleAudioVolume* pVol = NULL;
        if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol))) {
            BOOL bMute;
            float vol;
            pVol->GetMasterVolume(&vol);
            pVol->GetMute(&bMute);
            AudioUpdateInfo info(VolumeType::App, pid, vol, bMute);
            PostMessage(hWnd, WM_APP_REGISTERED, info._wp, info._lp);
            wprintf(L"Registering [PID %u], vol: %d\n", pid, (int)(info._vol * 100));
            pVol->Release();
        }

        // create master event listener
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

    // extract application vol
    BOOL bMute;
    float vol;
    pEndpointVolume->GetMasterVolumeLevelScalar(&vol);
    pEndpointVolume->GetMute(&bMute);
    AudioUpdateInfo info(VolumeType::Master, (PID)0, vol, bMute);
    PostMessage(hWnd, WM_REFRESH_VOL, info._wp, info._lp);

    // create application event listener
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

void AudioUpdateListener::cleanupExpiredSessions()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& e : g_expiredSessions) {
        e.pCtrl->UnregisterAudioSessionNotification(e.pEvents);
        e.pEvents->Release(); // pair for AddRef above
        e.pCtrl->Release(); // pair for AddRef in OnSessionCreated
    }
    g_expiredSessions.clear();
    printf("Cleanup session, g_trackedSessions num %llu\n", g_trackedSessions.size());
}

void AudioUpdateListener::setVol(SelectInfo selectInfo, float vol)
{
    if (selectInfo._type == VolumeType::Master) {
        if (pEndpointVolume) {
            pEndpointVolume->SetMasterVolumeLevelScalar(vol, nullptr);
            // wprintf(L"Setting master vol: %d\n", (int)(vol * 100));
        }

    } else if (selectInfo._type == VolumeType::App) {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto* pCtrl : g_trackedSessions) {
            IAudioSessionControl2* pCtrl2 = nullptr;
            pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2);
            if (!pCtrl2)
                continue;

            DWORD sessionPid = 0;
            pCtrl2->GetProcessId(&sessionPid);
            pCtrl2->Release();

            if (sessionPid == selectInfo._pid) {
                ISimpleAudioVolume* pVolume = nullptr;
                pCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVolume);
                if (pVolume) {
                    pVolume->SetMasterVolume(vol, nullptr);
                    pVolume->Release();
                    // wprintf(L"Setting app vol [PID %u], vol: %d\n", selectInfo._pid, (int)(vol * 100));
                }
                break; // remove if app has multiple sessions
            }
        }
    }
}

std::wstring GetProcessName(PID pid)
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

IconInfo IconManager::getIconFromProcess(PID pid)
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

void Slider::draw(HDC hdc, bool isSystem) const
{
    float drawHeight = (_rect.bottom - _rect.top) * (1.f - _val);

    RECT drawRect {
        _rect.left + margin, _rect.top + LONG(drawHeight),
        _rect.right - margin, _rect.bottom
    };

    auto& im = IconManager::get();
    IconInfo iconInfo {};
    if (isSystem) {
        iconInfo = im.getIconMasterVol();
    } else {
        iconInfo = im.getIconFromProcess(_pid);
    }

    if (drawRect.right > drawRect.left && drawRect.bottom > drawRect.top)
        FillRect(hdc, &drawRect, iconInfo.hBrush);

    if (iconInfo.hLarge)
        DrawIconEx(hdc,
            _rect.left + sliderWidth / 2 - iconInfo.width / 2,
            drawRect.bottom - sliderWidth / 2 - iconInfo.width / 4,
            iconInfo.hLarge, 0, 0, 0, NULL, DI_NORMAL);
}

//
// USER INTERFACE MANAGER
//

Slider* SliderManager::getGetBySelectInfo(SelectInfo info)
{
    if (info._type == VolumeType::Master)
        return &sliderMaster;

    else if (info._type == VolumeType::App) {
        auto it = std::find_if(slidersApp.begin(), slidersApp.end(), [&](const Slider& s) { return s.getPID() == info._pid; });
        if (it != slidersApp.end())
            return &*it;
    }

    return nullptr;
}

void SliderManager::addAppSlider(PID pid, float vol, bool muted)
{
    auto it = std::find_if(slidersApp.begin(), slidersApp.end(), [&](const Slider& s) { return s.getPID() == pid; });
    slidersApp.push_back(Slider(pid, vol));
}

void SliderManager::removeAppSlider(PID pid)
{
    auto it = std::find_if(slidersApp.begin(), slidersApp.end(), [&](const Slider& s) { return s.getPID() == pid; });
    slidersApp.erase(it);
}

SelectInfo SliderManager::getHoveredSlider(POINT mousePos)
{
    if (sliderMaster.intersects(mousePos)) {
        return SelectInfo(VolumeType::Master, (PID)0);
    }

    for (int i = 0; i < slidersApp.size(); ++i)
        if (slidersApp.at(i).intersects(mousePos)) {
            return SelectInfo(VolumeType::App, slidersApp.at(i).getPID());
        }

    return {};
}

void SliderManager::recalculateSliderRects(const RECT& r)
{
    sliderMaster._rect = { r.left, r.top, r.left + sliderWidth, r.bottom };
    int offset = sliderMaster._rect.right + 30;
    for (auto& slider : slidersApp) {
        slider._rect = { offset, r.top, offset + sliderWidth, r.bottom };
        offset += sliderWidth;
    }
}

void SliderManager::drawSliders(HDC hdc)
{
    sliderMaster.draw(hdc, true);
    for (auto& slider : slidersApp)
        slider.draw(hdc);
}

FileManager::FileManager()
{
    PWSTR appDataPath = NULL;
    _iniPath = L"";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath))) {
        _iniPath = std::wstring(appDataPath) + L"\\YourAppName.ini";
        CoTaskMemFree(appDataPath);
    }
}

void FileManager::loadWindowRect(HWND hwnd) const
{
    std::wstring iniPath = _iniPath;
    if (iniPath.empty())
        return;

    RECT rect;
    rect.left = GetPrivateProfileIntW(L"Window", L"x", 100, iniPath.c_str());
    rect.top = GetPrivateProfileIntW(L"Window", L"y", 100, iniPath.c_str());
    int width = GetPrivateProfileIntW(L"Window", L"w", 800, iniPath.c_str());
    int height = GetPrivateProfileIntW(L"Window", L"h", 600, iniPath.c_str());
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;

    HMONITOR hMonitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
    if (hMonitor == NULL) {
        rect.left = 100;
        rect.top = 100;

    } else {
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            if (rect.right > mi.rcWork.right)
                rect.left = mi.rcWork.right - width;
            if (rect.bottom > mi.rcWork.bottom)
                rect.top = mi.rcWork.bottom - height;
            if (rect.left < mi.rcWork.left)
                rect.left = mi.rcWork.left;
            if (rect.top < mi.rcWork.top)
                rect.top = mi.rcWork.top;
        }
    }

    SetWindowPos(hwnd, NULL, rect.left, rect.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void FileManager::saveWindowRect(HWND hwnd) const
{
    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        std::wstring iniPath = _iniPath;
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        WritePrivateProfileStringW(L"Window", L"x", std::to_wstring(rect.left).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"Window", L"y", std::to_wstring(rect.top).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"Window", L"w", std::to_wstring(width).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"Window", L"h", std::to_wstring(height).c_str(), iniPath.c_str());
    }
}
