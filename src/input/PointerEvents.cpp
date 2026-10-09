#include "outland/input/PointerEvents.hpp"
#include <array>
#include <cmath>
#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

struct GLFWwindow;
namespace outland::input {
namespace {
constexpr std::size_t limit = 8;
std::array<Vector2, limit> pending{}, frame{};
std::size_t pending_count = 0, frame_count = 0;
using MouseCallback = void (*)(GLFWwindow *, int, int, int);
using SetCallback = MouseCallback (*)(GLFWwindow *, MouseCallback);
using GetCursor = void (*)(GLFWwindow *, double *, double *);
GLFWwindow *window = nullptr;
MouseCallback previous = nullptr;
SetCallback set_callback = nullptr;
GetCursor get_cursor = nullptr;
void capture(GLFWwindow *source, int button, int action, int mods) {
    if (source == window && button == MOUSE_BUTTON_LEFT && action == 1 && pending_count < limit) {
        double x = 0, y = 0;
        get_cursor(source, &x, &y);
        if (std::isfinite(x) && std::isfinite(y))
            pending[pending_count++] = {static_cast<float>(x), static_cast<float>(y)};
    }
    // Raylib still receives every button event, including release and other buttons.
    if (previous)
        previous(source, button, action, mods);
}
} // namespace
PointerEvents::PointerEvents() {
#if defined(__unix__) || defined(__APPLE__)
    if (window)
        return;
    // Termux raylib links external GLFW. Bundled/hidden GLFW and native Android
    // simply use the existing raylib/native input path; no extra library is loaded.
    const auto context =
        reinterpret_cast<GLFWwindow *(*)()>(dlsym(RTLD_DEFAULT, "glfwGetCurrentContext"));
    set_callback = reinterpret_cast<SetCallback>(dlsym(RTLD_DEFAULT, "glfwSetMouseButtonCallback"));
    get_cursor = reinterpret_cast<GetCursor>(dlsym(RTLD_DEFAULT, "glfwGetCursorPos"));
    if (!context || !set_callback || !get_cursor)
        return;
    window = context();
    if (!window)
        return;
    previous = set_callback(window, capture);
    installed_ = true;
#endif
}
PointerEvents::~PointerEvents() {
    if (!installed_)
        return;
    // Preserve another callback installed after ours, should another subsystem do so.
    const auto current = set_callback(window, previous);
    if (current != capture)
        set_callback(window, current);
    window = nullptr;
    previous = nullptr;
    pending_count = frame_count = 0;
}
void PointerEvents::begin_frame() {
    frame_count = pending_count;
    frame = pending;
    pending_count = 0;
}
std::span<const Vector2> PointerEvents::presses() { return {frame.data(), frame_count}; }
} // namespace outland::input
