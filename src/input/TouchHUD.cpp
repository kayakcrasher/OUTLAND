#include "outland/input/TouchHUD.hpp"

#include <raylib.h>

#include <algorithm>

namespace outland::input {

namespace {

Vector2 position_of(
    const TouchElementLayout& element,
    int width,
    int height
) {
    return {
        element.x *
            static_cast<float>(width),

        element.y *
            static_cast<float>(height)
    };
}

float hud_scale(
    int width,
    int height
) {
    return std::clamp(
        std::min(
            static_cast<float>(width) /
                1280.0F,

            static_cast<float>(height) /
                720.0F
        ),
        0.65F,
        1.35F
    );
}

void draw_stick(
    Vector2 center,
    Vector2 value,
    float radius,
    float opacity
) {
    DrawCircleV(
        center,
        radius,
        Fade(BLACK, opacity)
    );

    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        radius,
        Fade(WHITE, 0.75F)
    );

    const Vector2 knob{
        center.x +
            value.x * radius,

        center.y +
            value.y * radius
    };

    DrawCircleV(
        knob,
        radius * 0.42F,
        Fade(WHITE, 0.60F)
    );
}

void draw_button(
    Vector2 center,
    float radius,
    float opacity,
    const char* text,
    bool active
) {
    const float fill_opacity =
        active
            ? std::min(
                opacity + 0.30F,
                0.90F
            )
            : opacity;

    DrawCircleV(
        center,
        radius,
        Fade(
            BLACK,
            fill_opacity
        )
    );

    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        radius,
        WHITE
    );

    if (
        text == nullptr ||
        text[0] == '\0'
    ) {
        return;
    }

    const int font_size =
        static_cast<int>(
            std::max(
                12.0F,
                radius * 0.36F
            )
        );

    const int text_width =
        MeasureText(
            text,
            font_size
        );

    DrawText(
        text,
        static_cast<int>(
            center.x -
            text_width / 2.0F
        ),
        static_cast<int>(
            center.y -
            font_size / 2.0F
        ),
        font_size,
        WHITE
    );
}

}

void TouchHUD::draw(
    const PlayerInput& input,
    const TouchLayout& layout,
    int screen_width,
    int screen_height
) const {
    const float scale =
        hud_scale(
            screen_width,
            screen_height
        );

    const float base_stick_radius =
        92.0F * scale;

    const float base_button_radius =
        43.0F * scale;

    draw_button(position_of(layout.reload, screen_width, screen_height),
                base_button_radius * layout.reload.scale, layout.reload.opacity, "LOAD", input.reload);
    draw_button(position_of(layout.weapon, screen_width, screen_height),
                base_button_radius * layout.weapon.scale, layout.weapon.opacity, "GUN", input.next_weapon);

    draw_stick(
        position_of(
            layout.movement,
            screen_width,
            screen_height
        ),
        {
            input.move_x,
            input.move_y
        },
        base_stick_radius *
            layout.movement.scale,
        layout.movement.opacity
    );

    // Look stick intentionally has no label.
    draw_stick(
        position_of(
            layout.look,
            screen_width,
            screen_height
        ),
        {
            input.look_x,
            input.look_y
        },
        base_stick_radius *
            layout.look.scale,
        layout.look.opacity
    );

    draw_button(
        position_of(
            layout.jump,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.jump.scale,
        layout.jump.opacity,
        "JUMP",
        input.jump
    );

    draw_button(
        position_of(
            layout.view,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.view.scale,
        layout.view.opacity,
        "VIEW",
        input.toggle_view
    );

    draw_button(
        position_of(
            layout.fire,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.fire.scale,
        layout.fire.opacity,
        "FIRE",
        input.fire
    );

    draw_button(
        position_of(
            layout.aim,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.aim.scale,
        layout.aim.opacity,
        "AIM",
        input.aim
    );
}

}
