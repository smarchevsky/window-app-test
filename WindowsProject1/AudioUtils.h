#pragma once

#include <Windows.h>

#include <string>
#include <unordered_map>
#include <vector>

typedef DWORD PID;

enum {
    _____WM_APP = WM_APP,
    WM_REFRESH_VOL,
    WM_APP_REGISTERED,
    WM_APP_UNREGISTERED,
    IDT_TIMER_1,
};

enum class VolumeType : uint8_t {
    Invalid,
    Master,
    App
};

struct SelectInfo {
    SelectInfo(VolumeType type, PID pid) { _type = type, _pid = pid; }
    SelectInfo() = default;
    bool operator==(const SelectInfo& rhs) const { return _type == rhs._type && _pid == rhs._pid; }
    bool operator!=(const SelectInfo& rhs) const { return !operator==(rhs); }
    operator bool() const { return _type != VolumeType::Invalid; }
    PID _pid;
    VolumeType _type;
};

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

struct IconInfo {
    HBRUSH hBrush;
    HICON hLarge;
    int width;
};

class IconManager {
    IconManager();
    IconManager(const IconManager&) = delete;

    std::unordered_map<PID, IconInfo> cachedProcessIcons;
    IconInfo iiMasterSpeaker, iiMasterHeadphones, iiSystemSounds;

public:
    void uninit();
    static IconManager& get()
    {
        static IconManager instance;
        return instance;
    }

    IconInfo getIconFromProcess(PID pid);
    IconInfo getIconMasterVol();
};

//
// SLIDER
//

static bool isValidRect(const RECT& rect) { return rect.right > rect.left && rect.bottom > rect.top; }

static constexpr int sliderWidth = 80;
static constexpr int margin = 10;

class Slider {
    PID _pid;

public:
    RECT _rect;
    float _val;

    Slider(PID pid, float value) { _pid = pid, _rect = { 0 }, _val = value; }
    Slider() = default;

    PID getPID() const { return _pid; }
    float getHeight() const { return float(_rect.bottom - _rect.top); }
    bool intersects(POINT pos) const { return isValidRect(_rect) ? PtInRect(&_rect, pos) : false; }
    void draw(HDC hdc, bool isSystem = false) const;
};

//
// SLIDER MANAGER
//

class SliderManager {
    Slider sliderMaster {};
    std::vector<Slider> slidersApp;

public:
    Slider* getGetBySelectInfo(SelectInfo info);
    void addAppSlider(PID pid, float vol, bool muted);
    void removeAppSlider(PID pid);
    SelectInfo getHoveredSlider(POINT mousePos);

    void recalculateSliderRects(const RECT& rect);
    void drawSliders(HDC hdc);
};

class FileManager {
    std::wstring _iniPath;
    FileManager();

public:
    static FileManager& get()
    {
        static FileManager instance;
        return instance;
    }

    void loadWindowRect(HWND hwnd) const;
    void saveWindowRect(HWND hwnd) const;
};