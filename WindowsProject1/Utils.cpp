#include "Utils.h"

#include <psapi.h> // GetModuleBaseName
#include <shlobj.h>

//
// BOILERPLATE
//

namespace {
COLORREF getIconColor(BITMAP bmp, ICONINFO iconInfo)
{
    // 1. Setup the bitmap info header
    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight; // Negative for top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    // 2. Allocate memory for pixels
    int pixelCount = bmp.bmWidth * bmp.bmHeight;
    std::vector<DWORD> pixels(pixelCount);

    // 3. Get the raw bits
    HDC hdc = GetDC(NULL);
    GetDIBits(hdc, iconInfo.hbmColor, 0, bmp.bmHeight, &pixels[0], (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);

    // 4. Calculate Average
    long long totalR = 0, totalG = 0, totalB = 0;
    int opaquePixels = 0;

    for (int i = 0; i < pixelCount; i++) {
        BYTE a = (pixels[i] >> 24) & 0xFF;
        BYTE r = (pixels[i] >> 16) & 0xFF;
        BYTE g = (pixels[i] >> 8) & 0xFF;
        BYTE b = pixels[i] & 0xFF;

        if (a > 127) {
            totalR += r, totalG += g, totalB += b, opaquePixels++;
        }
    }

    if (opaquePixels > 0) {
        BYTE avgR = BYTE(totalR / opaquePixels);
        BYTE avgG = BYTE(totalG / opaquePixels);
        BYTE avgB = BYTE(totalB / opaquePixels);
        return RGB(avgR, avgG, avgB);
    }
    return RGB(160, 160, 160);
}

IconInfo createIconInfo(HICON icon)
{
    if (!icon)
        return {};

    ICONINFO iconInfo;
    GetIconInfo(icon, &iconInfo);

    BITMAP bmp;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    IconInfo info {};
    info.hBrush = CreateSolidBrush(getIconColor(bmp, iconInfo));
    info.hLarge = icon;
    info.width = bmp.bmWidth;
    return info;
}

std::wstring getProcessName(PID pid)
{
    if (pid == 0)
        return L"System Sounds";
    TCHAR szName[MAX_PATH] = TEXT("<unknown>");
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc) {
        GetModuleBaseName(hProc, NULL, szName, MAX_PATH);
        CloseHandle(hProc);
    }
    return szName;
};
}

//
// ICON
//

#pragma region ICON
void IconManager::uninit()
{
    for (auto& pair : cachedProcessIcons) {
        auto& iconInfo = pair.second;
        if (iconInfo.hLarge)
            DestroyIcon(iconInfo.hLarge);
    }
    DestroyIcon(iiMasterSpeaker.hLarge);
    DestroyIcon(iiMasterHeadphones.hLarge);
    DestroyIcon(iiSystemSounds.hLarge);
}

IconManager::IconManager()
{
    HICON hIcon = nullptr;
    wchar_t dllPathSource[MAX_PATH];
    GetSystemDirectoryW(dllPathSource, MAX_PATH);

    auto loadIcon = [&](const wchar_t* path, int index) -> IconInfo {
        wchar_t dllPath[MAX_PATH];
        wcscpy_s(dllPath, MAX_PATH, dllPathSource);
        wcscat_s(dllPath, path);
        ExtractIconExW(dllPath, index, &hIcon, nullptr, 1);
        return createIconInfo(hIcon);
    };

    iiMasterSpeaker = loadIcon(L"\\mmres.dll", 0);
    iiMasterHeadphones = loadIcon(L"\\mmres.dll", 2);
    iiSystemSounds = loadIcon(L"\\imageres.dll", 104);
    iiNoIconApp = loadIcon(L"\\imageres.dll", 11);
}

IconInfo IconManager::getIconFromProcess(PID pid)
{
    IconInfo result = {};

    auto foundIconIt = cachedProcessIcons.find(pid);
    if (foundIconIt == cachedProcessIcons.end()) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) {
            result = iiSystemSounds;
            return result;
        }

        wchar_t exePath[MAX_PATH];
        DWORD size = MAX_PATH;

        if (!QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
            CloseHandle(hProcess);
            return result;
        }

        CloseHandle(hProcess);

        HICON icon;
        UINT icons = ExtractIconExW(exePath, 0, &icon, nullptr, 1);
        if (icons == 0) {
            result = iiNoIconApp;
            return result;
        }

        result = createIconInfo(icon);

        cachedProcessIcons[pid] = result;

        wprintf(L"Stored new icon for: %s\n", exePath);
    } else {
        result = foundIconIt->second;
    }

    return result;
}

IconInfo IconManager::getIconMasterVol() { return iiMasterSpeaker; }
#pragma endregion

//
// SLIDER
//

#pragma region SLIDER

// #include <shellapi.h>
//  #pragma comment(lib, "Shell32.lib")

