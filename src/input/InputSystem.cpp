#include <chrono>
#include "outland/input/InputSystem.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>

namespace outland::input {
namespace {
// Wall clock for the aim button's tap/hold timing (no window clock needed).
double seconds_now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

namespace {

Vector2 element_position(
    const TouchElementLayout& element,
    int width,
    int height
) {
    return {
        element.x * static_cast<float>(width),
        element.y * static_cast<float>(height)
    };
}

float base_scale(
    int width,
    int height
) {
    return touch_scale(width, height);
}

bool inside_circle(
    Vector2 point,
    Vector2 center,
    float radius
) {
    return Vector2Distance(
        point,
        center
    ) <= radius;
}

Vector2 analog_value(
    Vector2 point,
    Vector2 center,
    float radius
) {
    constexpr float deadzone = 0.10F;

    Vector2 offset =
        Vector2Subtract(
            point,
            center
        );

    const float offset_length =
        Vector2Length(offset);

    if (
        offset_length <=
        radius * deadzone
    ) {
        return {
            0.0F,
            0.0F
        };
    }

    if (offset_length <= 0.0001F) {
        return {
            0.0F,
            0.0F
        };
    }

    float magnitude =
        offset_length /
        radius;

    magnitude =
        std::clamp(
            magnitude,
            0.0F,
            1.0F
        );

    magnitude =
        (
            magnitude -
            deadzone
        ) /
        (
            1.0F -
            deadzone
        );

    const Vector2 direction =
        Vector2Scale(
            offset,
            1.0F / offset_length
        );

    return Vector2Scale(
        direction,
        magnitude
    );
}

}

InputSystem::InputSystem() = default;

Rectangle InputSystem::pan_area(int width, int height) {
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    return {w * .56F, h * .16F, w * .44F, h * .48F};
}

bool InputSystem::navigation_owns_point(Vector2 point,int width,int height) const {
    const float scale=base_scale(width,height);
    return inside_circle(point,element_position(layout_.movement,width,height),92*scale*layout_.movement.scale) ||
        inside_circle(point,element_position(layout_.look,width,height),92*scale*layout_.look.scale);
}

void InputSystem::update(int screen_width, int screen_height, bool gameplay, bool blocked,
    const std::function<bool(Vector2)>& reserved) {
    player_ = {};
    if (more_ && seconds_now() > more_until_) more_ = false;
    if (screen_width <= 0 || screen_height <= 0) { cancel_controls(); return; }
    const bool changed = width_ != 0 && (gameplay != gameplay_ || blocked != blocked_ ||
        screen_width != width_ || screen_height != height_ || driving_ != was_driving_);
    was_driving_ = driving_;
    gameplay_ = gameplay;
    blocked_ = blocked;
    width_ = screen_width;
    height_ = screen_height;
    if (changed) {
        // Existing contacts stay quarantined until release across mode/size changes.
        for (auto& owner : owners_) owner.control = Control::None;
        mouse_control_ = Control::None;
    }
    const float scale = base_scale(screen_width, screen_height) * button_size_;
    const float w = static_cast<float>(screen_width), h = static_cast<float>(screen_height);
    const auto center = [&](const TouchElementLayout& e) {
        return element_position(e, screen_width, screen_height);
    };
    const auto hit = [&](Vector2 p, const TouchElementLayout& e, float radius) {
        return inside_circle(p, center(e), radius * scale * e.scale);
    };
    // On foot (and building), the right of the screen is one big look surface and the lower left
    // a floating movement stick, as in PUBG Mobile and Call of Duty Mobile.
    const bool on_foot = !look_stick_ && !driving_;
    const auto capture = [&](Vector2 p) {
        if (blocked || p.y < std::min(80.0F, h * 0.12F) || (reserved && reserved(p))) return Control::None;
        if (gameplay && driving_) {
            if (hit(p, layout_.gas, 50)) return Control::Gas;
            if (hit(p, layout_.brake_pedal, 50)) return Control::BrakePedal;
            if (hit(p, layout_.handbrake, 43)) return Control::Handbrake;
            if (hit(p, layout_.interact, 43)) return Control::Interact;
            if (hit(p, layout_.sprint, 43)) return Control::Sprint;
            if (drive_by_ && hit(p, layout_.fire_left, 43)) return Control::FireLeft;
            if (police_car_ && hit(p, layout_.lights, 43)) return Control::Lights;
            if (hit(p, layout_.radio, 40)) return Control::Radio;
            if (hit(p, layout_.movement, 92)) return Control::Movement;
            if (const auto pan = pan_area(screen_width, screen_height); p.x >= pan.x && p.x <= pan.x + pan.width && p.y >= pan.y && p.y <= pan.y + pan.height) return Control::Pan;
            return Control::None;
        }
        if (gameplay) {
            if (hit(p, layout_.jump, 43)) return Control::Jump;
            if (hit(p, layout_.view, 43)) return more_ ? Control::View : Control::More;
            if (hit(p, layout_.fire, 43)) return Control::Fire;
            if (hit(p, layout_.fire_left, 43)) return Control::FireLeft;
            if (more_ && hit(p, layout_.fire_mode, 43)) return Control::FireMode;
            if (hit(p, layout_.aim, 43)) return Control::Aim;
            if (hit(p, layout_.reload, 43)) return Control::Reload;
            if (grenade_ && hit(p, layout_.grenade, 43)) return Control::Grenade;
            if (heal_ && hit(p, layout_.heal, 43)) return Control::Heal;
            if (tagger_ && hit(p, layout_.tag, 43)) return Control::Tag;
            if (officer_ && !tagger_ && hit(p, layout_.tag, 43)) return Control::Backup;
            if (more_ && hit(p, layout_.sight, 43)) return Control::Sight;
            if (hit(p, layout_.weapon, 43)) return Control::Weapon;
            if (hit(p, layout_.sprint, 43)) return Control::Sprint;
            if (hit(p, layout_.crouch, 43)) return Control::Crouch;
            if (hit(p, layout_.inventory, 43)) return Control::Inventory;
            if (usable_ && hit(p, layout_.interact, 43)) return Control::Interact;
        }
        if (hit(p, layout_.movement, 92)) return Control::Movement;
        if (on_foot && p.x < w * .40F && p.y > h * .40F) return Control::Movement; // floating
        if (on_foot && p.x > w * .42F) return Control::Swipe;
        if (hit(p, layout_.look, 92)) return Control::Look;
        // Unclaimed pointers never become gameplay triggers.
        return Control::None;
    };
    const float swipe = PI / h * look_.touch; // half the screen's height: 90 degrees at 1.0
    const auto drag = [&](Vector2 p, Vector2& last) {
        player_.look_dx += (p.x - last.x) * swipe;
        player_.look_dy += (p.y - last.y) * swipe;
        last = p;
    };
    const auto apply = [&](Control control, Vector2 p, bool fresh, Vector2& origin, Vector2& last) {
        if (fresh) { origin = hit(p, layout_.movement, 92) || driving_ ? center(layout_.movement) : p; last = p; }
        switch (control) {
        case Control::Movement: {
            const auto v = analog_value(p, origin, 92 * scale * layout_.movement.scale);
            player_.move_x = v.x;
            if (!driving_) player_.move_y = v.y; // at the wheel the stick only steers
            stick_center_ = origin;
            break;
        }
        case Control::Gas: player_.gas = 1; break;
        case Control::BrakePedal: player_.brake_pedal = 1; break;
        case Control::Handbrake: player_.brake = true; break;
        // At the wheel the space above the pedals is swiped to look round the car, as in PUBG.
        case Control::Pan: drag(p, last); break;
        case Control::Look: {
            const auto v = analog_value(p, center(layout_.look), 92 * scale * layout_.look.scale);
            player_.look_x = v.x; player_.look_y = v.y; break;
        }
        case Control::Swipe: drag(p, last); break;
        case Control::Jump: player_.jump |= fresh; player_.brake = true; break;
        case Control::More: if (fresh) {more_ = true; more_until_ = seconds_now() + 4;} break;
        case Control::View: player_.toggle_view |= fresh; if (fresh) more_ = false; break;
        case Control::Reload: player_.reload |= fresh; break;
        case Control::Grenade: player_.grenade |= fresh; break;
        case Control::Heal: player_.heal |= fresh; break;
        case Control::Lights: player_.lights |= fresh; break;
        case Control::Radio: player_.radio |= fresh; break;
        case Control::Tag: player_.tag |= fresh; break;
        case Control::Backup: player_.backup |= fresh; break;
        case Control::Sight: player_.sight |= fresh; if (fresh) more_until_ = seconds_now() + 4; break;
        case Control::Weapon: player_.next_weapon |= fresh; break;
        case Control::Inventory: player_.inventory |= fresh; break;
        case Control::Interact: player_.interact |= fresh; break;
        case Control::Sprint: player_.sprint = true; break;
        case Control::Crouch: player_.crouch = true; break;
        // Fire and keep aiming: the thumb on FIRE steers the sights as it slides.
        case Control::Fire: case Control::FireLeft: player_.fire = true; drag(p, last); break;
        case Control::FireMode: player_.fire_mode |= fresh; if (fresh) more_until_ = seconds_now() + 4; break;
        case Control::Aim:
            if (fresh) aim_pressed_at_ = seconds_now();
            aim_held_ = true;
            break;
        case Control::None: break;
        }
    };
    const bool aim_was_held = aim_held_;
    aim_held_ = false;
    stick_center_ = {-1, -1};
    const int count = GetTouchPointCount();
    std::erase_if(owners_, [&](const TouchOwner& owner) {
        for (int i = 0; i < count; ++i) if (GetTouchPointId(i) == owner.id) return false;
        return true;
    });
    for (int i = 0; i < count; ++i) {
        const int id = GetTouchPointId(i);
        const auto p = GetTouchPosition(i);
        auto owner = std::find_if(owners_.begin(), owners_.end(),
            [&](const TouchOwner& o) { return o.id == id; });
        const bool fresh = owner == owners_.end();
        if (fresh) {
            auto control = changed ? Control::None : capture(p);
            // Only one finger owns each stick. An unclaimed finger never steals it later.
            if (control == Control::Movement || control == Control::Look || control == Control::Pan || control == Control::Swipe) {
                for (const auto& o : owners_) if (o.control == control) control = Control::None;
            }
            owners_.push_back({id, control});
            owner = owners_.end() - 1;
        }
        apply(owner->control, p, fresh, owner->origin, owner->last);
    }
    // A captured mouse works the bindings, once any button held from before is let go.
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && count == 0 && mouse_control_ == Control::None)
        mouse_quarantined_ = false;
    const bool mouse_buttons = mouse_look_ && !blocked && !mouse_quarantined_;
    // Android can synthesize a final mouse press after the last native contact disappears.
    if (count == 0) {
        const auto p=GetMousePosition();const bool down=IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const bool fresh=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if(had_native_touch_ && down)mouse_quarantined_=true;
        if(fresh && !changed && !had_native_touch_ && !mouse_quarantined_ && touch_controls_ && !mouse_look_)mouse_control_=capture(p);
        if(down && !mouse_quarantined_ && mouse_control_!=Control::None)apply(mouse_control_,p,fresh,mouse_origin_,mouse_last_);
        else mouse_control_=Control::None;
        if(!down)mouse_quarantined_=false;
        had_native_touch_=false;
    } else {mouse_control_=Control::None;had_native_touch_=true;}
    // Aim button released: a quick tap toggles sights, a long hold just ends.
    if (aim_was_held && !aim_held_ && seconds_now() - aim_pressed_at_ < .3) aim_toggled_ = !aim_toggled_;
    // Keyboard and mouse AIM: hold, or a press toggles when the player chose so.
    const bool key_aim = gameplay && !blocked && bindings_.down(Action::Aim, mouse_buttons);
    if (gameplay && !blocked && look_.aim_toggle && bindings_.pressed(Action::Aim, mouse_buttons)) aim_toggled_ = !aim_toggled_;
    if (!gameplay || blocked) aim_toggled_ = false;
    player_.aim = aim_toggled_ || (aim_held_ && seconds_now() - aim_pressed_at_ >= .3) || (key_aim && !look_.aim_toggle);
    if (blocked) { player_.look_dx = player_.look_dy = 0; return; }
    if (mouse_look_) {
        player_.look_dx += mouse_motion_.x * .0012F * look_.mouse;
        player_.look_dy += mouse_motion_.y * .0012F * look_.mouse;
    }
    if (look_.invert_y) { player_.look_dy = -player_.look_dy; player_.look_y = -player_.look_y; }
    const auto& b = bindings_;
    if (b.down(Action::Forward, mouse_buttons)) player_.move_y -= 1;
    if (b.down(Action::Back, mouse_buttons)) player_.move_y += 1;
    if (b.down(Action::Left, mouse_buttons)) player_.move_x -= 1;
    if (b.down(Action::Right, mouse_buttons)) player_.move_x += 1;
    player_.jump |= b.pressed(Action::Jump, mouse_buttons);
    player_.brake |= b.down(Action::Jump, mouse_buttons);
    player_.sprint |= b.down(Action::Sprint, mouse_buttons);
    player_.toggle_view |= b.pressed(Action::View, mouse_buttons);
    player_.lights |= b.pressed(Action::Lights, mouse_buttons);
    player_.radio |= b.pressed(Action::Radio, mouse_buttons);
    if (gameplay) {
        player_.crouch |= b.down(Action::Crouch, mouse_buttons);
        player_.fire |= b.down(Action::Fire, mouse_buttons);
        player_.reload |= b.pressed(Action::Reload, mouse_buttons);
        player_.grenade |= b.pressed(Action::Grenade, mouse_buttons);
        player_.heal |= b.pressed(Action::Heal, mouse_buttons);
        player_.tag |= b.pressed(Action::Tag, mouse_buttons);
        player_.backup |= b.pressed(Action::Backup, mouse_buttons);
        player_.sight |= b.pressed(Action::Sight, mouse_buttons);
        player_.next_weapon |= b.pressed(Action::NextWeapon, mouse_buttons);
        player_.fire_mode |= b.pressed(Action::FireMode, mouse_buttons);
        player_.inventory |= b.pressed(Action::Inventory, mouse_buttons);
        player_.interact |= b.pressed(Action::Interact, mouse_buttons);
    }
    Vector2 movement{player_.move_x, player_.move_y};
    if (Vector2Length(movement) > 1) {
        movement = Vector2Normalize(movement);
        player_.move_x = movement.x; player_.move_y = movement.y;
    }
    if (player_.crouch) player_.sprint = false;
    // Sprinting lowers the sights, as in any battle royale.
    if (player_.sprint && Vector2Length(movement) > .1F) {aim_toggled_ = false; player_.aim = false;}
}

void InputSystem::set_mouse_look(bool captured, Vector2 motion) {
    if (captured && !mouse_look_ && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)))
        mouse_quarantined_ = true;
    mouse_look_ = captured;
    mouse_motion_ = motion;
}

