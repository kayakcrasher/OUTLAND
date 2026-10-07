#include "outland/input/TouchHUD.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace outland::input {

namespace {

Vector2 position_of(
    const TouchElementLayout& element,
    int width,
    int height
) {
    return {
        element.x * static_cast<float>(width),
        element.y * static_cast<float>(height)
    };
}


float hud_scale(
    int width,
    int height
) {
    return std::clamp(
        std::min(
            static_cast<float>(width) / 1280.0F,
            static_cast<float>(height) / 720.0F
        ),
        0.65F,
        1.35F
    );
}


void draw_ring(
    Vector2 center,
    float radius,
    Color color
) {
    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        radius,
        color
    );

    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        radius - 1.0F,
        Fade(color, 0.55F)
    );
}


void draw_stick(
    Vector2 center,
    Vector2 value,
    float radius,
    float opacity,
    bool subtle
) {
    const float background_opacity =
        subtle
            ? opacity * 0.42F
            : opacity;

    const float ring_opacity =
        subtle
            ? 0.16F
            : 0.58F;

    DrawCircleV(
        center,
        radius,
        Fade(BLACK, background_opacity)
    );

    // Outer movement boundary.
    draw_ring(
        center,
        radius,
        Fade(RAYWHITE, ring_opacity)
    );

    // Inner reference ring.
    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        radius * 0.55F,
        Fade(
            RAYWHITE,
            subtle ? 0.08F : 0.18F
        )
    );

    // Cardinal marks make movement direction readable
    // without cluttering the control.
    if (!subtle) {
        const float mark_inner = radius * 0.78F;
        const float mark_outer = radius * 0.91F;

        DrawLineV(
            {center.x, center.y - mark_inner},
            {center.x, center.y - mark_outer},
            Fade(RAYWHITE, 0.50F)
        );

        DrawLineV(
            {center.x, center.y + mark_inner},
            {center.x, center.y + mark_outer},
            Fade(RAYWHITE, 0.50F)
        );

        DrawLineV(
            {center.x - mark_inner, center.y},
            {center.x - mark_outer, center.y},
            Fade(RAYWHITE, 0.50F)
        );

        DrawLineV(
            {center.x + mark_inner, center.y},
            {center.x + mark_outer, center.y},
            Fade(RAYWHITE, 0.50F)
        );
    }

    const Vector2 knob{
        center.x + value.x * radius * 0.72F,
        center.y + value.y * radius * 0.72F
    };

    const float knob_radius =
        radius * (subtle ? 0.27F : 0.35F);

    DrawCircleV(
        knob,
        knob_radius,
        Fade(
            RAYWHITE,
            subtle ? 0.12F : 0.42F
        )
    );

    draw_ring(
        knob,
        knob_radius,
        Fade(
            RAYWHITE,
            subtle ? 0.15F : 0.72F
        )
    );
}


void draw_button_base(
    Vector2 center,
    float radius,
    float opacity,
    bool active,
    bool primary
) {
    const float active_boost =
        active ? 0.24F : 0.0F;

    const float fill =
        std::clamp(
            opacity + active_boost,
            0.0F,
            0.88F
        );

    // Soft outer halo.
    if (active || primary) {
        DrawCircleV(
            center,
            radius * 1.12F,
            Fade(
                RAYWHITE,
                active ? 0.15F : 0.055F
            )
        );
    }

    DrawCircleV(
        center,
        radius,
        Fade(BLACK, fill)
    );

    draw_ring(
        center,
        radius,
        Fade(
            RAYWHITE,
            active
                ? 0.95F
                : primary
                    ? 0.78F
                    : 0.58F
        )
    );
}


void draw_label(
    Vector2 center,
    float radius,
    const char* text,
    bool active
) {
    if (
        text == nullptr ||
        text[0] == '\0'
    ) {
        return;
    }

    const int font_size =
        static_cast<int>(
            std::max(
                11.0F,
                radius * 0.31F
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
            text_width * 0.5F
        ),
        static_cast<int>(
            center.y -
            font_size * 0.5F
        ),
        font_size,
        Fade(
            RAYWHITE,
            active ? 1.0F : 0.88F
        )
    );
}


void draw_text_button(
    Vector2 center,
    float radius,
    float opacity,
    const char* text,
    bool active,
    bool primary = false
) {
    draw_button_base(
        center,
        radius,
        opacity,
        active,
        primary
    );

    draw_label(
        center,
        radius,
        text,
        active
    );
}


