#pragma once

#include "outland/input/Bindings.hpp"
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

    // How the player looks and which keys do what (from game::GameSettings).
    struct Look {
        float touch{1.0F};      // swipe: half the screen's height turns 90 degrees at 1.0
        float mouse{1.0F};      // .0012 radians per pixel at 1.0
        bool invert_y{false};
        bool aim_toggle{false}; // mouse/keys: AIM toggles the sights instead of holding
    };
    void set_look(const Look& look) { look_ = look; }
    void set_bindings(const Bindings& bindings) { bindings_ = bindings; }
    [[nodiscard]] const Bindings& bindings() const { return bindings_; }
    // On-screen touch controls: when on, a mouse click works them like a finger (as before);
    // when off the game plays like a PC shooter. Touches always reach the touch controls.
    void set_touch_controls(bool on) { touch_controls_ = on; }
    [[nodiscard]] bool touch_controls() const { return touch_controls_; }
    // The pointer is captured (locked) for mouse look, and how far it moved this frame (pixels).
    // A button still held from a menu click (or the click that captured the pointer) does not
    // fire or aim until it is let go.
    void set_mouse_look(bool captured, Vector2 motion);
    [[nodiscard]] bool mouse_look() const { return mouse_look_; }
    // The Creator keeps its look stick; everywhere else the right of the screen is swiped.
    void set_look_stick(bool on) { look_stick_ = on; }
    // Touch button size (1 = the layout's own).
    void set_button_size(float size) { button_size_ = size; }
    // The layout to draw: button sizes applied and the movement stick where the thumb put it.
    [[nodiscard]] TouchLayout drawn_layout(int width, int height) const;

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
        None, Movement, Look, Swipe, Jump, View, Fire, FireLeft, FireMode, Aim, Reload, Weapon, Grenade, Heal, Sight,
        Sprint, Crouch, Inventory, Interact, Gas, BrakePedal, Handbrake, Pan
    };
    // A finger and what it works; origin is where a floating stick was put down, last where a
    // swiping finger was a frame ago.
    struct TouchOwner { int id; Control control; Vector2 origin{}, last{}; };
    std::vector<TouchOwner> owners_;
    Control mouse_control_{Control::None};
    Vector2 mouse_origin_{}, mouse_last_{};
    Look look_{};
    Bindings bindings_{Bindings::defaults()};
    bool touch_controls_{true}, mouse_look_{false}, look_stick_{false};
    Vector2 mouse_motion_{};
    float button_size_{1.0F};
    Vector2 stick_center_{-1, -1}; // a floating movement stick's centre while held
    bool mouse_quarantined_{false},had_native_touch_{false};
    bool gameplay_{true}, driving_{false}, was_driving_{false};
    // Aim button: a tap toggles aiming down sights, a hold aims while held.
    bool aim_toggled_{false}, aim_held_{false};
    double aim_pressed_at_{0};
    bool blocked_{false};
    int width_{0}, height_{0};
};

}
