#pragma once

#include "shared/delegates/Delegates.h"
#include "shared/keycodes/Keys.h"
#include <SimpleMath.h>
#include <d3d11.h>
#include <windows.h>
#include <unordered_set>

class InputDevice {
  HWND hwnd_;

  std::unordered_set<Keys> keys;

public:
  struct MouseMoveEventArgs {
    DirectX::SimpleMath::Vector2 Position;
    DirectX::SimpleMath::Vector2 Offset;
    int WheelDelta;
  };

  DirectX::SimpleMath::Vector2 MousePosition{};
  DirectX::SimpleMath::Vector2 MouseOffset{};
  int MouseWheelDelta{};

  MulticastDelegate<const MouseMoveEventArgs &> MouseMove;

public:
  explicit InputDevice(HWND hwnd);
  ~InputDevice() = default;

  void AddPressedKey(Keys key);
  void RemovePressedKey(Keys key);
  bool IsKeyDown(Keys key);

protected:
  struct KeyboardInputEventArgs {
    USHORT MakeCode;
    USHORT Flags;
    USHORT VKey;
    UINT Message;
  };

  enum class MouseButtonFlags {
    LeftButtonDown  = 1,
    LeftButtonUp    = 2,
    RightButtonDown = 4,
    RightButtonUp   = 8,
    MiddleButtonDown = 16,
    MiddleButtonUp   = 32,
    Button1Down = LeftButtonDown,
    Button1Up   = LeftButtonUp,
    Button2Down = RightButtonDown,
    Button2Up   = RightButtonUp,
    Button3Down = MiddleButtonDown,
    Button3Up   = MiddleButtonUp,
    Button4Down = 64,
    Button4Up   = 128,
    Button5Down = 256,
    Button5Up   = 512,
    MouseWheel  = 1024,
    Hwheel      = 2048,
    None        = 0,
  };

  struct RawMouseEventArgs {
    int Mode;
    int ButtonFlags;
    int ExtraInformation;
    int Buttons;
    int WheelDelta;
    int X;
    int Y;
  };

  void OnKeyDown(KeyboardInputEventArgs args);
  void OnMouseMove(RawMouseEventArgs args);
};

/*
                case WM_INPUT:
                {
                        UINT dwSize = 0;
                        GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam),
   RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER)); LPBYTE lpb = new
   BYTE[dwSize]; if (lpb == nullptr) { return 0;
                        }

                        if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, lpb,
   &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
                                OutputDebugString(TEXT("GetRawInputData does not
   return correct size !\n"));

                        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb);

                        if (raw->header.dwType == RIM_TYPEKEYBOARD)
                        {
                                InputDevice->OnKeyDown({
                                        raw->data.keyboard.MakeCode,
                                        raw->data.keyboard.Flags,
                                        raw->data.keyboard.VKey,
                                        raw->data.keyboard.Message
                                });
                        }
                        else if (raw->header.dwType == RIM_TYPEMOUSE)
                        {
                                InputDevice->OnMouseMove({
                                        raw->data.mouse.usFlags,
                                        raw->data.mouse.usButtonFlags,
                                        static_cast<int>(raw->data.mouse.ulExtraInformation),
                                        static_cast<int>(raw->data.mouse.ulRawButtons),
                                        static_cast<short>(raw->data.mouse.usButtonData),
                                        raw->data.mouse.lLastX,
                                        raw->data.mouse.lLastY
                                });
                        }

                        delete[] lpb;
                        return DefWindowProc(hwnd, umessage, wparam, lparam);
                }
 */
