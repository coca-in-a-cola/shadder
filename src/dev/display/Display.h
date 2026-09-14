#pragma once

#include <cstdint>

struct ScreenSize {
    int width = 800;
    int height = 800;
};

// Platform window abstraction. Rendering backends use GetNativeHandle() only
// inside their platform-specific implementation code.
class Display {
  public:
    virtual ~Display() = default;

    virtual bool Create(ScreenSize size) = 0;
    virtual void *GetNativeHandle() const = 0;
    [[nodiscard]] virtual ScreenSize GetScreenSize() const = 0;
    [[nodiscard]] virtual bool IsFullscreen() const = 0;
    virtual void ToggleFullscreen() = 0;

    // Messages are opaque outside a platform implementation.
    virtual bool HandleNativeMessage(std::uintptr_t message,
                                     std::uintptr_t wParam,
                                     std::intptr_t lParam) = 0;
};
