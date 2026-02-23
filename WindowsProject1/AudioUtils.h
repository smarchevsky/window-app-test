#pragma once

#include <Windows.h>

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

// class ListenerAudio_AllApplications {
//     IAudioSessionManager2* g_pSessionManager;
//
// public:
//
//     void init(HWND hwnd);
//     void uninit();
//     void RefreshUI();
//
//     static ListenerAudio_AllApplications& get()
//     {
//         static ListenerAudio_AllApplications instance;
//         return instance;
//     }
//
// private:
//     ListenerAudio_AllApplications() = default;
// };
