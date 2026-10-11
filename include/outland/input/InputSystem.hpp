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
    // A touch screen has been used: aim assist helps thumbs as it does on phones in PUBG/Fortnite.
    [[nodiscard]] bool touch_device() const { return had_native_touch_; }

    // Keep active contacts quarantined until their release.
    void cancel_controls();
    // At the wheel: steer on the left stick, GAS and BRAKE pedals, a pan area above them.
    void set_driving(bool driving) { driving_ = driving; }
    [[nodiscard]] bool driving() const { return driving_; }
    // The camera-pan area while driving (screen pixels).
    [[nodiscard]] static Rectangle pan_area(int width, int height);

private:
    PlayerInput player_{};
    TouchLayout layout_{};

    enum class Control {
        None, Movement, Look, Jump, View, Fire, FireLeft, FireMode, Aim, Reload, Weapon, Grenade, Sight,
        Sprint, Crouch, Inventory, Interact, Gas, BrakePedal, Handbrake, Pan
    };
    struct TouchOwner { int id; Control control; };
    std::vector<TouchOwner> owners_;
    Control mouse_control_{Control::None};
    bool mouse_quarantined_{false},had_native_touch_{false};
    bool gameplay_{true}, driving_{false}, was_driving_{false};
    Vector2 pan_origin_{};
    // Aim button: a tap toggles aiming down sights, a hold aims while held.
    bool aim_toggled_{false}, aim_held_{false};
    double aim_pressed_at_{0};
    bool blocked_{false};
    int width_{0}, height_{0};
};

}
