#pragma once
#include <windows.h>

class DisplayWin32 {
public:
    DisplayWin32(LPCWSTR applicationName, HINSTANCE hInstance, int screenWidth, int screenHeight, WNDPROC wndProc);
    ~DisplayWin32() = default;

    HWND hWnd;
    HINSTANCE hInstance;
    WNDCLASSEX wc;
    int ClientWidth;
    int ClientHeight;

    int GetWidth() const { return ClientWidth; }
    int GetHeight() const { return ClientHeight; }
};
