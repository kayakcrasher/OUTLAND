#pragma once
#include <array>
#include <cstdint>
#include <string>

// Keyboard and mouse bindings, as in other PC shooters: WASD to move, the mouse to look, left
// button to fire, right button to aim. Every action has two slots and both can be rebound from
// the pause menu's CONTROLS page. A code is a raylib key (KEY_*), a mouse button (mouse(b)) or
// the wheel (wheel); 0 is unbound.
namespace outland::input {

enum class Action : std::uint8_t {
    Forward, Back, Left, Right, Jump, Sprint, Crouch,
    Fire, Aim, Reload, Interact, Grenade, NextWeapon, FireMode, Sight,
    Inventory, View, Map, Build, Heal, Lights, Tag, Backup, Radio, Hitch,
    Count
};
constexpr std::size_t action_count = static_cast<std::size_t>(Action::Count);

struct Bindings {
    static constexpr int wheel = -20;
    static constexpr int mouse(int button) { return -1 - button; } // left -1, right -2, middle -3, side -4/-5
    static constexpr bool is_mouse(int code) { return code <= -1 && code >= -7; }

    std::array<std::array<int, 2>, action_count> codes{};

    static Bindings defaults();
    [[nodiscard]] const std::array<int, 2>& operator[](Action a) const { return codes[static_cast<std::size_t>(a)]; }
    [[nodiscard]] std::array<int, 2>& operator[](Action a) { return codes[static_cast<std::size_t>(a)]; }

    // Held / pressed this frame (raylib input; the mouse only counts when `mouse` is true).
    [[nodiscard]] bool down(Action a, bool mouse = true) const;
    [[nodiscard]] bool pressed(Action a, bool mouse = true) const;
    // Binds `code` to slot `slot` of `a`; whatever else used it loses it (no double bindings).
    void bind(Action a, int slot, int code);
    // The key, mouse button or wheel turn made this frame, or 0. Escape is never returned.
    static int captured();

    static const char* label(Action a);   // "FIRE", "MOVE FORWARD", ...
    static const char* key(Action a);     // the settings file's name: "fire", "forward", ...
    static std::string name(int code);    // "W", "LEFT MOUSE", "SPACE", "-" for 0
};

} // namespace outland::input
