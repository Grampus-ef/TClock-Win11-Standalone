// TClock11 Standalone compatibility prototype
// Copyright (C) 2026. Distributed under GPL-3.0-or-later, see ../LICENSE.
// A separate-process taskbar overlay. This does NOT inject into explorer.exe.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <cstdio>
#include <cmath>
#include <cstdint>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace {
constexpr UINT_PTR kRefreshTimer = 1;
constexpr UINT_PTR kGeometryTimer = 2;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kMenuReload = 1001;
constexpr UINT kMenuSettings = 1002;
constexpr UINT kMenuExit = 1003;
constexpr UINT kTrayId = 17;

struct Config {
    int width = 156;
    int height = 42;
    int offset = 10; // pixel distance from far edge, right/bottom
    int shiftX = 0;
    int shiftY = 0;
    int fontSize = 14;
    bool bold = false;
    bool showCpu = true;
    bool showMemory = true;
    bool showNetwork = false;
    bool showMeter = true;
    bool showInFullscreen = false;
    bool useOriginalFormat = false;
    std::wstring font = L"Yu Gothic UI";
    std::wstring format = L"yyyy/mm/dd ddd\\nhh:nn:ss";
    COLORREF foreground = RGB(255,255,255);
    COLORREF background = RGB(35,35,35);
    COLORREF meter = RGB(110,205,126);
};

Config g_cfg;
HWND g_hwnd = nullptr;
std::wstring g_ini;
HFONT g_font = nullptr;
UINT g_taskbarCreated = 0;
ULONGLONG g_lastIdle = 0, g_lastKernel = 0, g_lastUser = 0;
unsigned g_cpu = 0, g_mem = 0;
uint64_t g_bytesIn = 0, g_bytesOut = 0;
double g_down = 0, g_up = 0;
ULONGLONG g_lastNetworkTime = 0;

std::wstring DirOfExe() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    auto pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : p.substr(0,pos);
}

std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* fallback) {
    wchar_t buf[2048] = {};
    GetPrivateProfileStringW(section, key, fallback, buf, 2048, g_ini.c_str());
    return buf;
}
int ReadInteger(const wchar_t* section, const wchar_t* key, int fallback) {
    return static_cast<int>(GetPrivateProfileIntW(section,key,fallback,g_ini.c_str()));
}
COLORREF ReadColor(const wchar_t* name, COLORREF fallback) {
    std::wstring value=ReadString(L"Appearance",name,L"");
    if (value.size()!=7 || value[0]!=L'#') return fallback;
    unsigned r,g,b;
    if(swscanf_s(value.c_str()+1,L"%2x%2x%2x",&r,&g,&b)!=3) return fallback;
    return RGB(r,g,b);
}
void ReloadConfig() {
    Config cfg;
    cfg.width = std::clamp(ReadInteger(L"Placement",L"Width",cfg.width),72,600);
    cfg.height = std::clamp(ReadInteger(L"Placement",L"Height",cfg.height),18,120);
    cfg.offset = std::clamp(ReadInteger(L"Placement",L"EdgeInset",cfg.offset),0,150);
    cfg.shiftX = std::clamp(ReadInteger(L"Placement",L"ShiftX",0),-1000,1000);
    cfg.shiftY = std::clamp(ReadInteger(L"Placement",L"ShiftY",0),-1000,1000);
    cfg.fontSize = std::clamp(ReadInteger(L"Appearance",L"FontSize",cfg.fontSize),7,40);
    cfg.bold = ReadInteger(L"Appearance",L"Bold",0)!=0;
    cfg.foreground=ReadColor(L"Foreground",cfg.foreground);
    cfg.background=ReadColor(L"Background",cfg.background);
    cfg.meter=ReadColor(L"Meter",cfg.meter);
    cfg.font=ReadString(L"Appearance",L"Font",L"Yu Gothic UI");
    cfg.format=ReadString(L"Clock",L"Format",L"yyyy/mm/dd ddd\\nhh:nn:ss");
    cfg.showCpu=ReadInteger(L"Statistics",L"CPU",1)!=0;
    cfg.showMemory=ReadInteger(L"Statistics",L"Memory",1)!=0;
    cfg.showNetwork=ReadInteger(L"Statistics",L"Network",0)!=0;
    cfg.showMeter=ReadInteger(L"Statistics",L"CPUBar",1)!=0;
    cfg.showInFullscreen=ReadInteger(L"Placement",L"ShowInFullscreen",0)!=0;
    cfg.useOriginalFormat=ReadInteger(L"Compatibility",L"UseOriginalFormat",0)!=0;
    if(cfg.useOriginalFormat) {
        std::wstring oldPath=ReadString(L"Compatibility",L"OriginalIni",L"tclock-win10.ini");
        if(oldPath.find(L':')==std::wstring::npos && oldPath.rfind(L"\\",0)!=0) oldPath=DirOfExe()+L"\\"+oldPath;
        DWORD attr=GetFileAttributesW(oldPath.c_str());
        if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_DIRECTORY)==0) {
            wchar_t original[2048]={};
            GetPrivateProfileStringW(L"Format",L"Format",L"",original,2048,oldPath.c_str());
            if(original[0]) cfg.format=original;
        }
    }
    g_cfg=cfg;
    if(g_font) { DeleteObject(g_font); g_font=nullptr; }
    // Per-monitor DPI awareness requires the DPI of this window, not a display DC.
    const UINT dpi = g_hwnd ? GetDpiForWindow(g_hwnd) : 96;
    g_font=CreateFontW(-MulDiv(cfg.fontSize,static_cast<int>(dpi),72),0,0,0,cfg.bold?FW_BOLD:FW_NORMAL,
        FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,DEFAULT_PITCH,cfg.font.c_str());
}

