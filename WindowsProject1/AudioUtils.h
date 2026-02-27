#pragma once

#include <Windows.h>

#include <string>
#include <unordered_map>
#include <vector>

enum {
    _____WM_USER = WM_USER,
    WM_REFRESH_MASTER_VOL,
    WM_REFRESH_APP_VOLUMES,
    IDT_TIMER_1,
};

class CoinitializeWrapper {
public:
    CoinitializeWrapper();
    ~CoinitializeWrapper();
};

//
// MASTER
//

struct IAudioEndpointVolume;
class ListenerAudio_MasterVolume {
    IAudioEndpointVolume* g_pVolumeControl;

public:
    void init(HWND callbackWnd);
    void uninit();
    float getValue();
    void setValue(float val);

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

struct IAudioSessionManager2;
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
    HBRUSH hBrush;
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

static bool isValidRect(const RECT& rect) { return rect.right > rect.left && rect.bottom > rect.top; }

static constexpr int sliderWidth = 80;
static constexpr int margin = 10;
class CustomSlider {

    RECT m_rect;
    float m_value;
    DWORD m_pid;

public:
    CustomSlider(float value, DWORD pid) { m_rect = { 0 }, m_pid = pid, m_value = value; }
    CustomSlider() = default;

    void setRect(RECT rect) { m_rect = rect; }

    float getHeight() const { return float(m_rect.bottom - m_rect.top); }

    void setValue(float value) { m_value = value; }
    float getValue() const { return m_value; }
    bool intersects(POINT pos) const { return isValidRect(m_rect) ? PtInRect(&m_rect, pos) : false; }
    void draw(HDC hdc, bool isSystem = false) const;
};

//
// SLIDER MANAGER
//

enum class SliderSelect {
    None,
    Master,
    App_0
};

class SliderManager {
    CustomSlider sliderMasterVol {};
    std::vector<CustomSlider> slidersAppVol;

public:
    CustomSlider& getSlider(SliderSelect select);
    SliderSelect getHoveredSlider(POINT mousePos);
    void updateApplicationInfo(std::vector<AppAudioInfo>& appInfos);

    void recalculateSliderRects(HWND hWnd);
    void drawSliders(HDC hdc);
};