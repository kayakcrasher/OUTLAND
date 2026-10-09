#pragma once
#include <raylib.h>
#include <span>

namespace outland::input {
// Optional GLFW desktop click capture. Native Android touch polling stays unchanged.
// Keeps short X11 clicks that raylib's per-frame button-state polling can miss.
class PointerEvents {
  public:
    PointerEvents();
    ~PointerEvents();
    PointerEvents(const PointerEvents &) = delete;
    PointerEvents &operator=(const PointerEvents &) = delete;
    void begin_frame();
    static std::span<const Vector2> presses();
    bool installed() const { return installed_; }

  private:
    bool installed_{false};
};
} // namespace outland::input