unsigned TwoDigits(unsigned x) { return x%100; }
void AppendNumber(std::wstring& output,int number,int minDigits) {
    wchar_t b[40];
    swprintf_s(b,L"%0*d",minDigits,number);
    output+=b;
}
std::wstring RenderFormat(const SYSTEMTIME& time) {
    // TClock-compatible subset: yyyy, yy, mm, m, dd, ddd, dddd, aaa, aaaa, hh, h, nn, n, ss, s.
    // Invalid/unimplemented original TClock macros remain visible deliberately.
    const wchar_t* shortDays[]={L"日",L"月",L"火",L"水",L"木",L"金",L"土"};
    const wchar_t* longDays[]={L"日曜日",L"月曜日",L"火曜日",L"水曜日",L"木曜日",L"金曜日",L"土曜日"};
    std::wstring result;
    const std::wstring& f=g_cfg.format;
    for(size_t i=0;i<f.size();) {
        if(f[i]==L'\\' && i+1<f.size() && f[i+1]==L'n') { result+=L'\n'; i+=2; continue; }
        if(f[i]==L'\\' && i+1<f.size() && f[i+1]==L'\\') { result+=L'\\'; i+=2; continue; }
        // Original format.c evaluates tokens inside <% ... %>. The standalone
        // renderer supports only the documented subset below.
        if(f.compare(i,2,L"<%") == 0 || f.compare(i,2,L"%>")==0) {i+=2;continue;}
        wchar_t c=static_cast<wchar_t>(towlower(f[i]));
        size_t j=i;
        while(j<f.size() && towlower(f[j])==c) ++j;
        size_t count=j-i;
        switch(c) {
        case L'y': if(count>=4) AppendNumber(result,time.wYear,4); else if(count==2) AppendNumber(result,TwoDigits(time.wYear),2); else AppendNumber(result,time.wYear,1); break;
        case L'm': AppendNumber(result,time.wMonth,count>=2?2:1); break;
        case L'a':
            if(count==3) result+=shortDays[time.wDayOfWeek];
            else if(count>=4) result+=longDays[time.wDayOfWeek];
            else result.append(f,i,count);
            break;
        case L'd':
            if(count>=4) result+=longDays[time.wDayOfWeek];
            else if(count==3) result+=shortDays[time.wDayOfWeek];
            else AppendNumber(result,time.wDay,count>=2?2:1);
            break;
        case L'h': AppendNumber(result,time.wHour,count>=2?2:1); break;
        case L'n': AppendNumber(result,time.wMinute,count>=2?2:1); break;
        case L's': AppendNumber(result,time.wSecond,count>=2?2:1); break;
        default: result.append(f,i,count); break;
        }
        i=j;
    }
    return result;
}

