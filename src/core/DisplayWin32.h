#pragma once

#include <windows.h>

class DisplayWin32 {
public:
    DisplayWin32(LPCWSTR applicationName, HINSTANCE hInstance, int screenWidth, int screenHeight, WNDPROC wndProc);
    ~DisplayWin32();

    HWND hWnd;
    HINSTANCE hInstance;
    HMODULE Module;
    WNDCLASSEX wc;
    int ClientWidth;
    int ClientHeight;
};
