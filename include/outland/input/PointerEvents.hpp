#pragma once
#include <raylib.h>
#include <span>

namespace outland::input {
// Click capture: GLFW on desktop, a canvas pointer listener in browsers. Native Android touch
// polling stays unchanged. Keeps short clicks and taps that raylib's per-frame polling can miss.
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