uint64_t QuadTo64(const FILETIME& t) {
    ULARGE_INTEGER v; v.LowPart=t.dwLowDateTime; v.HighPart=t.dwHighDateTime; return v.QuadPart;
}
void SampleCpuAndMemory() {
    FILETIME idle{},kernel{},user{};
    if(GetSystemTimes(&idle,&kernel,&user)) {
        const auto i=QuadTo64(idle), k=QuadTo64(kernel), u=QuadTo64(user);
        if(g_lastKernel || g_lastUser) {
            const auto di=i>=g_lastIdle?i-g_lastIdle:0;
            const auto dt=(k>=g_lastKernel?k-g_lastKernel:0)+(u>=g_lastUser?u-g_lastUser:0);
            if(dt>0) g_cpu=static_cast<unsigned>(std::clamp(100.0*(1.0-static_cast<double>(di)/dt),0.0,100.0));
        }
        g_lastIdle=i;g_lastKernel=k;g_lastUser=u;
    }
    MEMORYSTATUSEX st{};st.dwLength=sizeof(st);
    if(GlobalMemoryStatusEx(&st)) g_mem=st.dwMemoryLoad;
}
void SampleNetwork() {
    PMIB_IF_TABLE2 table=nullptr;
    if(GetIfTable2(&table)!=NO_ERROR) return;
    uint64_t rx=0,tx=0;
    for(ULONG i=0;i<table->NumEntries;i++) {
        const MIB_IF_ROW2& row=table->Table[i];
        if(row.OperStatus!=IfOperStatusUp || row.Type==IF_TYPE_SOFTWARE_LOOPBACK) continue;
        rx+=row.InOctets;
        tx+=row.OutOctets;
    }
    FreeMibTable(table);
    const ULONGLONG now=GetTickCount64();
    if(g_lastNetworkTime && now>g_lastNetworkTime && rx>=g_bytesIn && tx>=g_bytesOut) {
        const double seconds=(now-g_lastNetworkTime)/1000.0;
        g_down=(rx-g_bytesIn)/seconds;g_up=(tx-g_bytesOut)/seconds;
    } else {g_down=0;g_up=0;}
    g_bytesIn=rx;g_bytesOut=tx;g_lastNetworkTime=now;
}

std::wstring Rate(double bytes) {
    wchar_t buf[64];
    if(bytes >= 1024.0*1024) swprintf_s(buf,L"%.1fM",bytes/1024.0/1024.0);
    else if(bytes>=1024) swprintf_s(buf,L"%.0fK",bytes/1024.0);
    else swprintf_s(buf,L"%.0fB",bytes);
    return buf;
}

bool RectCoversMonitor(const RECT& rect,const RECT& monitor) {
    // Ignore maximized windows that do not cover the taskbar (working area only).
    return rect.left<=monitor.left+2 && rect.top<=monitor.top+2 &&
        rect.right>=monitor.right-2 && rect.bottom>=monitor.bottom-2;
}
bool IsFullscreenOnTaskbarMonitor(HWND taskbar) {
    if(g_cfg.showInFullscreen) return false;
    HWND fg=GetForegroundWindow();
    if(!fg || fg==g_hwnd || fg==taskbar || IsIconic(fg) || !IsWindowVisible(fg))return false;
    if(GetWindow(fg,GW_OWNER)!=nullptr) return false;
    wchar_t klass[80]={};GetClassNameW(fg,klass,80);
    if(wcscmp(klass,L"Progman")==0 || wcscmp(klass,L"WorkerW")==0 ||
        wcscmp(klass,L"Shell_TrayWnd")==0) return false;
    HMONITOR m=MonitorFromWindow(taskbar,MONITOR_DEFAULTTONEAREST);
    if(MonitorFromWindow(fg,MONITOR_DEFAULTTONEAREST)!=m)return false;
    MONITORINFO info{};info.cbSize=sizeof(info);
    RECT r{};
    return GetMonitorInfoW(m,&info) && GetWindowRect(fg,&r) && RectCoversMonitor(r,info.rcMonitor);
}

