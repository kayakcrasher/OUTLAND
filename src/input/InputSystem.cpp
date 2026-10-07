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
    const float width_scale =
        static_cast<float>(width) / 1280.0F;

    const float height_scale =
        static_cast<float>(height) / 720.0F;

    return std::clamp(
        std::min(
            width_scale,
            height_scale
        ),
        0.65F,
        1.35F
    );
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

void InputSystem::update(
    int screen_width,
    int screen_height
) {
    // ========================================================
    // RESET FRAME STATE
    // ========================================================

    player_.move_x = 0.0F;
    player_.move_y = 0.0F;

    player_.look_x = 0.0F;
    player_.look_y = 0.0F;

    player_.jump = false;
    player_.toggle_view = false;

    player_.fire = false;
    player_.aim = false;
    player_.reload = false;
    player_.next_weapon = false;
    player_.interact = false;
    player_.crouch = false;
    player_.sprint = false;

    // ========================================================
    // HUD GEOMETRY
    // ========================================================

    const float scale =
        base_scale(
            screen_width,
            screen_height
        );

    const float movement_radius =
        92.0F *
        scale *
        layout_.movement.scale;

    const float look_radius =
        92.0F *
        scale *
        layout_.look.scale;

    const float base_button_radius =
        43.0F * scale;

    const Vector2 movement_center =
        element_position(
            layout_.movement,
            screen_width,
            screen_height
        );

    const Vector2 look_center =
        element_position(
            layout_.look,
            screen_width,
            screen_height
        );

    const Vector2 jump_center =
        element_position(
            layout_.jump,
            screen_width,
            screen_height
        );

    const Vector2 view_center =
        element_position(
            layout_.view,
            screen_width,
            screen_height
        );

    const Vector2 fire_center =
        element_position(
            layout_.fire,
            screen_width,
            screen_height
        );

    const Vector2 aim_center =
        element_position(
            layout_.aim,
            screen_width,
            screen_height
        );

    const float jump_radius =
        base_button_radius *
        layout_.jump.scale;

    const float view_radius =
        base_button_radius *
        layout_.view.scale;

    const float fire_radius =
        base_button_radius *
        layout_.fire.scale;

    const float aim_radius =
        base_button_radius *
        layout_.aim.scale;

    const Vector2 reload_center=element_position(layout_.reload,screen_width,screen_height);
    const Vector2 weapon_center=element_position(layout_.weapon,screen_width,screen_height);
    const float reload_radius=base_button_radius*layout_.reload.scale;
    const float weapon_radius=base_button_radius*layout_.weapon.scale;

    // ========================================================
    // NATIVE TOUCH INPUT
    // ========================================================

    bool jump_down = false;
    bool view_down = false;
    bool reload_down = false;
    bool weapon_down = false;

    bool movement_alive = false;
    bool look_alive = false;

    const int touch_count =
        GetTouchPointCount();

    for (
        int i = 0;
        i < touch_count;
        ++i
    ) {
        const int id =
            GetTouchPointId(i);

        const Vector2 position =
            GetTouchPosition(i);

        if (
            inside_circle(
                position,
                jump_center,
                jump_radius
            )
        ) {
            jump_down = true;
            continue;
        }

        if (
            inside_circle(
                position,
                view_center,
                view_radius
            )
        ) {
            view_down = true;
            continue;
        }

        if (
            inside_circle(
                position,
                fire_center,
                fire_radius
            )
        ) {
            player_.fire = true;
            continue;
        }

        if (
            inside_circle(
                position,
                aim_center,
                aim_radius
            )
        ) {
            player_.aim = true;
            continue;
        }

        if (inside_circle(position,reload_center,reload_radius)) { reload_down=true;continue; }
        if (inside_circle(position,weapon_center,weapon_radius)) { weapon_down=true;continue; }

        if (
            id == movement_touch_id_ ||
            (
                movement_touch_id_ < 0 &&
                inside_circle(
                    position,
                    movement_center,
                    movement_radius * 1.40F
                )
            )
        ) {
            movement_touch_id_ = id;
            movement_alive = true;

            const Vector2 value =
                analog_value(
                    position,
                    movement_center,
                    movement_radius
                );

            player_.move_x = -value.x;
            player_.move_y = value.y;

            continue;
        }

        if (
            id == look_touch_id_ ||
            (
                look_touch_id_ < 0 &&
                inside_circle(
                    position,
                    look_center,
                    look_radius * 1.40F
                )
            )
        ) {
            look_touch_id_ = id;
            look_alive = true;

            const Vector2 value =
                analog_value(
                    position,
                    look_center,
                    look_radius
                );

            player_.look_x = value.x;
            player_.look_y = value.y;
        }
    }

    if (!movement_alive) {
        movement_touch_id_ = -1;
    }

    if (!look_alive) {
        look_touch_id_ = -1;
    }

    // ========================================================
    // X11 MOUSE-AS-TOUCH
    //
    // Termux:X11 can expose Android finger input as an X11
    // pointer. This lets the development build use the same HUD.
    //
    // Native Android touch remains supported above.
    // ========================================================

    const Vector2 mouse =
        GetMousePosition();

    if (IsMouseButtonPressed(
            MOUSE_BUTTON_LEFT
        )) {

        if (
            inside_circle(
                mouse,
                jump_center,
                jump_radius
            )
        ) {
            mouse_control_ =
                MouseControl::Jump;
        }
        else if (
            inside_circle(
                mouse,
                view_center,
                view_radius
            )
        ) {
            mouse_control_ =
                MouseControl::View;
        }
        else if (
            inside_circle(
                mouse,
                fire_center,
                fire_radius
            )
        ) {
            mouse_control_ =
                MouseControl::Fire;
        }
        else if (
            inside_circle(
                mouse,
                aim_center,
                aim_radius
            )
        ) {
            mouse_control_ =
                MouseControl::Aim;
        }
        else if (inside_circle(mouse,reload_center,reload_radius)) mouse_control_=MouseControl::Reload;
        else if (inside_circle(mouse,weapon_center,weapon_radius)) mouse_control_=MouseControl::Weapon;
        else if (
            inside_circle(
                mouse,
                movement_center,
                movement_radius * 1.40F
            )
        ) {
            mouse_control_ =
                MouseControl::Movement;
        }
        else if (
            inside_circle(
                mouse,
                look_center,
                look_radius * 1.40F
            )
        ) {
            mouse_control_ =
                MouseControl::Look;
        }
        else {
            mouse_control_ =
                MouseControl::None;
        }
    }

    if (IsMouseButtonDown(
            MOUSE_BUTTON_LEFT
        )) {

        switch (mouse_control_) {

        case MouseControl::Movement: {
            const Vector2 value =
                analog_value(
                    mouse,
                    movement_center,
                    movement_radius
                );

            player_.move_x =
                value.x;

            player_.move_y =
                value.y;

            break;
        }

        case MouseControl::Look: {
            const Vector2 value =
                analog_value(
                    mouse,
                    look_center,
                    look_radius
                );

            player_.look_x =
                value.x;

            player_.look_y =
                value.y;

            break;
        }

        case MouseControl::Jump:
            jump_down = true;
            break;

        case MouseControl::View:
            view_down = true;
            break;

        case MouseControl::Fire:
            player_.fire = true;
            break;

        case MouseControl::Aim:
            player_.aim = true;
            break;

        case MouseControl::Reload:
            reload_down=true;
            break;
        case MouseControl::Weapon:
            weapon_down=true;
            break;
        case MouseControl::None:
            // Preserve HUD dragging and keep top-row UI clicks out of gunfire.
            if (mouse.y>80) player_.fire=true;
            break;
        }
    }

    if (IsMouseButtonReleased(
            MOUSE_BUTTON_LEFT
        )) {
        mouse_control_ =
            MouseControl::None;
    }

    // ========================================================
    // EDGE-TRIGGER BUTTONS
    // ========================================================

    player_.jump =
        jump_down &&
        !previous_jump_;

    player_.toggle_view =
        view_down &&
        !previous_view_;

    player_.reload=reload_down && !previous_reload_;
    player_.next_weapon=weapon_down && !previous_weapon_;
    previous_reload_=reload_down;
    previous_weapon_=weapon_down;

    previous_jump_ =
        jump_down;

    previous_view_ =
        view_down;

    // ========================================================
    // KEYBOARD DEVELOPMENT INPUT
    // ========================================================

    if (IsKeyDown(KEY_W)) {
        player_.move_y -= 1.0F;
    }

    if (IsKeyDown(KEY_S)) {
        player_.move_y += 1.0F;
    }

    if (IsKeyDown(KEY_A)) {
        player_.move_x -= 1.0F;
    }

    if (IsKeyDown(KEY_D)) {
        player_.move_x += 1.0F;
    }

    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        player_.sprint = true;
    }

    if (IsKeyPressed(KEY_SPACE)) {
        player_.jump = true;
    }

    if (IsKeyPressed(KEY_V)) {
        player_.toggle_view = true;
    }

    if (IsKeyDown(KEY_F)) {
        player_.fire = true;
    }

    if (IsKeyDown(KEY_Q)) {
        player_.aim = true;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) player_.aim=true;
    if (IsKeyPressed(KEY_TAB)) player_.next_weapon=true;

    if (IsKeyPressed(KEY_R)) {
        player_.reload = true;
    }

    if (IsKeyPressed(KEY_E)) {
        player_.interact = true;
    }

    // ========================================================
    // NORMALIZE MOVEMENT
    // ========================================================

    Vector2 movement{
        player_.move_x,
        player_.move_y
    };

    const float movement_length =
        Vector2Length(movement);

    if (
        movement_length > 0.92F &&
        (movement_alive || mouse_control_ == MouseControl::Movement)
    ) {
        player_.sprint = true;
    }

    if (
        movement_length >
        1.0F
    ) {
        movement =
            Vector2Normalize(
                movement
            );

        player_.move_x =
            movement.x;

        player_.move_y =
            movement.y;
    }
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

void InputSystem::reset_layout() {
    layout_.reset();
}

}
