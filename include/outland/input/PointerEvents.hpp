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
    // Mouse look: lock the pointer to the game (desktop: a hidden, centred cursor; browsers:
    // pointer lock, granted on the next click if not at once). Off frees it for menus.
    static void capture_mouse(bool want);
    [[nodiscard]] static bool mouse_captured();
    // How far the captured mouse moved since the last frame (pixels).
    [[nodiscard]] static Vector2 mouse_motion();
    // A phone or tablet (Android, or a browser whose main pointer is a finger).
    [[nodiscard]] static bool touch_screen();
    bool installed() const { return installed_; }

  private:
    bool installed_{false};
};
} // namespace outland::input