TouchLayout InputSystem::drawn_layout(int width, int height) const {
    TouchLayout out = layout_;
    for (auto* e : {&out.movement, &out.look, &out.fire, &out.fire_left, &out.fire_mode, &out.aim, &out.jump, &out.reload,
                    &out.weapon, &out.view, &out.sprint, &out.grenade, &out.heal, &out.sight, &out.gas, &out.brake_pedal,
                    &out.handbrake, &out.lights, &out.radio, &out.tag, &out.crouch, &out.inventory, &out.interact})
        e->scale *= button_size_;
    if (stick_center_.x >= 0 && width > 0 && height > 0) {
        out.movement.x = stick_center_.x / static_cast<float>(width);
        out.movement.y = stick_center_.y / static_cast<float>(height);
    }
    return out;
}

const PlayerInput&
InputSystem::player() const {
    return player_;
}

PlayerInput&
InputSystem::player() {
    return player_;
}

const TouchLayout&
InputSystem::layout() const {
    return layout_;
}

TouchLayout&
InputSystem::layout() {
    return layout_;
}

void InputSystem::cancel_controls() {
    player_ = {};
    for (auto& owner : owners_) owner.control = Control::None;
    // DEV/UI may cancel before this frame's fresh contacts have been captured.
    for(int i=0;i<GetTouchPointCount();++i){const int id=GetTouchPointId(i);if(std::none_of(owners_.begin(),owners_.end(),[&](const auto& owner){return owner.id==id;}))owners_.push_back({id,Control::None});}
    mouse_control_ = Control::None;
    mouse_quarantined_|=IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
}

void InputSystem::reset_layout() {
    cancel_controls();
    layout_.reset();
}

}
