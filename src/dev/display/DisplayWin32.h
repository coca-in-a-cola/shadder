#pragma once
#include "dev/display/Display.h"

#include <windows.h>
#include <string>

class DisplayWin32 final : public Display {
  public:
  DisplayWin32(LPCWSTR applicationName, HINSTANCE hInstance, WNDPROC wndProc);
  ~DisplayWin32() override;

  bool Create(ScreenSize size) override;
  void *GetNativeHandle() const override { return hWnd_; }
  [[nodiscard]] ScreenSize GetScreenSize() const override { return screenSize_; }
  [[nodiscard]] bool IsFullscreen() const override { return fullscreen_; }
  void ToggleFullscreen() override;
  bool HandleNativeMessage(std::uintptr_t message,
                           std::uintptr_t wParam,
                           std::intptr_t lParam) override;

  [[nodiscard]] HWND GetHWnd() const { return hWnd_; }

  private:
  std::wstring applicationName_;
  HINSTANCE hInstance_ = nullptr;
  WNDPROC wndProc_ = nullptr;
  HWND hWnd_ = nullptr;
  ATOM classAtom_ = 0;
  ScreenSize screenSize_{};
  bool fullscreen_ = false;
  DWORD windowStyle_ = 0;
  DWORD windowExStyle_ = 0;
  WINDOWPLACEMENT windowPlacement_{sizeof(WINDOWPLACEMENT)};
};