void Slider::draw(HDC hdc, bool isSystem) const
{
    float drawHeight = (_rect.bottom - _rect.top) * (1.f - _val);

    RECT drawRect {
        _rect.left + (_focused ? 0 : margin), _rect.top + LONG(drawHeight),
        _rect.right - (_focused ? 0 : margin), _rect.bottom
    };

    auto& im = IconManager::get();
    IconInfo iconInfo {};
    if (isSystem) {
        iconInfo = im.getIconMasterVol();
    } else {
        iconInfo = im.getIconFromProcess(_pid);
    }

    if (drawRect.right > drawRect.left && drawRect.bottom > drawRect.top)
        FillRect(hdc, &drawRect, iconInfo.hBrush);

    if (iconInfo.hLarge)
        DrawIconEx(hdc,
            (_rect.left + _rect.right) / 2 - iconInfo.width / 2,
            // (_rect.top + _rect.bottom) / 2 - iconInfo.width / 2,
            _rect.bottom - 30 - iconInfo.width / 2,
            iconInfo.hLarge, 0, 0, 0, NULL, DI_NORMAL);
}

//
// SLIDER MANAGER
//

Slider* SliderManager::getSliderFromSelect(SelectInfo info)
{
    if (info._type == VolumeType::Master)
        return &_sliderMaster;

    else if (info._type == VolumeType::App) {
        auto it = std::find_if(_slidersApps.begin(), _slidersApps.end(), [&](const Slider& s) { return s.getPID() == info._pid; });
        if (it != _slidersApps.end())
            return &*it;
    }

    return nullptr;
}

void SliderManager::appSliderAdd(PID pid, float vol, bool muted)
{
    auto it = std::find_if(_slidersApps.begin(), _slidersApps.end(), [&](const Slider& s) { return s.getPID() == pid; });
    _slidersApps.push_back(Slider(pid, vol));
}

void SliderManager::appSliderRemove(PID pid)
{
    auto it = std::find_if(_slidersApps.begin(), _slidersApps.end(), [&](const Slider& s) { return s.getPID() == pid; });
    _slidersApps.erase(it);
}

SelectInfo SliderManager::getSelectAtPosition(POINT mousePos)
{
    if (_sliderMaster.intersects(mousePos)) {
        return SelectInfo(VolumeType::Master, (PID)0);
    }

    for (int i = 0; i < _slidersApps.size(); ++i)
        if (_slidersApps.at(i).intersects(mousePos)) {
            return SelectInfo(VolumeType::App, _slidersApps.at(i).getPID());
        }

    return {};
}

void SliderManager::recalculateSliderRects(const RECT& r)
{
    int offset = r.left + margin;
    _sliderMaster._rect = { offset, r.top, offset += sliderWidth + 20, r.bottom };

    for (auto& slider : _slidersApps) {
        slider._rect = { offset, r.top, offset += sliderWidth, r.bottom };
    }
}

void SliderManager::drawSliders(HDC hdc)
{
    _sliderMaster.draw(hdc, true);
    for (auto& slider : _slidersApps)
        slider.draw(hdc);
}
#pragma endregion

//
// FILE
//

#pragma region FILE
FileManager::FileManager()
{
    PWSTR appDataPath = NULL;
    _iniPath = L"";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath))) {
        _iniPath = std::wstring(appDataPath) + L"\\VolumeApp.ini";
        CoTaskMemFree(appDataPath);
    }
}

constexpr int defaultX = 100, defaultY = 100, defaultWidth = 600, defaultHeight = 300;
void FileManager::loadWindowRect(RECT& rect) const
{
    std::wstring iniPath = _iniPath;
    if (iniPath.empty())
        return;

    rect.left = GetPrivateProfileIntW(L"Window", L"x", 100, iniPath.c_str());
    rect.top = GetPrivateProfileIntW(L"Window", L"y", 100, iniPath.c_str());
    int width = GetPrivateProfileIntW(L"Window", L"w", 800, iniPath.c_str());
    int height = GetPrivateProfileIntW(L"Window", L"h", 600, iniPath.c_str());
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;

    /* HMONITOR hMonitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
    if (hMonitor == NULL) {
        rect.left = defaultX;
        rect.top = defaultY;

    } else {
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            if (rect.right > mi.rcWork.right)
                rect.left = mi.rcWork.right - width;
            if (rect.bottom > mi.rcWork.bottom)
                rect.top = mi.rcWork.bottom - height;
            if (rect.left < mi.rcWork.left)
                rect.left = mi.rcWork.left;
            if (rect.top < mi.rcWork.top)
                rect.top = mi.rcWork.top;
        }
    } */
}

void FileManager::saveWindowRect(const RECT& rect) const
{
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    WritePrivateProfileStringW(L"Window", L"x", std::to_wstring(rect.left).c_str(), _iniPath.c_str());
    WritePrivateProfileStringW(L"Window", L"y", std::to_wstring(rect.top).c_str(), _iniPath.c_str());
    WritePrivateProfileStringW(L"Window", L"w", std::to_wstring(width).c_str(), _iniPath.c_str());
    WritePrivateProfileStringW(L"Window", L"h", std::to_wstring(height).c_str(), _iniPath.c_str());
}
#pragma endregion