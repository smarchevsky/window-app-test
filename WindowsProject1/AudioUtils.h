#pragma once

#include "Common.h"

struct AudioUpdateInfo {

    union {
        struct {
            WPARAM _wp;
            LPARAM _lp;
        };
        struct {
            PID _pid;
            float _vol;
            bool _isMuted;
            VolumeType _type;
        };
    };
    AudioUpdateInfo(VolumeType type, PID pid, float vol, bool isMuted) { _pid = pid, _vol = vol, _isMuted = isMuted, _type = type; }
    AudioUpdateInfo(WPARAM wp, LPARAM lp) { _wp = wp, _lp = lp; }
};

static_assert(sizeof(WPARAM) == 8);
static_assert(sizeof(LPARAM) == 8);
static_assert(sizeof(AudioUpdateInfo) <= 16);

class CoinitializeWrapper {
public:
    CoinitializeWrapper();
    ~CoinitializeWrapper();
};

struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioEndpointVolume; // master
struct IAudioEndpointVolumeCallback;
struct IAudioSessionManager2; // apps
struct IAudioSessionNotification;

class AudioUpdateListener {
    IMMDeviceEnumerator* pEnumerator {};
    IMMDevice* pDevice {};
    IAudioEndpointVolume* pEndpointVolume {}; // master
    IAudioEndpointVolumeCallback* pCallback {}; // master
    IAudioSessionManager2* pMgr {}; // apps
    IAudioSessionNotification* pNotif {}; // apps

public:
    void init(HWND hWnd);
    void uninit();
    void cleanupExpiredSessions();

    void setVol(SelectInfo selectInfo, float vol);

    static AudioUpdateListener& get()
    {
        static AudioUpdateListener instance;
        return instance;
    }

private:
    AudioUpdateListener() = default;
};

//
// NON AUDIO
//

//
// ICON
//