void UpdatePosition() {
    if(!g_hwnd)return;
    HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);
    if(!taskbar || !IsWindowVisible(taskbar) || IsFullscreenOnTaskbarMonitor(taskbar)) {
        ShowWindow(g_hwnd,SW_HIDE);return;
    }
    RECT task{};MONITORINFO mi{};mi.cbSize=sizeof(mi);
    if(!GetWindowRect(taskbar,&task) || !GetMonitorInfoW(MonitorFromWindow(taskbar,MONITOR_DEFAULTTONEAREST),&mi)) {
        ShowWindow(g_hwnd,SW_HIDE);return;
    }
    // On an autohidden taskbar don't paint a floating overlay on the desktop.
    APPBARDATA abd{};abd.cbSize=sizeof(abd);
    if(SHAppBarMessage(ABM_GETSTATE,&abd)&ABS_AUTOHIDE) {
        POINT p;GetCursorPos(&p);
        if(!PtInRect(&task,p)) {ShowWindow(g_hwnd,SW_HIDE);return;}
    }
    RECT mr=mi.rcMonitor;
    int tWidth=task.right-task.left, tHeight=task.bottom-task.top;
    // Detect orientation from actual visible geometry, not undocumented XAML controls.
    bool horizontal=tWidth>=tHeight;
    int x,y;
    if(horizontal) {
        x=task.right-g_cfg.width-g_cfg.offset+g_cfg.shiftX;
        y=task.top+(tHeight-g_cfg.height)/2+g_cfg.shiftY;
    } else {
        x=task.left+(tWidth-g_cfg.width)/2+g_cfg.shiftX;
        y=task.bottom-g_cfg.height-g_cfg.offset+g_cfg.shiftY;
    }
    // Keep the overlay within the physical monitor to avoid off-screen interaction.
    // RECT uses LONG; std::clamp requires all arguments to have the same type.
    const int minX = static_cast<int>(mr.left);
    const int minY = static_cast<int>(mr.top);
    const int maxX = std::max(minX, static_cast<int>(mr.right) - g_cfg.width);
    const int maxY = std::max(minY, static_cast<int>(mr.bottom) - g_cfg.height);
    x = std::clamp(x, minX, maxX);
    y = std::clamp(y, minY, maxY);
    SetWindowPos(g_hwnd,HWND_TOPMOST,x,y,g_cfg.width,g_cfg.height,
        SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_SHOWWINDOW);
}

void OpenNotificationCenter() {
    // Windows 11 documented keyboard shortcut Win+N.
    INPUT keys[4]={};
    keys[0].type=INPUT_KEYBOARD;keys[0].ki.wVk=VK_LWIN;
    keys[1].type=INPUT_KEYBOARD;keys[1].ki.wVk='N';
    keys[2].type=INPUT_KEYBOARD;keys[2].ki.wVk='N';keys[2].ki.dwFlags=KEYEVENTF_KEYUP;
    keys[3].type=INPUT_KEYBOARD;keys[3].ki.wVk=VK_LWIN;keys[3].ki.dwFlags=KEYEVENTF_KEYUP;
    SendInput(4,keys,sizeof(INPUT));
}

void AddTrayIcon() {
    NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=g_hwnd;
    data.uID=kTrayId;data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
    data.uCallbackMessage=kTrayMessage;
    data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wcscpy_s(data.szTip,L"TClock11 Standalone (prototype)");
    Shell_NotifyIconW(NIM_ADD,&data);
    data.uVersion=NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION,&data);
}
void RemoveTrayIcon() {
    NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=g_hwnd;data.uID=kTrayId;
    Shell_NotifyIconW(NIM_DELETE,&data);
}
void ShowContextMenu() {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,kMenuReload,L"Reload configuration");
    AppendMenuW(menu,MF_STRING,kMenuSettings,L"Open configuration file");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,kMenuExit,L"Exit TClock11");
    POINT pos{};GetCursorPos(&pos);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu,TPM_RIGHTBUTTON,pos.x,pos.y,0,g_hwnd,nullptr);
    DestroyMenu(menu);
}

