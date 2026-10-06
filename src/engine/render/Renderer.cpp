#include "outland/engine/render/Renderer.hpp"

#include "outland/input/InputSystem.hpp"
#include "outland/input/TouchHUD.hpp"
#include "outland/game/GameMode.hpp"
#include "outland/game/HomeScreen.hpp"
#include "outland/dev/DevLab.hpp"
#include "outland/player/VerdanCharacter.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainWorld.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/foliage/FoliageSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"

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

    game::HomeScreen home_screen;
    dev::DevLab dev_lab;

    game::GameMode game_mode =
        game::GameMode::Home;

    world::VerdaRegion verda_region;
    world::terrain::TerrainWorld terrain_world;
    world::foliage::FoliageSystem foliage_system;

    float character_animation_time = 0.0F;

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
        // HOME SCREEN
        // ====================================================

        if (
            game_mode ==
            game::GameMode::Home
        ) {
            const game::GameMode selected =
                home_screen.update(
                    screen_width,
                    screen_height
                );

            BeginDrawing();

            home_screen.draw(
                screen_width,
                screen_height
            );

            EndDrawing();

            if (
                selected !=
                game::GameMode::Home
            ) {
                game_mode =
                    selected;

                player.position = {
                    0.0F,
                    world::terrain::TerrainHeight::sample(
                        0.0F,
                        8.0F
                    ) + 1.0F,
                    8.0F
                };

                player.vertical_velocity =
                    0.0F;

                player.grounded =
                    true;

                character_animation_time =
                    0.0F;
            }

            continue;
        }

        // ====================================================
        // RETURN HOME
        // ====================================================

        if (
            IsKeyPressed(KEY_ESCAPE)
        ) {
            game_mode =
                game::GameMode::Home;

            continue;
        }

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
        // DEV LAB
        // ====================================================

        if (
            game_mode ==
            game::GameMode::DevLab
        ) {
            dev_lab.update();

            if (
                dev_lab.teleport_requested()
            ) {
                player.position =
                    dev_lab.spawn_position();

                player.vertical_velocity =
                    0.0F;

                player.grounded =
                    true;

                dev_lab.clear_teleport();
            }
        }

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

        const float movement_strength =
            Vector3Length(movement);

        if (
            movement_strength >
            0.01F
        ) {
            /*
             * Preserve analog stick magnitude.
             *
             * Only clamp movement when combined inputs
             * exceed the valid unit-vector range.
             */
            if (
                movement_strength >
                1.0F
            ) {
                movement =
                    Vector3Scale(
                        movement,
                        1.0F /
                        movement_strength
                    );
            }

            const Vector3 desired_position =
                Vector3Add(
                    player.position,
                    Vector3Scale(
                        movement,
                        speed * dt
                    )
                );

            player.position =
                world::physics::WorldCollision::
                    resolve_player_movement(
                        player.position,
                        desired_position,
                        verda_region,
                        0.45F
                    );
        }

        // ====================================================
        // TERRAIN FOLLOW
        // ====================================================

        if (player.grounded) {
            player.position.y =
                world::terrain::TerrainHeight::sample(
                    player.position.x,
                    player.position.z
                ) + 1.0F;
        }

        const float movement_amount =
            std::clamp(
                std::sqrt(
                    controls.move_x *
                    controls.move_x +
                    controls.move_y *
                    controls.move_y
                ),
                0.0F,
                1.0F
            );

        if (movement_amount > 0.02F) {
            character_animation_time +=
                dt;
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

            const float terrain_ground =
                world::terrain::TerrainHeight::sample(
                    player.position.x,
                    player.position.z
                );

            const float player_ground_y =
                terrain_ground + 1.0F;

            if (
                player.position.y <=
                player_ground_y
            ) {
                player.position.y =
                    player_ground_y;

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
        // VERDA TERRAIN
        // ----------------------------------------------------

        terrain_world.draw();

        // ----------------------------------------------------
        // VERDA FOLIAGE
        // ----------------------------------------------------

        foliage_system.draw(
            camera.position
        );


        // ----------------------------------------------------
        // TRAINING ARENA
        //
        // Temporary flat development pad.
        // This stays while player, targets and structures
        // are converted to terrain-aware placement.
        // ----------------------------------------------------

        /*
         * The procedural terrain is now the ground.
         *
         * The old 100 x 100 development plane and
         * debug grid have been retired.
         */

        // First region of Verda.
        verda_region.draw();

        // Central training structure.
        const float structure_ground_y =
            world::terrain::TerrainHeight::sample(
                0.0F,
                0.0F
            );

        const Vector3 structure_position{
            0.0F,
            structure_ground_y + 1.5F,
            0.0F
        };

        DrawCube(
            structure_position,
            4.0F,
            3.0F,
            4.0F,
            DARKGRAY
        );

        DrawCubeWires(
            structure_position,
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
            const float target_x =
                static_cast<float>(
                    i * 4
                );

            const float target_z =
                -18.0F;

            const float target_ground_y =
                world::terrain::TerrainHeight::sample(
                    target_x,
                    target_z
                );

            DrawCube(
                {
                    target_x,
                    target_ground_y + 1.0F,
                    target_z
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

            const float player_ground_y =
                world::terrain::TerrainHeight::sample(
                    player.position.x,
                    player.position.z
                );

            const Vector3 character_feet{
                player.position.x,
                player_ground_y,
                player.position.z
            };

            player::VerdanCharacter::draw(
                character_feet,
                player.yaw,
                movement_amount,
                character_animation_time
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
            game::game_mode_name(
                game_mode
            ),
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

        // ====================================================
        // TEMPORARY MOVEMENT DIAGNOSTICS
        // ====================================================

        DrawText(
            TextFormat(
                "STICK X: %.2f  Y: %.2f",
                controls.move_x,
                controls.move_y
            ),
            20,
            100,
            22,
            YELLOW
        );

        DrawText(
            TextFormat(
                "MOVE X: %.2f  Z: %.2f",
                movement.x,
                movement.z
            ),
            20,
            128,
            22,
            YELLOW
        );

        DrawText(
            TextFormat(
                "YAW: %.2f",
                player.yaw
            ),
            20,
            156,
            22,
            YELLOW
        );

        if (
            game_mode ==
            game::GameMode::DevLab
        ) {
            dev_lab.draw_overlay(
                player.position,
                player.yaw,
                player.pitch,
                player.grounded
            );
        }

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
