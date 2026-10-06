#include "outland/engine/render/Renderer.hpp"

#include "outland/input/InputSystem.hpp"
#include "outland/input/TouchHUD.hpp"
#include "outland/world/VerdaRegion.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace outland::engine {

namespace {

constexpr float PI_F =
    3.14159265358979323846F;

struct PlayerState {
    Vector3 position{
        0.0F,
        1.0F,
        8.0F
    };

    float vertical_velocity{0.0F};

    float yaw{PI_F};
    float pitch{-0.15F};

    bool grounded{true};
    bool third_person{true};
};

}

// ============================================================
// INITIALIZATION
// ============================================================

bool Renderer::initialize(
    int width,
    int height
) {
    width_ = width;
    height_ = height;

    SetConfigFlags(
        FLAG_MSAA_4X_HINT |
        FLAG_WINDOW_RESIZABLE
    );

    InitWindow(
        width_,
        height_,
        "OUTLAND - Training Arena"
    );

    if (!IsWindowReady()) {
        initialized_ = false;
        return false;
    }

    SetTargetFPS(60);

    initialized_ = true;

    return true;
}

// ============================================================
// GAME / RENDER LOOP
// ============================================================

void Renderer::run() {
    if (!initialized_) {
        return;
    }

    PlayerState player;

    input::InputSystem input_system;
    input::TouchHUD touch_hud;

    world::VerdaRegion verda_region;

    Camera3D camera{};

    camera.up = {
        0.0F,
        1.0F,
        0.0F
    };

    camera.fovy = 70.0F;
    camera.projection =
        CAMERA_PERSPECTIVE;

    constexpr float walk_speed =
        6.0F;

    constexpr float sprint_speed =
        10.0F;

    constexpr float gravity =
        22.0F;

    constexpr float jump_speed =
        8.5F;

    while (!WindowShouldClose()) {

        const float dt =
            std::min(
                GetFrameTime(),
                0.05F
            );

        const int screen_width =
            GetScreenWidth();

        const int screen_height =
            GetScreenHeight();

        // ====================================================
        // INPUT
        // ====================================================

        input_system.update(
            screen_width,
            screen_height
        );

        const input::PlayerInput&
            controls =
                input_system.player();

        // ====================================================
        // VIEW MODE
        // ====================================================

        if (controls.toggle_view) {
            player.third_person =
                !player.third_person;
        }

        // ====================================================
        // CAMERA LOOK
        // ====================================================

        const float look_speed =
            input_system
                .layout()
                .look_sensitivity;

        player.yaw -=
            controls.look_x *
            look_speed *
            dt;

        player.pitch -=
            controls.look_y *
            look_speed *
            dt;

        player.pitch =
            std::clamp(
                player.pitch,
                -1.2F,
                1.0F
            );

        // ====================================================
        // MOVEMENT DIRECTIONS
        // ====================================================

        const Vector3 forward{
            std::sin(player.yaw),
            0.0F,
            std::cos(player.yaw)
        };

        const Vector3 right{
            std::cos(player.yaw),
            0.0F,
            -std::sin(player.yaw)
        };

        const float speed =
            controls.sprint
                ? sprint_speed
                : walk_speed;

        Vector3 movement =
            Vector3Add(
                Vector3Scale(
                    forward,
                    -controls.move_y
                ),
                Vector3Scale(
                    right,
                    controls.move_x
                )
            );

        if (
            Vector3Length(movement) >
            0.01F
        ) {
            movement =
                Vector3Normalize(
                    movement
                );

            player.position =
                Vector3Add(
                    player.position,
                    Vector3Scale(
                        movement,
                        speed * dt
                    )
                );
        }

        // ====================================================
        // JUMP + GRAVITY
        // ====================================================

        if (
            controls.jump &&
            player.grounded
        ) {
            player.vertical_velocity =
                jump_speed;

            player.grounded =
                false;
        }

        if (!player.grounded) {

            player.vertical_velocity -=
                gravity * dt;

            player.position.y +=
                player.vertical_velocity *
                dt;

            if (
                player.position.y <=
                1.0F
            ) {
                player.position.y =
                    1.0F;

                player.vertical_velocity =
                    0.0F;

                player.grounded =
                    true;
            }
        }

        // ====================================================
        // CAMERA POSITION
        // ====================================================

        const Vector3 look_direction{
            std::sin(player.yaw) *
                std::cos(player.pitch),

            std::sin(player.pitch),

            std::cos(player.yaw) *
                std::cos(player.pitch)
        };

        const Vector3 head_position{
            player.position.x,
            player.position.y + 0.75F,
            player.position.z
        };

        if (player.third_person) {

            constexpr float
                camera_distance =
                    6.0F;

            camera.target =
                head_position;

            camera.position =
                Vector3Subtract(
                    head_position,
                    Vector3Scale(
                        look_direction,
                        camera_distance
                    )
                );

            camera.position.y +=
                2.0F;
        }
        else {
            camera.position =
                head_position;

            camera.target =
                Vector3Add(
                    head_position,
                    look_direction
                );
        }

        // ====================================================
        // DRAW
        // ====================================================

        BeginDrawing();

        ClearBackground(
            Color{
                135,
                180,
                220,
                255
            }
        );

        BeginMode3D(camera);

        // ----------------------------------------------------
        // TRAINING ARENA
        // ----------------------------------------------------

        DrawPlane(
            {
                0.0F,
                0.0F,
                0.0F
            },
            {
                100.0F,
                100.0F
            },
            Color{
                94,
                125,
                75,
                255
            }
        );

        DrawGrid(
            50,
            2.0F
        );

        // First region of Verda.
        verda_region.draw();

        // Central structure.
        DrawCube(
            {
                0.0F,
                1.5F,
                0.0F
            },
            4.0F,
            3.0F,
            4.0F,
            DARKGRAY
        );

        DrawCubeWires(
            {
                0.0F,
                1.5F,
                0.0F
            },
            4.0F,
            3.0F,
            4.0F,
            BLACK
        );

        // Range targets.
        for (
            int i = -4;
            i <= 4;
            ++i
        ) {
            DrawCube(
                {
                    static_cast<float>(
                        i * 4
                    ),
                    1.0F,
                    -18.0F
                },
                1.0F,
                2.0F,
                0.5F,
                RED
            );
        }

        // ----------------------------------------------------
        // PLAYER BODY
        // ----------------------------------------------------

        if (player.third_person) {

            DrawCube(
                player.position,
                0.8F,
                2.0F,
                0.8F,
                BLUE
            );

            DrawCubeWires(
                player.position,
                0.8F,
                2.0F,
                0.8F,
                BLACK
            );
        }

        EndMode3D();

        // ====================================================
        // HUD
        // ====================================================

        DrawText(
            "OUTLAND",
            12,
            10,
            26,
            BLACK
        );

        DrawText(
            "TRAINING ARENA",
            12,
            40,
            14,
            DARKGRAY
        );

        DrawText(
            player.third_person
                ? "TPS"
                : "FPS",
            12,
            60,
            14,
            BLACK
        );

        // Crosshair.
        const int center_x =
            screen_width / 2;

        const int center_y =
            screen_height / 2;

        DrawLine(
            center_x - 7,
            center_y,
            center_x + 7,
            center_y,
            BLACK
        );

        DrawLine(
            center_x,
            center_y - 7,
            center_x,
            center_y + 7,
            BLACK
        );

        // Input diagnostics.
        if (controls.fire) {
            DrawText(
                "FIRE",
                center_x - 20,
                center_y + 25,
                18,
                RED
            );
        }

        if (controls.aim) {
            DrawText(
                "AIM",
                center_x - 18,
                center_y + 48,
                18,
                DARKBLUE
            );
        }

        touch_hud.draw(
            controls,
            input_system.layout(),
            screen_width,
            screen_height
        );

        DrawFPS(
            screen_width - 90,
            10
        );

        EndDrawing();
    }
}

// ============================================================
// SHUTDOWN
// ============================================================

void Renderer::shutdown() {
    if (!initialized_) {
        return;
    }

    CloseWindow();

    initialized_ = false;
}

bool Renderer::initialized() const {
    return initialized_;
}

}