void Draw(HDC target,RECT rect) {
    HBRUSH bg=CreateSolidBrush(g_cfg.background);
    FillRect(target,&rect,bg);DeleteObject(bg);
    SelectObject(target,g_font?g_font:GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(target,TRANSPARENT);
    SetTextColor(target,g_cfg.foreground);
    SYSTEMTIME st{};GetLocalTime(&st);
    std::wstring text=RenderFormat(st);
    if(g_cfg.showCpu || g_cfg.showMemory || g_cfg.showNetwork) {
        text+=L"\n";
        if(g_cfg.showCpu)text+=L"CPU "+std::to_wstring(g_cpu)+L"% ";
        if(g_cfg.showMemory)text+=L"MEM "+std::to_wstring(g_mem)+L"% ";
        if(g_cfg.showNetwork)text+=L"D "+Rate(g_down)+L"/s U "+Rate(g_up)+L"/s";
    }
    RECT textRect=rect;
    textRect.left+=2;textRect.right-=2;
    textRect.bottom-=(g_cfg.showCpu && g_cfg.showMeter)?5:0;
    DrawTextW(target,text.c_str(),-1,&textRect,DT_CENTER|DT_VCENTER|DT_NOPREFIX);
    if(g_cfg.showCpu && g_cfg.showMeter) {
        RECT meter={rect.left+1,rect.bottom-4,rect.right-1,rect.bottom-1};
        HBRUSH shade=CreateSolidBrush(RGB(70,70,70));FillRect(target,&meter,shade);DeleteObject(shade);
        meter.right=meter.left+(meter.right-meter.left)*g_cpu/100;
        HBRUSH bar=CreateSolidBrush(g_cfg.meter);FillRect(target,&meter,bar);DeleteObject(bar);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==g_taskbarCreated && g_taskbarCreated!=0) {AddTrayIcon();UpdatePosition();return 0;}
    switch(msg) {
    case WM_CREATE:
        g_hwnd=hwnd;
        ReloadConfig();
        SampleCpuAndMemory();SampleNetwork();
        AddTrayIcon();
        SetTimer(hwnd,kRefreshTimer,1000,nullptr);
        SetTimer(hwnd,kGeometryTimer,2500,nullptr);
        UpdatePosition();return 0;
    case WM_TIMER:
        if(wp==kRefreshTimer) {
            SampleCpuAndMemory();if(g_cfg.showNetwork)SampleNetwork();
            InvalidateRect(hwnd,nullptr,FALSE);
        } else if(wp==kGeometryTimer) UpdatePosition();
        return 0;
    case WM_DPICHANGED:
        // Recreate the font at the new monitor DPI before painting again.
        ReloadConfig();UpdatePosition();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE:
        UpdatePosition();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);
        RECT r{};GetClientRect(hwnd,&r);
        HDC mem=CreateCompatibleDC(dc);
        HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right-r.left,r.bottom-r.top);
        if(mem && bitmap) {
            HGDIOBJ old=SelectObject(mem,bitmap);Draw(mem,r);
            BitBlt(dc,0,0,r.right-r.left,r.bottom-r.top,mem,0,0,SRCCOPY);
            SelectObject(mem,old);
        } else Draw(dc,r);
        if(bitmap)DeleteObject(bitmap);if(mem)DeleteDC(mem);
        EndPaint(hwnd,&ps);return 0;
    }
    case WM_LBUTTONUP:OpenNotificationCenter();return 0;
    case WM_LBUTTONDBLCLK:
        ShellExecuteW(nullptr,L"open",L"ms-settings:dateandtime",nullptr,nullptr,SW_SHOWNORMAL);return 0;
    case WM_RBUTTONUP:ShowContextMenu();return 0;
    case kTrayMessage:
        if(LOWORD(lp)==WM_CONTEXTMENU || LOWORD(lp)==WM_RBUTTONUP) ShowContextMenu();
        else if(LOWORD(lp)==NIN_SELECT || LOWORD(lp)==WM_LBUTTONUP) OpenNotificationCenter();
        return 0;
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case kMenuReload:ReloadConfig();UpdatePosition();InvalidateRect(hwnd,nullptr,TRUE);break;
        case kMenuSettings:ShellExecuteW(nullptr,L"open",g_ini.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
        case kMenuExit:DestroyWindow(hwnd);break;
        }return 0;
    case WM_DESTROY:
        RemoveTrayIcon();KillTimer(hwnd,kRefreshTimer);KillTimer(hwnd,kGeometryTimer);
        if(g_font) {DeleteObject(g_font);g_font=nullptr;}
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\TClock11StandalonePrototype");
    if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) {CloseHandle(mutex);return 0;}
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_ini=DirOfExe()+L"\\TClock11Standalone.ini";
    g_taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);
    cls.lpfnWndProc=WndProc;cls.hInstance=instance;
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    cls.lpszClassName=L"TClock11StandalonePrototypeClass";
    cls.style=CS_DBLCLKS;
    if(!RegisterClassExW(&cls)) {CloseHandle(mutex);return 1;}
    HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,
        cls.lpszClassName,L"TClock11 Standalone",WS_POPUP,0,0,150,40,
        nullptr,nullptr,instance,nullptr);
    if(!hwnd) {CloseHandle(mutex);return 1;}
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    CloseHandle(mutex);
    return static_cast<int>(msg.wParam);
}
