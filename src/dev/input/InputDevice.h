#pragma once

#include "util/keycodes/Keys.h"
#include "util/callable/callable.h"
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

  struct KeyboardInputEventArgs {
    USHORT MakeCode;
    USHORT Flags;
    USHORT VKey;
    UINT Message;
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

  DirectX::SimpleMath::Vector2 MousePosition{};
  DirectX::SimpleMath::Vector2 MouseOffset{};
  int MouseWheelDelta{};

  Signal<MouseMoveEventArgs> MouseMove;

  explicit InputDevice(HWND hwnd);
  ~InputDevice() = default;

  void AddPressedKey(Keys key);
  void RemovePressedKey(Keys key);
  bool IsKeyDown(Keys key);

  void OnKeyDown(KeyboardInputEventArgs args);
  void OnMouseMove(RawMouseEventArgs args);

protected:
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
};
