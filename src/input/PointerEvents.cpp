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
    // Mouse look: the pointer is locked to the canvas while playing; motion adds up between frames
    // (raylib's own web backend keeps only the last event of a frame).
    // Browsers can report a jump as the lock starts (to or from the cursor's old place): moves
    // in the first 80 ms of a lock and single moves beyond 400 px are dropped.
    Module['outlandMotion'] = [0, 0];
    var locked_at = 0;
    document.addEventListener('pointerlockchange', function() { locked_at = performance.now(); }, true);
    document.addEventListener('mousemove', function(e) {
        if (document.pointerLockElement !== canvas) return;
        var dx = e.movementX || 0, dy = e.movementY || 0;
        if (performance.now() - locked_at < 80 || Math.abs(dx) > 400 || Math.abs(dy) > 400) return;
        Module['outlandMotion'][0] += dx;
        Module['outlandMotion'][1] += dy;
    }, true);
    // A click while the game wants the pointer locks it (browsers allow it only from a gesture).
    canvas.addEventListener('mousedown', function() {
        if (Module['outlandWantLock'] && document.pointerLockElement !== canvas) Module['outlandLock']();
    }, true);
    Module['outlandLock'] = function() {
        try {
            var r = canvas.requestPointerLock({unadjustedMovement: true});
            if (r && r.catch) r.catch(function() {
                try { var q = canvas.requestPointerLock(); if (q && q.catch) q.catch(function() {}); } catch (e) {}
            });
        } catch (e) {}
    };
    return 1;
});
EM_JS(void, outland_pointer_want_lock, (int want), {
    var canvas = Module['canvas'];
    var had = !!Module['outlandWantLock'];
    Module['outlandWantLock'] = !!want;
    if (!canvas) return;
    if (want && !had && document.pointerLockElement !== canvas && Module['outlandLock']) Module['outlandLock']();
    if (!want && document.pointerLockElement === canvas) document.exitPointerLock();
});
EM_JS(int, outland_touch_screen, (), {
    var coarse = window.matchMedia && window.matchMedia('(pointer: coarse)').matches;
    var fine = window.matchMedia && window.matchMedia('(any-pointer: fine)').matches;
    return coarse || (navigator.maxTouchPoints > 0 && !fine) ? 1 : 0;
});
EM_JS(int, outland_pointer_locked, (), {
    return Module['canvas'] && document.pointerLockElement === Module['canvas'] ? 1 : 0;
});
EM_JS(void, outland_pointer_motion, (float* out), {
    var m = Module['outlandMotion'] || [0, 0];
    HEAPF32[out >> 2] = m[0];
    HEAPF32[(out >> 2) + 1] = m[1];
    m[0] = 0; m[1] = 0;
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
Vector2 motion{};
bool want_lock = false, cursor_disabled = false;
int settle = 0; // frames to ignore after the desktop cursor is captured (it jumps to the centre)
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
#ifdef __EMSCRIPTEN__
    float m[2]{};
    outland_pointer_motion(m);
    motion = {m[0], m[1]};
#elif !defined(__ANDROID__)
    motion = cursor_disabled && settle == 0 ? GetMouseDelta() : Vector2{};
    if (settle > 0) --settle;
#endif
}
void PointerEvents::capture_mouse(bool want) {
#ifdef __EMSCRIPTEN__
    if (want != want_lock) outland_pointer_want_lock(want ? 1 : 0);
#elif !defined(__ANDROID__)
    if (want && !cursor_disabled && IsWindowFocused()) { DisableCursor(); cursor_disabled = true; settle = 2; }
    if (!want && cursor_disabled) { EnableCursor(); cursor_disabled = false; }
#endif
    want_lock = want;
}
bool PointerEvents::mouse_captured() {
#ifdef __EMSCRIPTEN__
    return outland_pointer_locked() != 0;
#elif defined(__ANDROID__)
    return false;
#else
    return cursor_disabled && IsWindowFocused();
#endif
}
Vector2 PointerEvents::mouse_motion() { return motion; }
bool PointerEvents::touch_screen() {
#ifdef __EMSCRIPTEN__
    return outland_touch_screen() != 0;
#elif defined(__ANDROID__)
    return true;
#else
    return false;
#endif
}
std::span<const Vector2> PointerEvents::presses() { return {frame.data(), frame_count}; }
} // namespace outland::input
