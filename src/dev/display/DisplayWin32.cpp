#include "DisplayWin32.h"

DisplayWin32::DisplayWin32(LPCWSTR applicationName, HINSTANCE hInstance, WNDPROC wndProc)
    : applicationName_(applicationName), hInstance_(hInstance), wndProc_(wndProc) {
}

DisplayWin32::~DisplayWin32() {
    if (hWnd_) {
        DestroyWindow(hWnd_);
    }
    if (classAtom_) {
        UnregisterClass(applicationName_.c_str(), hInstance_);
    }
}

bool DisplayWin32::Create(ScreenSize size) {
    if (hWnd_ || size.width <= 0 || size.height <= 0) return false;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wndProc_;
    wc.hInstance = hInstance_;
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = applicationName_.c_str();
    classAtom_ = RegisterClassEx(&wc);
    if (!classAtom_ && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    windowStyle_ = WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME;
    windowExStyle_ = WS_EX_APPWINDOW;
    RECT windowRect = {0, 0, size.width, size.height};
    AdjustWindowRectEx(&windowRect, windowStyle_, FALSE, windowExStyle_);
    const int posX = (GetSystemMetrics(SM_CXSCREEN) - size.width) / 2;
    const int posY = (GetSystemMetrics(SM_CYSCREEN) - size.height) / 2;

    hWnd_ = CreateWindowEx(windowExStyle_, applicationName_.c_str(), applicationName_.c_str(),
                           windowStyle_, posX, posY,
                           windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
                           nullptr, nullptr, hInstance_, nullptr);
    if (!hWnd_) return false;

    screenSize_ = size;
    ShowWindow(hWnd_, SW_SHOW);
    SetForegroundWindow(hWnd_);
    SetFocus(hWnd_);
    ShowCursor(TRUE);
    return true;
}

void DisplayWin32::ToggleFullscreen() {
    if (!hWnd_) return;

    if (!fullscreen_) {
        windowStyle_ = static_cast<DWORD>(GetWindowLongPtr(hWnd_, GWL_STYLE));
        windowExStyle_ = static_cast<DWORD>(GetWindowLongPtr(hWnd_, GWL_EXSTYLE));
        windowPlacement_.length = sizeof(windowPlacement_);
        GetWindowPlacement(hWnd_, &windowPlacement_);

        const HMONITOR monitor = MonitorFromWindow(hWnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {sizeof(monitorInfo)};
        GetMonitorInfo(monitor, &monitorInfo);
        SetWindowLongPtr(hWnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd_, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                     monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        fullscreen_ = true;
    } else {
        SetWindowLongPtr(hWnd_, GWL_STYLE, windowStyle_);
        SetWindowLongPtr(hWnd_, GWL_EXSTYLE, windowExStyle_);
        SetWindowPlacement(hWnd_, &windowPlacement_);
        SetWindowPos(hWnd_, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER);
        fullscreen_ = false;
    }
}

bool DisplayWin32::HandleNativeMessage(std::uintptr_t message,
                                       std::uintptr_t wParam,
                                       std::intptr_t lParam) {
    switch (message) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                const int width = LOWORD(static_cast<DWORD_PTR>(lParam));
                const int height = HIWORD(static_cast<DWORD_PTR>(lParam));
                if (width > 0 && height > 0) screenSize_ = {width, height};
            }
            return true;
        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN && (lParam & (static_cast<std::intptr_t>(1) << 29))) {
                ToggleFullscreen();
                return true;
            }
            return false;
        default:
            return false;
    }
}
