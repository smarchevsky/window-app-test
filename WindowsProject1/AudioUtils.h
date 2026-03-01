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

struct AudioUpdateInfo {
    enum Type : uint8_t { Master,
        App };
    union {
        struct {
            WPARAM _wp;
            LPARAM _lp;
        };
        struct {
            PID _pid;
            float _vol;
            bool _isMuted;
            Type _type;
        };
    };
    AudioUpdateInfo(Type type, PID pid, float vol, bool isMuted) { _pid = pid, _vol = vol, _isMuted = isMuted, _type = type; }
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
class CustomSlider {

    RECT m_rect;
    float m_value;
    PID m_pid;

public:
    CustomSlider(PID pid, float value) { m_rect = { 0 }, m_pid = pid, m_value = value; }
    CustomSlider() = default;

    PID getPID() const { return m_pid; }

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

class SliderManager {
    CustomSlider sliderMasterVol {};
    std::vector<CustomSlider> slidersAppVol;

public:
    CustomSlider& getMaster() { return sliderMasterVol; }
    void addAppSlider(PID pid, float vol, bool muted);
    void removeAppSlider(PID pid);
    void setSliderValue(PID pid, float vol, bool muted);
    //void removeAppSlider(PID pid);
    // std::optional<PID> getHoveredSlider(POINT mousePos);

    void recalculateSliderRects(HWND hWnd);
    void drawSliders(HDC hdc);
};