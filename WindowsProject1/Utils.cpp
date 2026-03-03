#include "Utils.h"

#include <psapi.h> // GetModuleBaseName
#include <shlobj.h>

FileManager::FileManager()
{
    PWSTR appDataPath = NULL;
    _iniPath = L"";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath))) {
        _iniPath = std::wstring(appDataPath) + L"\\YourAppName.ini";
        CoTaskMemFree(appDataPath);
    }
}

void FileManager::loadWindowRect(HWND hwnd) const
{
    std::wstring iniPath = _iniPath;
    if (iniPath.empty())
        return;

    RECT rect;
    rect.left = GetPrivateProfileIntW(L"Window", L"x", 100, iniPath.c_str());
    rect.top = GetPrivateProfileIntW(L"Window", L"y", 100, iniPath.c_str());
    int width = GetPrivateProfileIntW(L"Window", L"w", 800, iniPath.c_str());
    int height = GetPrivateProfileIntW(L"Window", L"h", 600, iniPath.c_str());
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;

    HMONITOR hMonitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
    if (hMonitor == NULL) {
        rect.left = 100;
        rect.top = 100;

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
    }

    SetWindowPos(hwnd, NULL, rect.left, rect.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void FileManager::saveWindowRect(HWND hwnd) const
{
    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        std::wstring iniPath = _iniPath;
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        WritePrivateProfileStringW(L"Window", L"x", std::to_wstring(rect.left).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"Window", L"y", std::to_wstring(rect.top).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"Window", L"w", std::to_wstring(width).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"Window", L"h", std::to_wstring(height).c_str(), iniPath.c_str());
    }
}

std::wstring GetProcessName(PID pid)
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

static IconInfo createIconInfo(HICON icon)
{
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
        if (icons == 0)
            return result;

        result = createIconInfo(icon);

        cachedProcessIcons[pid] = result;

        wprintf(L"Stored new icon for: %s\n", exePath);
    } else {
        result = foundIconIt->second;
    }

    return result;
}

IconInfo IconManager::getIconMasterVol() { return iiMasterSpeaker; }

//
// SLIDER
//

// #include <shellapi.h>
//  #pragma comment(lib, "Shell32.lib")

void Slider::draw(HDC hdc, bool isSystem) const
{
    float drawHeight = (_rect.bottom - _rect.top) * (1.f - _val);

    RECT drawRect {
        _rect.left + margin, _rect.top + LONG(drawHeight),
        _rect.right - margin, _rect.bottom
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
            _rect.left + sliderWidth / 2 - iconInfo.width / 2,
            drawRect.bottom - sliderWidth / 2 - iconInfo.width / 4,
            iconInfo.hLarge, 0, 0, 0, NULL, DI_NORMAL);
}

//
// USER INTERFACE MANAGER
//

Slider* SliderManager::getGetBySelectInfo(SelectInfo info)
{
    if (info._type == VolumeType::Master)
        return &sliderMaster;

    else if (info._type == VolumeType::App) {
        auto it = std::find_if(slidersApp.begin(), slidersApp.end(), [&](const Slider& s) { return s.getPID() == info._pid; });
        if (it != slidersApp.end())
            return &*it;
    }

    return nullptr;
}

void SliderManager::addAppSlider(PID pid, float vol, bool muted)
{
    auto it = std::find_if(slidersApp.begin(), slidersApp.end(), [&](const Slider& s) { return s.getPID() == pid; });
    slidersApp.push_back(Slider(pid, vol));
}

void SliderManager::removeAppSlider(PID pid)
{
    auto it = std::find_if(slidersApp.begin(), slidersApp.end(), [&](const Slider& s) { return s.getPID() == pid; });
    slidersApp.erase(it);
}

SelectInfo SliderManager::getHoveredSlider(POINT mousePos)
{
    if (sliderMaster.intersects(mousePos)) {
        return SelectInfo(VolumeType::Master, (PID)0);
    }

    for (int i = 0; i < slidersApp.size(); ++i)
        if (slidersApp.at(i).intersects(mousePos)) {
            return SelectInfo(VolumeType::App, slidersApp.at(i).getPID());
        }

    return {};
}

void SliderManager::recalculateSliderRects(const RECT& r)
{
    sliderMaster._rect = { r.left, r.top, r.left + sliderWidth, r.bottom };
    int offset = sliderMaster._rect.right + 30;
    for (auto& slider : slidersApp) {
        slider._rect = { offset, r.top, offset + sliderWidth, r.bottom };
        offset += sliderWidth;
    }
}

void SliderManager::drawSliders(HDC hdc)
{
    sliderMaster.draw(hdc, true);
    for (auto& slider : slidersApp)
        slider.draw(hdc);
}
