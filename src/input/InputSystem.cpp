#include "outland/input/InputSystem.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>

namespace outland::input {

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

bool InputSystem::navigation_owns_point(Vector2 point,int width,int height) const {
    const float scale=base_scale(width,height);
    return inside_circle(point,element_position(layout_.movement,width,height),92*scale*layout_.movement.scale) ||
        inside_circle(point,element_position(layout_.look,width,height),92*scale*layout_.look.scale);
}

void InputSystem::update(int screen_width, int screen_height, bool gameplay, bool blocked,
    const std::function<bool(Vector2)>& reserved) {
    player_ = {};
    if (screen_width <= 0 || screen_height <= 0) { cancel_controls(); return; }
    const bool changed = width_ != 0 && (gameplay != gameplay_ || blocked != blocked_ ||
        screen_width != width_ || screen_height != height_);
    gameplay_ = gameplay;
    blocked_ = blocked;
    width_ = screen_width;
    height_ = screen_height;
    if (changed) {
        // Existing contacts stay quarantined until release across mode/size changes.
        for (auto& owner : owners_) owner.control = Control::None;
        mouse_control_ = Control::None;
    }
    const float scale = base_scale(screen_width, screen_height);
    const auto center = [&](const TouchElementLayout& e) {
        return element_position(e, screen_width, screen_height);
    };
    const auto hit = [&](Vector2 p, const TouchElementLayout& e, float radius) {
        return inside_circle(p, center(e), radius * scale * e.scale);
    };
    const auto capture = [&](Vector2 p) {
        if (blocked || p.y < std::min(80.0F, screen_height * 0.12F) || (reserved && reserved(p))) return Control::None;
        if (gameplay) {
            if (hit(p, layout_.jump, 43)) return Control::Jump;
            if (hit(p, layout_.view, 43)) return Control::View;
            if (hit(p, layout_.fire, 43)) return Control::Fire;
            if (hit(p, layout_.aim, 43)) return Control::Aim;
            if (hit(p, layout_.reload, 43)) return Control::Reload;
            if (hit(p, layout_.weapon, 43)) return Control::Weapon;
            if (hit(p, layout_.sprint, 43)) return Control::Sprint;
            if (hit(p, layout_.crouch, 43)) return Control::Crouch;
            if (hit(p, layout_.inventory, 43)) return Control::Inventory;
            if (hit(p, layout_.interact, 43)) return Control::Interact;
        }
        if (hit(p, layout_.movement, 92)) return Control::Movement;
        if (hit(p, layout_.look, 92)) return Control::Look;
        // Unclaimed pointers never become gameplay triggers.
        return Control::None;
    };
    const auto apply = [&](Control control, Vector2 p, bool fresh) {
        switch (control) {
        case Control::Movement: {
            const auto v = analog_value(p, center(layout_.movement), 92 * scale * layout_.movement.scale);
            player_.move_x = v.x; player_.move_y = v.y; break;
        }
        case Control::Look: {
            const auto v = analog_value(p, center(layout_.look), 92 * scale * layout_.look.scale);
            player_.look_x = v.x; player_.look_y = v.y; break;
        }
        case Control::Jump: player_.jump |= fresh; player_.brake = true; break;
        case Control::View: player_.toggle_view |= fresh; break;
        case Control::Reload: player_.reload |= fresh; break;
        case Control::Weapon: player_.next_weapon |= fresh; break;
        case Control::Inventory: player_.inventory |= fresh; break;
        case Control::Interact: player_.interact |= fresh; break;
        case Control::Sprint: player_.sprint = true; break;
        case Control::Crouch: player_.crouch = true; break;
        case Control::Fire: player_.fire = true; break;
        case Control::Aim: player_.aim = true; break;
        case Control::None: break;
        }
    };
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
            if (control == Control::Movement || control == Control::Look) {
                for (const auto& o : owners_) if (o.control == control) control = Control::None;
            }
            owners_.push_back({id, control});
            owner = owners_.end() - 1;
        }
        apply(owner->control, p, fresh);
    }
    // Android can synthesize a final mouse press after the last native contact disappears.
    if (count == 0) {
        const auto p=GetMousePosition();const bool down=IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const bool fresh=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if(had_native_touch_ && down)mouse_quarantined_=true;
        if(fresh && !changed && !had_native_touch_ && !mouse_quarantined_)mouse_control_=capture(p);
        if(down && !mouse_quarantined_)apply(mouse_control_,p,fresh);
        else mouse_control_=Control::None;
        if(!down)mouse_quarantined_=false;
        had_native_touch_=false;
    } else {mouse_control_=Control::None;had_native_touch_=true;}
    if (blocked) return;
    if (IsKeyDown(KEY_W)) player_.move_y -= 1;
    if (IsKeyDown(KEY_S)) player_.move_y += 1;
    if (IsKeyDown(KEY_A)) player_.move_x -= 1;
    if (IsKeyDown(KEY_D)) player_.move_x += 1;
    player_.jump |= IsKeyPressed(KEY_SPACE);
    player_.brake |= IsKeyDown(KEY_SPACE);
    player_.sprint |= IsKeyDown(KEY_LEFT_SHIFT);
    player_.toggle_view |= IsKeyPressed(KEY_V);
    if (gameplay) {
        player_.crouch |= IsKeyDown(KEY_C);
        player_.fire |= IsKeyDown(KEY_F);
        player_.aim |= IsKeyDown(KEY_Q) || (count == 0 && IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
        player_.reload |= IsKeyPressed(KEY_R);
        player_.next_weapon |= IsKeyPressed(KEY_TAB);
        player_.inventory |= IsKeyPressed(KEY_I);
        player_.interact |= IsKeyPressed(KEY_E);
    }
    Vector2 movement{player_.move_x, player_.move_y};
    if (Vector2Length(movement) > 1) {
        movement = Vector2Normalize(movement);
        player_.move_x = movement.x; player_.move_y = movement.y;
    }
    if (player_.crouch) player_.sprint = false;
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
    mouse_quarantined_|=IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

void InputSystem::reset_layout() {
    cancel_controls();
    layout_.reset();
}

}
