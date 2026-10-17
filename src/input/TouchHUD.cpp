#include "outland/input/TouchHUD.hpp"
#include "outland/input/InputSystem.hpp"

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
    return touch_scale(width, height);
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
    int screen_height,
    bool gameplay,
    const char* fire_mode,
    const char* sight,
    bool driving,
    bool look_stick,
    bool skydiving
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


    const auto button = [&](const TouchElementLayout& e, const char* label, bool active) {
        draw_text_button(position_of(e, screen_width, screen_height),
            base_button_radius * e.scale, e.opacity, label, active);
    };
    if (gameplay && skydiving) {
        // The drop: steer with the stick, JUMP leaves the helicopter / opens the parachute; the
        // rest of the right side is for looking (and so for steering the canopy).
        draw_stick(position_of(layout.movement, screen_width, screen_height), {input.move_x, input.move_y},
                   base_stick_radius * layout.movement.scale, layout.movement.opacity, false);
        const Vector2 jump = position_of(layout.jump, screen_width, screen_height);
        const float radius = base_button_radius * layout.jump.scale * 1.25F;
        draw_jump_vault_button(jump, radius, std::max(layout.jump.opacity, .85F), input.jump);
        const char* label = "JUMP";
        const int size = static_cast<int>(16 * scale);
        DrawText(label, static_cast<int>(jump.x - MeasureText(label, size) / 2.0F), static_cast<int>(jump.y + radius + 6 * scale), size, RAYWHITE);
        return;
    }
    if (gameplay && driving) {
        // Left thumb: steering only (the knob slides sideways).
        draw_stick(position_of(layout.movement, screen_width, screen_height), {input.move_x, 0},
                   base_stick_radius * layout.movement.scale, layout.movement.opacity, false);
        // Right thumb: the pedals, tall and easy to find without looking.
        const auto pedal = [&](const TouchElementLayout& e, const char* label, float pressed, Color tint) {
            const Vector2 c = position_of(e, screen_width, screen_height);
            const float w = 46 * scale * e.scale, h = 70 * scale * e.scale;
            const Rectangle r{c.x - w, c.y - h, w * 2, h * 2};
            const unsigned char a = static_cast<unsigned char>(255 * std::clamp(e.opacity + pressed * .35F, 0.0F, 1.0F));
            DrawRectangleRounded(r, .3F, 8, Color{tint.r, tint.g, tint.b, a});
            DrawRectangleRoundedLinesEx(r, .3F, 8, 2.5F, Color{255, 255, 255, static_cast<unsigned char>(a / 2 + 60)});
            for (int k = 0; k < 4; ++k) // tread lines
                DrawRectangle(static_cast<int>(r.x + w * .45F), static_cast<int>(r.y + h * (.45F + k * .32F)), static_cast<int>(w * 1.1F), 3,
                              Color{255, 255, 255, static_cast<unsigned char>(a / 3)});
            const int size = static_cast<int>(17 * scale * e.scale);
            DrawText(label, static_cast<int>(c.x - MeasureText(label, size) / 2.0F), static_cast<int>(r.y + 10 * scale), size, RAYWHITE);
        };
        pedal(layout.gas, "GAS", input.gas, Color{40, 140, 70, 255});
        pedal(layout.brake_pedal, "BRAKE", input.brake_pedal, Color{160, 50, 45, 255});
        button(layout.handbrake, "HAND", input.brake);
        button(layout.interact, "EXIT", input.interact);
        button(layout.sprint, "BOOST", input.sprint);
        if (drive_by_) button(layout.fire_left, "FIRE", input.fire);
        if (police_car_) button(layout.lights, lights_on_ ? "LIGHTS ON" : "LIGHTS", lights_on_ || input.lights);
        // The pan area above the pedals: a faint hint while no thumb is on it.
        const Rectangle pan = InputSystem::pan_area(screen_width, screen_height);
        if (input.look_dx == 0 && input.look_dy == 0) {
            const char* hint = "SWIPE TO LOOK";
            const int size = static_cast<int>(14 * scale);
            DrawText(hint, static_cast<int>(pan.x + pan.width / 2 - MeasureText(hint, size) / 2.0F), static_cast<int>(pan.y + pan.height - 22 * scale), size,
                     Color{255, 255, 255, 70});
        }
        return;
    }
    if (gameplay) {
    button(layout.sprint, "SPRINT", input.sprint);
    button(layout.crouch, "CROUCH", input.crouch);
    button(layout.inventory, "BAG", input.inventory);
    if (usable_) button(layout.interact, "USE", input.interact);
    if (tagger_) button(layout.tag, "TAG", input.tag);
    if (sight && more_) button(layout.sight, sight, input.sight);
    }

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
    // In play and while building, the whole right of the screen is the look surface (swipe
    // anywhere, or slide the thumb on FIRE), as in PUBG; only the Creator keeps a look stick.

    if (look_stick) draw_stick(position_of(layout.look, screen_width, screen_height), {input.look_x, input.look_y},
                              base_stick_radius * layout.look.scale, layout.look.opacity, true);


    // ========================================================
    // PRIMARY COMBAT CLUSTER
    // ========================================================

    if (!gameplay) return;

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

    // Left trigger: same glyph, smaller, above the movement stick.
    draw_fire_button(
        position_of(
            layout.fire_left,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.fire_left.scale,
        layout.fire_left.opacity,
        input.fire
    );

    if (fire_mode && more_) draw_text_button(
        position_of(
            layout.fire_mode,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.fire_mode.scale,
        layout.fire_mode.opacity,
        fire_mode,
        input.fire_mode
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

    if (grenade_) draw_text_button(
        position_of(
            layout.grenade,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.grenade.scale,
        layout.grenade.opacity,
        "NADE",
        input.grenade
    );

    if (heal_) draw_text_button(
        position_of(
            layout.heal,
            screen_width,
            screen_height
        ),
        base_button_radius *
            layout.heal.scale,
        layout.heal.opacity,
        "HEAL",
        input.heal
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
        more_ ? "VIEW" : "MORE",
        input.toggle_view
    );
}

} // namespace outland::input
