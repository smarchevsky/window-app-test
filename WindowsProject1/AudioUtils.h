#pragma once

#include <Windows.h>

#include <string>

enum {
    _____WM_USER = WM_USER,
    WM_REFRESH_MASTER_VOL,
    WM_REFRESH_VOLUMES
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

    IAudioEndpointVolume* getEndPointVolume() { return g_pVolumeControl; }

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

class IAudioSessionManager2;
class ListenerAudio_AllApplications {
    IAudioSessionManager2* g_pSessionManager;

public:
    void init(HWND hwnd);
    void uninit();
    std::wstring getInfo();

    static ListenerAudio_AllApplications& get()
    {
        static ListenerAudio_AllApplications instance;
        return instance;
    }

private:
    ListenerAudio_AllApplications() = default;
};
