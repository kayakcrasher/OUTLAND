#include "outland/engine/render/Renderer.hpp"
#include "outland/engine/audio/EnvironmentAudio.hpp"
#include "outland/game/combat/CombatRenderer.hpp"

#include "outland/input/InputSystem.hpp"
#include "outland/input/TouchHUD.hpp"
#include "outland/game/GameMode.hpp"
#include "outland/game/HomeScreen.hpp"
#include "outland/dev/DevLab.hpp"

#ifdef OUTLAND_DEV_TOOLS
#include "outland/creator/CreatorController.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/creator/CreatorTouchUI.hpp"
#endif
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
#include <unordered_map>

namespace outland::engine {

namespace {

std::unordered_map<std::string, Model>&
world_asset_model_cache() {
    static std::unordered_map<std::string, Model> cache;
    return cache;
}

Model* get_world_asset_model(
    const std::string& path
) {
    if (path.empty()) {
        return nullptr;
    }

    auto& cache = world_asset_model_cache();

    const auto found = cache.find(path);

    if (found != cache.end()) {
        return &found->second;
    }

    if (!FileExists(path.c_str())) {
        TraceLog(
            LOG_WARNING,
            "OUTLAND asset missing: %s",
            path.c_str()
        );
        return nullptr;
    }

    Model model = LoadModel(path.c_str());

    if (model.meshCount <= 0) {
        TraceLog(
            LOG_WARNING,
            "OUTLAND asset failed to load: %s",
            path.c_str()
        );
        return nullptr;
    }

    auto result = cache.emplace(
        path,
        std::move(model)
    );

    return &result.first->second;
}

void draw_model_backed_world_assets(
    const world::VerdaRegion& region,
    const Vector3& camera_position
) {
    constexpr float max_distance = 320.0F;
    constexpr float max_distance_sq =
        max_distance * max_distance;

    for (
        const world::Settlement& settlement :
        region.settlements()
    ) {
        for (
            const world::WorldAsset& asset :
            settlement.assets
        ) {
            if (asset.model_path.empty()) {
                continue;
            }

            const float dx =
                asset.position.x - camera_position.x;

            const float dy =
                asset.position.y - camera_position.y;

            const float dz =
                asset.position.z - camera_position.z;

            const float distance_sq =
                dx * dx +
                dy * dy +
                dz * dz;

            if (distance_sq > max_distance_sq) {
                continue;
            }

            Model* model =
                get_world_asset_model(
                    asset.model_path
                );

            if (model == nullptr) {
                continue;
            }

            DrawModelEx(
                *model,
                asset.position,
                {0.0F, 1.0F, 0.0F},
                asset.rotation_y,
                {1.0F, 1.0F, 1.0F},
                WHITE
            );
        }
    }
}

#ifdef OUTLAND_DEV_TOOLS
void draw_creator_model_preview(
    const creator::CreatorController& controller
) {
    const creator::CreatorPreview& preview =
        controller.preview();

    if (!preview.valid) {
        return;
    }

    const creator::CreatorAssetDefinition* asset =
        controller.selected_asset();

    if (asset == nullptr) {
        return;
    }

    if (asset->model_path.empty()) {
        return;
    }

    Model* model =
        get_world_asset_model(asset->model_path);

    if (model == nullptr) {
        return;
    }

    const Color ghost_color =
        Fade(GREEN, 0.58F);

    DrawModelEx(
        *model,
        preview.position,
        {0.0F, 1.0F, 0.0F},
        preview.rotation_y,
        {
            asset->default_scale,
            asset->default_scale,
            asset->default_scale
        },
        ghost_color
    );

    const float width =
        asset->footprint.width *
        asset->default_scale;

    const float depth =
        asset->footprint.depth *
        asset->default_scale;

    const float height =
        asset->footprint.height *
        asset->default_scale;

    DrawCubeWires(
        {
            preview.position.x,
            preview.position.y +
                height * 0.5F,
            preview.position.z
        },
        width,
        height,
        depth,
        GREEN
    );
}
#endif


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
    float pitch{-0.01F};

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

#ifdef OUTLAND_DEV_TOOLS
    creator::CreatorController creator_controller;
    creator::CreatorTouchUI creator_touch_ui;
#endif

    game::GameMode game_mode =
        game::GameMode::Home;

    world::VerdaRegion verda_region;
    world::terrain::TerrainWorld terrain_world;
    world::foliage::FoliageSystem foliage_system;
    audio::EnvironmentAudio environment_audio(
        std::string(GetApplicationDirectory()) + "assets/audio/environment"
    );

