#pragma once

#include "outland/input/PlayerInput.hpp"
#include "outland/input/TouchLayout.hpp"
#include <vector>
#include <functional>
#include <raylib.h>

namespace outland::input {

class InputSystem {
public:
    InputSystem();

    // Gameplay controls are omitted in Creator mode; unreserved sticks still navigate.
    // blocked cancels all input. reserved is checked only when a contact begins.
    void update(
        int screen_width,
        int screen_height,
        bool gameplay = true,
        bool blocked = false,
        const std::function<bool(Vector2)>& reserved = {}
    );

    [[nodiscard]]
    const PlayerInput& player() const;

    [[nodiscard]]
    PlayerInput& player();

    [[nodiscard]]
    const TouchLayout& layout() const;

    [[nodiscard]]
    TouchLayout& layout();

    void reset_layout();
    [[nodiscard]] bool navigation_owns_point(Vector2 point,int width,int height) const;

    // Keep active contacts quarantined until their release.
    void cancel_controls();

private:
    PlayerInput player_{};
    TouchLayout layout_{};

    enum class Control {
        None, Movement, Look, Jump, View, Fire, Aim, Reload, Weapon,
        Sprint, Crouch, Inventory, Interact
    };
    struct TouchOwner { int id; Control control; };
    std::vector<TouchOwner> owners_;
    Control mouse_control_{Control::None};
    bool mouse_quarantined_{false},had_native_touch_{false};
    bool gameplay_{true};
    bool blocked_{false};
    int width_{0}, height_{0};
};

}