void draw_fire_button(
    Vector2 center,
    float radius,
    float opacity,
    bool active
) {
    draw_button_base(
        center,
        radius,
        opacity,
        active,
        true
    );

    // OUTLAND fire glyph:
    // central impact dot + four short directional marks.
    DrawCircleV(
        center,
        radius * 0.14F,
        Fade(RAYWHITE, 0.92F)
    );

    const float inner =
        radius * 0.31F;

    const float outer =
        radius * 0.57F;

    DrawLineV(
        {center.x, center.y - inner},
        {center.x, center.y - outer},
        RAYWHITE
    );

    DrawLineV(
        {center.x, center.y + inner},
        {center.x, center.y + outer},
        RAYWHITE
    );

    DrawLineV(
        {center.x - inner, center.y},
        {center.x - outer, center.y},
        RAYWHITE
    );

    DrawLineV(
        {center.x + inner, center.y},
        {center.x + outer, center.y},
        RAYWHITE
    );

    const char* label = "FIRE";

    const int font_size =
        static_cast<int>(
            std::max(
                10.0F,
                radius * 0.22F
            )
        );

    const int text_width =
        MeasureText(
            label,
            font_size
        );

    DrawText(
        label,
        static_cast<int>(
            center.x -
            text_width * 0.5F
        ),
        static_cast<int>(
            center.y +
            radius * 0.62F
        ),
        font_size,
        Fade(RAYWHITE, 0.90F)
    );
}


void draw_aim_button(
    Vector2 center,
    float radius,
    float opacity,
    bool active
) {
    draw_button_base(
        center,
        radius,
        opacity,
        active,
        false
    );

    const float reticle =
        radius * 0.43F;

    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        reticle,
        Fade(RAYWHITE, 0.90F)
    );

    const float gap =
        radius * 0.16F;

    const float reach =
        radius * 0.62F;

    DrawLineV(
        {center.x, center.y - gap},
        {center.x, center.y - reach},
        RAYWHITE
    );

    DrawLineV(
        {center.x, center.y + gap},
        {center.x, center.y + reach},
        RAYWHITE
    );

    DrawLineV(
        {center.x - gap, center.y},
        {center.x - reach, center.y},
        RAYWHITE
    );

    DrawLineV(
        {center.x + gap, center.y},
        {center.x + reach, center.y},
        RAYWHITE
    );

    DrawCircleV(
        center,
        radius * 0.07F,
        RAYWHITE
    );
}


void draw_jump_vault_button(
    Vector2 center,
    float radius,
    float opacity,
    bool active
) {
    draw_button_base(
        center,
        radius,
        opacity,
        active,
        false
    );

    // Simple climbing figure / upward motion glyph.
    DrawCircleV(
        {
            center.x,
            center.y - radius * 0.28F
        },
        radius * 0.11F,
        RAYWHITE
    );

    DrawLineEx(
        {
            center.x,
            center.y - radius * 0.13F
        },
        {
            center.x,
            center.y + radius * 0.16F
        },
        std::max(
            2.0F,
            radius * 0.07F
        ),
        RAYWHITE
    );

    DrawLineEx(
        {
            center.x,
            center.y
        },
        {
            center.x - radius * 0.25F,
            center.y - radius * 0.12F
        },
        std::max(
            2.0F,
            radius * 0.055F
        ),
        RAYWHITE
    );

    DrawLineEx(
        {
            center.x,
            center.y
        },
        {
            center.x + radius * 0.27F,
            center.y - radius * 0.20F
        },
        std::max(
            2.0F,
            radius * 0.055F
        ),
        RAYWHITE
    );

    DrawLineEx(
        {
            center.x,
            center.y + radius * 0.15F
        },
        {
            center.x - radius * 0.22F,
            center.y + radius * 0.37F
        },
        std::max(
            2.0F,
            radius * 0.055F
        ),
        RAYWHITE
    );

    DrawLineEx(
        {
            center.x,
            center.y + radius * 0.15F
        },
        {
            center.x + radius * 0.25F,
            center.y + radius * 0.34F
        },
        std::max(
            2.0F,
            radius * 0.055F
        ),
        RAYWHITE
    );
}

} // namespace


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


    // ========================================================
    // LEFT THUMB — MOVEMENT
    // ========================================================

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
        layout.movement.opacity,
        false
    );


    // ========================================================
    // RIGHT THUMB — FREE LOOK
    // ========================================================
    //
    // Intentionally faint. Eventually the whole right-side
    // gameplay area becomes a free-look surface.

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
        layout.look.opacity,
        true
    );


    // ========================================================
    // PRIMARY COMBAT CLUSTER
    // ========================================================

    draw_fire_button(
        position_of(
            layout.fire,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.fire.scale,
        layout.fire.opacity,
        input.fire
    );

    draw_aim_button(
        position_of(
            layout.aim,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.aim.scale,
        layout.aim.opacity,
        input.aim
    );

    draw_jump_vault_button(
        position_of(
            layout.jump,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.jump.scale,
        layout.jump.opacity,
        input.jump
    );


    // ========================================================
    // SECONDARY COMBAT CONTROLS
    // ========================================================

    draw_text_button(
        position_of(
            layout.reload,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.reload.scale,
        layout.reload.opacity,
        "LOAD",
        input.reload
    );

    draw_text_button(
        position_of(
            layout.weapon,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.weapon.scale,
        layout.weapon.opacity,
        "GUN",
        input.next_weapon
    );

    draw_text_button(
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
}

} // namespace outland::input
