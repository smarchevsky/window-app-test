#pragma once

#include "Common.h"

#include <string>
#include <unordered_map>

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
    bool _focused;

    Slider(PID pid, float value) { _pid = pid, _rect = { 0 }, _val = value, _focused = false; }
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
    Slider _sliderMaster {};
    std::vector<Slider> _slidersApps;

public:
    void appSliderAdd(PID pid, float vol, bool muted);
    void appSliderRemove(PID pid);

    Slider* getSliderFromSelect(SelectInfo info);
    SelectInfo getSelectAtPosition(POINT mousePos);

    void recalculateSliderRects(const RECT& rect);
    void drawSliders(HDC hdc);
};

//
// FILE
//

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