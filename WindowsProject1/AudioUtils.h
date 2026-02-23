#pragma once

#include <Windows.h>

#include <string>
#include <vector>

enum {
    _____WM_USER = WM_USER,
    WM_REFRESH_MASTER_VOL,
    WM_REFRESH_APP_VOLUMES,
};

class CoinitializeWrapper {
public:
    CoinitializeWrapper();
    ~CoinitializeWrapper();
};

//
// MASTER
//

class IAudioEndpointVolume;
class ListenerAudio_MasterVolume {
    IAudioEndpointVolume* g_pVolumeControl;

public:
    void init(HWND callbackWnd);
    void uninit();
    float getValue();

    static ListenerAudio_MasterVolume& get()
    {
        static ListenerAudio_MasterVolume instance;
        return instance;
    }

private:
    ListenerAudio_MasterVolume() = default;
};

//
// APPLICATIONS
//

struct AppAudioInfo {
    DWORD appId;
    float currentVol;
    std::wstring appName;
};

class IAudioSessionManager2;
class ListenerAudio_AllApplications {
    IAudioSessionManager2* g_pSessionManager;
    HWINEVENTHOOK g_hook; // to detect app close

public:
    void init(HWND hwnd);
    void uninit();
    bool getInfo(std::vector<AppAudioInfo>& appInfos);

    static ListenerAudio_AllApplications& get()
    {
        static ListenerAudio_AllApplications instance;
        return instance;
    }

private:
    ListenerAudio_AllApplications() = default;
};
