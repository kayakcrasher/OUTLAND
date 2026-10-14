#include "outland/input/PointerEvents.hpp"
#include <array>
#include <cmath>
#if (defined(__unix__) || defined(__APPLE__)) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include <dlfcn.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Browsers: raylib polls button and touch state once per frame, so a click or tap whose press
// and release land between two frames (a quick click, or any click while a frame is slow) is
// lost. A listener on the canvas queues every press; begin_frame() collects them. A plain
// addEventListener leaves raylib's own emscripten/GLFW callbacks in place.
EM_JS(int, outland_pointer_install, (), {
    var canvas = Module['canvas'];
    if (!canvas || !canvas.addEventListener) return 0;
    if (Module['outlandPresses']) return 1;
    var queue = Module['outlandPresses'] = [];
    var down = function(x, y) {
        var rect = canvas.getBoundingClientRect();
        if (!rect.width || !rect.height || queue.length >= 16) return;
        queue.push((x - rect.left) / rect.width, (y - rect.top) / rect.height);
    };
    if (window.PointerEvent) {
        canvas.addEventListener('pointerdown', function(e) { if (e.button === 0) down(e.clientX, e.clientY); }, true);
    } else {
        canvas.addEventListener('mousedown', function(e) { if (e.button === 0) down(e.clientX, e.clientY); }, true);
        canvas.addEventListener('touchstart', function(e) {
            for (var i = 0; i < e.changedTouches.length; ++i) down(e.changedTouches[i].clientX, e.changedTouches[i].clientY);
        }, {capture: true, passive: true});
    }
    return 1;
});
// Moves up to `max` queued presses (fractions of the canvas, x then y) into `out`; returns the count.
EM_JS(int, outland_pointer_drain, (float* out, int max), {
    var queue = Module['outlandPresses'];
    if (!queue || !queue.length) return 0;
    var n = Math.min(max, queue.length / 2);
    for (var i = 0; i < n * 2; ++i) HEAPF32[(out >> 2) + i] = queue[i];
    queue.length = 0;
    return n;
});
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
#if (defined(__unix__) || defined(__APPLE__)) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
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
#elif defined(__EMSCRIPTEN__)
    installed_ = outland_pointer_install() != 0;
#endif
}
PointerEvents::~PointerEvents() {
    if (!installed_)
        return;
#ifndef __EMSCRIPTEN__
    // Preserve another callback installed after ours, should another subsystem do so.
    const auto current = set_callback(window, previous);
    if (current != capture)
        set_callback(window, current);
    window = nullptr;
    previous = nullptr;
#endif
    pending_count = frame_count = 0;
}
void PointerEvents::begin_frame() {
#ifdef __EMSCRIPTEN__
    if (installed_) {
        std::array<float, limit * 2> raw{};
        const int n = outland_pointer_drain(raw.data(), static_cast<int>(limit));
        const float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
        for (int i = 0; i < n && pending_count < limit; ++i)
            pending[pending_count++] = {raw[2 * i] * w, raw[2 * i + 1] * h};
    }
#endif
    frame_count = pending_count;
    frame = pending;
    pending_count = 0;
}
std::span<const Vector2> PointerEvents::presses() { return {frame.data(), frame_count}; }
} // namespace outland::input
