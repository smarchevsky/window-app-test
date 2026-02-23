#include "AudioUtils.h"

#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <psapi.h>

// #include <stdio.h>
#include <string>

namespace {
class VolumeChangeListener : public IAudioEndpointVolumeCallback {
public:
    HWND callbackWnd;
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

        PostMessage(callbackWnd, WM_REFRESH_MASTER_VOL, 0, 0);

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
    VolumeChangeListener::get().callbackWnd = callbackWnd;

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&g_pVolumeControl);

    PostMessage(callbackWnd, WM_REFRESH_MASTER_VOL, 0, 0);
    // m_onUpdated();

    // Get initial volume
    // float currentVol = 0;
    // g_pVolumeControl->GetMasterVolumeLevelScalar(&currentVol);
    // std::wstring text = L"Volume: " + std::to_wstring((int)(currentVol * 100)) + L"%";
    // SetWindowTextW(g_label, text.c_str());

    // Register our listener
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
