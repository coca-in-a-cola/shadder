#include "DisplayWin32.h"

DisplayWin32::DisplayWin32(LPCWSTR applicationName, HINSTANCE hInst, int screenWidth, int screenHeight, WNDPROC wndProc) : hInstance(hInst), hWnd(nullptr), wc{}, ClientWidth(screenWidth), ClientHeight(screenHeight) {
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		wc.lpfnWndProc = wndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
		wc.hIconSm = wc.hIcon;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
		wc.lpszMenuName = nullptr;
		wc.lpszClassName = applicationName;
		wc.cbSize = sizeof(WNDCLASSEX);

		RegisterClassEx(&wc);

		RECT windowRect = { 0, 0, static_cast<LONG>(screenWidth), static_cast<LONG>(screenHeight) };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		auto dwStyle = WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME;
		auto posX = (GetSystemMetrics(SM_CXSCREEN) - screenWidth) / 2;
		auto posY = (GetSystemMetrics(SM_CYSCREEN) - screenHeight) / 2;

		hWnd = CreateWindowEx(
						WS_EX_APPWINDOW, applicationName, applicationName, dwStyle, posX, posY,
						windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
						nullptr, nullptr, hInstance, nullptr);

		ShowWindow(hWnd, SW_SHOW);
		SetForegroundWindow(hWnd);
		SetFocus(hWnd);
		ShowCursor(true);
}
