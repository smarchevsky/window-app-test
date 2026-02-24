#pragma once

#include <Windows.h>

#include <string>
#include <unordered_map>
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
    DWORD pid;
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

//
// NON AUDIO
//

//
// ICON
//

struct IconInfo {
    HICON hLarge;
    int width;
};

class IconManager {
    IconManager();
    IconManager(const IconManager&) = delete;

    std::unordered_map<DWORD, IconInfo> cachedProcessIcons;
    IconInfo iiMasterSpeaker, iiMasterHeadphones, iiSystemSounds;

public:
    void uninit();
    static IconManager& get()
    {
        static IconManager instance;
        return instance;
    }

    IconInfo getIconFromProcess(DWORD pid);
    IconInfo getIconMasterVol();
};

//
// SLIDER
//

static constexpr int sliderWidth = 80;
struct CustomSlider {
    static constexpr int margin = 10;
    float value;
    DWORD pid;
    void Draw(HDC hdc, HBRUSH brush, LONG windowHeight, int leftOffset, bool isSystem = false);
};