    game::combat::WeaponSystem weapons;
    game::combat::CombatWorld combat_world(verda_region);
    float recoil_pitch = 0.0F, recoil_yaw = 0.0F;
    bool trigger_ready = false;
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

        const Rectangle audio_button{static_cast<float>(screen_width - 174), 44.0F, 162.0F, 34.0F};
        if (IsKeyPressed(KEY_M) ||
            (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), audio_button))) {
            environment_audio.toggle_mute();
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) environment_audio.set_volume(environment_audio.volume() - 0.1F);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) environment_audio.set_volume(environment_audio.volume() + 0.1F);

        // ====================================================
        // HOME SCREEN
        // ====================================================

        if (
            game_mode ==
            game::GameMode::Home
        ) {
            environment_audio.update(player.position, player.grounded, false, verda_region);
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

                character_animation_time = 0.0F;
                weapons.reset(game_mode == game::GameMode::DevLab);
                combat_world.reset_targets();
                recoil_pitch = recoil_yaw = 0.0F;
                trigger_ready = false;
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
            -std::cos(player.yaw),
            0.0F,
            std::sin(player.yaw)
        };

        const float speed = controls.aim ? walk_speed * 0.55F :
                            controls.sprint ? sprint_speed : walk_speed;

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
        // WINDOW VAULT
        // ====================================================
        //
        // JUMP near a real window opening performs an
        // intentional vault to the opposite side.
        //
        // The collision system determines whether the player
        // is aligned with and facing a valid window.

        if (controls.jump && player.grounded) {
            Vector3 vault_landing{};

            const Vector3 vault_forward{
                -forward.x,
                0.0F,
                -forward.z
            };

            if (
                world::physics::WorldCollision::
                    window_vault_target(
                        player.position,
                        vault_forward,
                        verda_region,
                        vault_landing
                    )
            ) {
                player.position.x =
                    vault_landing.x;

                player.position.z =
                    vault_landing.z;

                // Snap onto terrain on the destination side.
                player.position.y =
                    world::terrain::TerrainHeight::sample(
                        player.position.x,
                        player.position.z
                    ) + 1.0F;

                // Cancel vertical jump velocity so vaulting
                // does not immediately launch the player.
                player.vertical_velocity = 0.0F;
                player.grounded = true;
            }
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

        environment_audio.update(player.position, player.grounded, true, verda_region);

        recoil_pitch *= std::exp(-dt * 5.0F);
        recoil_yaw *= std::exp(-dt * 7.0F);
        const float shot_pitch = std::clamp(player.pitch + recoil_pitch, -1.2F, 1.15F);
        const float shot_yaw = player.yaw + recoil_yaw;
        const Vector3 look_direction{std::sin(shot_yaw) * std::cos(shot_pitch),
            std::sin(shot_pitch), std::cos(shot_yaw) * std::cos(shot_pitch)};
        const Vector3 gun_right{std::cos(shot_yaw), 0.0F, -std::sin(shot_yaw)};

        const Vector3 head_position{
            player.position.x,
            player.position.y + 0.75F,
            player.position.z
        };

        const float desired_fov = controls.aim ? 50.0F : 70.0F;
        camera.fovy += (desired_fov-camera.fovy) * (1.0F-std::exp(-dt*12.0F));
        if (player.third_person) {
            const float distance = controls.aim ? 2.5F : 5.0F;
            camera.position = Vector3Add(Vector3Subtract(head_position, Vector3Scale(look_direction,distance)),
                Vector3Add(Vector3Scale(gun_right,controls.aim ? .4F : .75F), {0,controls.aim ? .25F : .8F,0}));
            const auto obstruction = combat_world.trace_segment(head_position,camera.position);
            if (obstruction.hit()) {
                camera.position=Vector3Lerp(head_position,camera.position,
                    std::max(0.0F,obstruction.fraction-.08F));
            }
            camera.target=Vector3Add(head_position,Vector3Scale(look_direction,25.0F));
        } else {
            camera.position=head_position;
            camera.target=Vector3Add(head_position,look_direction);
        }

        if (game_mode == game::GameMode::DevLab && IsKeyPressed(KEY_T)) combat_world.reset_targets();
        combat_world.update(dt,game_mode == game::GameMode::DevLab);
        const Vector3 camera_direction=Vector3Normalize(Vector3Subtract(camera.target,camera.position));

#ifdef OUTLAND_DEV_TOOLS
        {
            const bool creator_active =
                game_mode == game::GameMode::DevLab;

            creator_controller.set_enabled(
                creator_active
            );

            if (creator_active) {
                creator_controller.update(
                    verda_region,
                    camera.position,
                    camera_direction
                );

                creator_touch_ui.update(
                    creator_controller,
                    screen_width,
                    screen_height
                );

                const auto& creator_actions =
                    creator_touch_ui.actions();

                // ====================================================
                // CREATOR WORLD PICKING
                //
                // X11 mouse clicks use the actual screen position.
                // UI owns its pixels, so editor buttons never select
                // world geometry behind them.
                // ====================================================
                if (
                    creator_touch_ui.active_tool() ==
                        creator::CreatorTouchTool::Select &&
                    IsMouseButtonPressed(
                        MOUSE_BUTTON_LEFT
                    )
                ) {
                    const Vector2 pointer =
                        GetMousePosition();

                    if (
                        !creator_touch_ui.pointer_over_ui(
                            pointer,
                            screen_width,
                            screen_height
                        )
                    ) {
                        const Ray pick_ray =
                            GetScreenToWorldRay(
                                pointer,
                                camera
                            );

                        (void)
                            creator_controller.select_target(
                                verda_region,
                                pick_ray.position,
                                pick_ray.direction
                            );
                    }
                }


                if (creator_actions.place) {
                    (void)creator_controller.place_selected(
                        verda_region
                    );
                }

                if (creator_actions.save) {
                    const bool saved =
                        creator::CreatorMapIO::save(
                            verda_region,
                            "maps/verda_creator.map"
                        );

                    TraceLog(
                        saved ? LOG_INFO : LOG_ERROR,
                        saved
                            ? "Creator map saved"
                            : "Creator map save FAILED"
                    );
                }

                if (creator_actions.load) {
                    const bool loaded =
                        creator::CreatorMapIO::load(
                            verda_region,
                            "maps/verda_creator.map"
                        );

                    TraceLog(
                        loaded ? LOG_INFO : LOG_ERROR,
                        loaded
                            ? "Creator map loaded"
                            : "Creator map load FAILED"
                    );
                }

                if (creator_actions.move) {
                    (void)creator_controller.move_selected(
                        verda_region
                    );
                }

                if (creator_actions.duplicate) {
                    (void)creator_controller.duplicate_selected(
                        verda_region
                    );
                }

                if (creator_actions.rotate) {
                    if (
                        !creator_controller.rotate_selected(
                            verda_region,
                            15.0F
                        )
                    ) {
                        creator_controller.rotate_preview(
                            15.0F
                        );
                    }
                }

                if (creator_actions.erase) {
                    creator_controller.delete_selected(
                        verda_region
                    );
                }
            }
        }
#endif
        const Vector3 far_point=Vector3Add(camera.position,Vector3Scale(camera_direction,500.0F));
        const auto aimed_hit=combat_world.trace_segment(camera.position,far_point);
        const Vector3 aim_point=aimed_hit.hit() ? aimed_hit.position : far_point;
        Vector3 gun_muzzle=Vector3Add(head_position,
            Vector3Add(Vector3Scale(look_direction,weapons.selected()==game::combat::WeaponId::Rifle ? .95F : .65F),
                Vector3Add(Vector3Scale(gun_right,controls.aim && !player.third_person ? 0.0F : .23F),
                    {0,controls.aim ? -.12F : -.25F,0})));
        const auto muzzle_obstruction=combat_world.trace_segment(head_position,gun_muzzle);
        if (muzzle_obstruction.hit()) gun_muzzle=Vector3Lerp(head_position,gun_muzzle,
            std::max(0.0F,muzzle_obstruction.fraction-.05F));
        const Vector3 gun_direction=Vector3Normalize(Vector3Subtract(aim_point,gun_muzzle));
        if (!controls.fire) trigger_ready=true;
        game::combat::WeaponInput weapon_input;
        weapon_input.fire=controls.fire && trigger_ready;
        weapon_input.aim=controls.aim;
        weapon_input.reload=controls.reload;
        weapon_input.next_weapon=controls.next_weapon;
        weapon_input.sprint=controls.sprint && !controls.aim && movement_amount>.1F;
        weapon_input.grounded=player.grounded;
        weapon_input.movement=movement_amount;
        weapons.update(dt,weapon_input,{gun_muzzle,gun_direction},combat_world);
        recoil_pitch=std::min(.20F,recoil_pitch+weapons.events().pitch_kick);
        recoil_yaw+=weapons.events().yaw_kick;
        environment_audio.play_combat(weapons.selected(),weapons.events());

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
            camera.position, verda_region
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
        verda_region.draw(camera.position);

#ifdef OUTLAND_DEV_TOOLS
        draw_model_backed_world_assets(
            verda_region,
            camera.position
        );
#endif

#ifdef OUTLAND_DEV_TOOLS
        if (
            game_mode ==
            game::GameMode::DevLab
        ) {
            creator_controller.draw_world_overlay(
                verda_region
            );

            draw_creator_model_preview(
                creator_controller
            );
        }
#endif

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

        game::combat::CombatRenderer::draw_world(weapons,combat_world);
        game::combat::CombatRenderer::draw_gun(gun_muzzle,gun_direction,weapons.selected(),
            weapons.reload_remaining()/weapons.weapon().reload_seconds,weapons.muzzle_flash());

        // ----------------------------------------------------
        // PLAYER BODY
        // ----------------------------------------------------

        if (player.third_person) {

            const Vector3 character_feet{player.position.x,player.position.y-1.0F,player.position.z};

            player::VerdanCharacter::draw(
                character_feet,
                player.yaw,
                movement_amount,
                character_animation_time,
                true
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

        const int center_x=screen_width/2, center_y=screen_height/2;
        const int gap=static_cast<int>((controls.aim ? 3 : 8)+movement_amount*5+recoil_pitch*180);
        const Color crosshair_color=weapons.hit_marker()>0 ?
            (weapons.last_headshot() ? ORANGE : GREEN) : RAYWHITE;
        DrawLine(center_x-gap-6,center_y,center_x-gap,center_y,crosshair_color);
        DrawLine(center_x+gap,center_y,center_x+gap+6,center_y,crosshair_color);
        DrawLine(center_x,center_y-gap-6,center_x,center_y-gap,crosshair_color);
        DrawLine(center_x,center_y+gap,center_x,center_y+gap+6,crosshair_color);
        if (weapons.hit_marker()>0) {
            DrawLine(center_x-10,center_y-10,center_x-5,center_y-5,crosshair_color);
            DrawLine(center_x+5,center_y+5,center_x+10,center_y+10,crosshair_color);
            DrawLine(center_x-10,center_y+10,center_x-5,center_y+5,crosshair_color);
            DrawLine(center_x+5,center_y-5,center_x+10,center_y-10,crosshair_color);
            DrawText(TextFormat("%s %.0f",weapons.last_headshot() ? "HEAD HIT" : "HIT",weapons.last_damage()),
                center_x-40,center_y+35,16,crosshair_color);
        }
        DrawRectangle(12,screen_height-103,320,91,Fade(BLACK,.65F));
        DrawText(weapons.weapon().name,24,screen_height-94,19,RAYWHITE);
        if (weapons.unlimited()) DrawText("AMMO UNLIMITED - DEV LAB",24,screen_height-68,18,YELLOW);
        else DrawText(TextFormat("%d / %d",weapons.ammo().loaded,weapons.ammo().reserve),
            24,screen_height-68,22,RAYWHITE);
        if (weapons.reload_remaining()>0) DrawText(TextFormat("RELOADING %.1fs",weapons.reload_remaining()),
            24,screen_height-39,16,ORANGE);
        else DrawText(weapons.ammo().loaded==0 && !weapons.unlimited() ?
            "EMPTY - R / LOAD TO RELOAD" : "F / FIRE   Q / AIM   R / LOAD   TAB / GUN",
            24,screen_height-39,12,RAYWHITE);

        touch_hud.draw(
            controls,
            input_system.layout(),
            screen_width,
            screen_height
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


#ifdef OUTLAND_DEV_TOOLS
        if (
            game_mode ==
            game::GameMode::DevLab
        ) {
            creator_controller.draw_hud();

            creator_touch_ui.draw(
                creator_controller,
                screen_width,
                screen_height
            );
        }
#endif

        DrawRectangleRec(audio_button, Fade(BLACK, 0.60F));
        DrawText(environment_audio.ready() ?
                 (environment_audio.muted() ? "AUDIO OFF · M" : "AUDIO ON · M") : "AUDIO UNAVAILABLE",
                 screen_width - 165, 53, 14, RAYWHITE);

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
