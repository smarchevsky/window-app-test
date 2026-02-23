#include "AudioUtils.h"

#include <endpointvolume.h> // IAudioEndpointVolume, IAudioEndpointVolumeCallback
#include <mmdeviceapi.h> // IMMDevice, IMMDeviceEnumerator

#include <audiopolicy.h>
#include <psapi.h>

// #include <stdio.h>
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

    // Get initial volume
    // float currentVol = 0;
    // g_pVolumeControl->GetMasterVolumeLevelScalar(&currentVol);
    // std::wstring text = L"Volume: " + std::to_wstring((int)(currentVol * 100)) + L"%";
    // SetWindowTextW(g_label, text.c_str());

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

std::wstring ListenerAudio_MasterVolume::getInfo()
{
    float currentVol = 0;
    ListenerAudio_MasterVolume::get().getEndPointVolume()->GetMasterVolumeLevelScalar(&currentVol);
    return L"Volume: " + std::to_wstring((int)(currentVol * 100)) + L"%";
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

    static AudioObserver& get()
    {
        static AudioObserver instance;
        return instance;
    }
};

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
    PostMessage(callbackWnd, WM_REFRESH_VOLUMES, 0, 0);

    pDev->Release();
    pDevEnum->Release();
}

void ListenerAudio_AllApplications::uninit()
{
}

std::wstring ListenerAudio_AllApplications::getInfo()
{
    std::wstring text;

    auto GetProcessName = [](DWORD pid) -> std::wstring {
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

    if (!g_pSessionManager)
        return text;

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
                            pVol->Release();
                        }
                    }
                    pControl2->Release();
                }
                pControl->Release();
            }
        }
        text = std::move(output);

        pEnum->Release();
    }
    return text;
}
