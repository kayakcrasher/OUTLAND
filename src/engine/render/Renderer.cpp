#include "outland/engine/render/Renderer.hpp"
#include "outland/engine/audio/EnvironmentAudio.hpp"
#include "outland/game/combat/CombatRenderer.hpp"
#include "outland/game/inventory/LootSession.hpp"
#include "outland/game/inventory/LootUI.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/game/vehicles/VehicleRenderer.hpp"
#include "outland/game/vehicles/VehicleAudio.hpp"
#include "outland/assets/ModelCache.hpp"

#include "outland/input/InputSystem.hpp"
#include <unordered_set>
#include "outland/input/TouchHUD.hpp"
#include "outland/game/GameMode.hpp"
#include "outland/game/HomeScreen.hpp"
#include "outland/dev/DevLab.hpp"
#include "outland/creator/CreatorMapIO.hpp"

#ifdef OUTLAND_DEV_TOOLS
#include "outland/creator/CreatorController.hpp"
#include "outland/creator/CreatorTouchUI.hpp"
#include "outland/creator/CreatorSession.hpp"
#endif
#include "outland/characters/CharacterRenderer.hpp"
#include "outland/game/combat/Health.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainWorld.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/foliage/FoliageSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <unordered_map>

namespace outland::engine {

namespace {

assets::ModelCache& world_asset_model_cache() {
    static assets::ModelCache cache;
    return cache;
}

Model* get_world_asset_model(const std::string& path, const characters::CharacterRegistry& registry) {
    const auto* character = registry.find_model(path);
    return world_asset_model_cache().load(path, [character](Model& model) {
        return !character || character->pool == characters::CharacterPool::Arms ||
            characters::CharacterRenderer::prepare(*character, model);
    });
}

void draw_model_backed_world_assets(
    const world::VerdaRegion& region,
    const Vector3& camera_position,
    const characters::CharacterRegistry& registry
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
            if (!asset.vehicle.definition.empty() || asset.model_path.empty()) {
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
                    asset.model_path, registry
                );

            if (model == nullptr) {
                continue;
            }

            DrawModelEx(
                *model,
                asset.position,
                {0.0F, 1.0F, 0.0F},
                asset.rotation_y + (registry.find_model(asset.model_path) ? registry.find_model(asset.model_path)->facing_degrees : 0.0F),
                {1.0F, 1.0F, 1.0F},
                WHITE
            );
        }
    }
}

#ifdef OUTLAND_DEV_TOOLS
void draw_creator_model_preview(
    const creator::CreatorController& controller,
    const characters::CharacterRegistry& registry
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
        get_world_asset_model(asset->model_path, registry);

    if (model == nullptr) {
        return;
    }

    const Color ghost_color =
        Fade(preview.blocked?RED:GREEN, 0.58F);

    DrawModelEx(
        *model,
        preview.position,
        {0.0F, 1.0F, 0.0F},
        preview.rotation_y + (registry.find_model(asset->model_path) ? registry.find_model(asset->model_path)->facing_degrees : 0.0F),
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

    // Renderer handles Escape as return-to-home; raylib must not close first.
    SetExitKey(KEY_NULL);
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
    bool inventory_open = false;
    std::string interaction_message;
    float interaction_remaining = 0.0F;

    game::HomeScreen home_screen;
    dev::DevLab dev_lab;

#ifdef OUTLAND_DEV_TOOLS
    creator::CreatorController creator_controller;
    creator::CreatorTouchUI creator_touch_ui;
#endif

    game::GameMode game_mode =
        game::GameMode::Home;

    const auto environment_path=[](const char* name){const auto* value=std::getenv(name);return value ? std::string(value):std::string{};};
    const std::string map_path=creator::CreatorMapIO::writable_path(GetApplicationDirectory(),environment_path("HOME"),environment_path("OUTLAND_SAVE_DIR"));
    const std::string initial_map=creator::CreatorMapIO::startup_map(GetApplicationDirectory(),environment_path("HOME"),environment_path("OUTLAND_SAVE_DIR"));
    world::VerdaRegion verda_region(!FileExists(initial_map.c_str()));
    if(FileExists(initial_map.c_str())) {
        const bool loaded=creator::CreatorMapIO::load(verda_region,initial_map);
        TraceLog(loaded ? LOG_INFO:LOG_ERROR,loaded ? "Saved Verda map loaded":"Map load failed - using training region");
    } else {
        const std::string defaults="maps/verda_loot_defaults.map";
        const std::string path=FileExists(defaults.c_str())?defaults:std::string(GetApplicationDirectory())+defaults;
        if(FileExists(path.c_str()) && !creator::CreatorMapIO::load(verda_region,path))
            TraceLog(LOG_ERROR,"Default loot markers could not be loaded");
    }
#ifdef OUTLAND_DEV_TOOLS
    creator::CreatorSession creator_session(map_path);
#endif
    const std::string character_manifest = "assets/verda/characters/character_manifest.tsv";
    const std::string character_root = FileExists((std::string(GetApplicationDirectory()) + character_manifest).c_str())
        ? GetApplicationDirectory() : ".";
    characters::CharacterRegistry character_registry;
    std::string character_error;
    if (!character_registry.load(character_root + "/" + character_manifest, character_error)) {
        TraceLog(LOG_ERROR, "%s", character_error.c_str());
        return;
    }
    characters::CharacterRenderer character_renderer(character_registry, character_root);
    game::inventory::ItemRegistry item_registry;
    std::string item_error;
    const std::string item_root=character_root+"/assets/verda/survival/gameplay/";
    if(!item_registry.load(item_root+"items.tsv",item_root+"loot_tables.tsv",item_error)){
        TraceLog(LOG_ERROR,"Item registry: %s",item_error.c_str());return;
    }
    game::inventory::LootSession loot_session(item_registry);
    game::inventory::LootUI loot_ui(character_root);
    characters::NpcSystem npcs;
    game::vehicles::VehicleRegistry vehicle_registry;
    std::string vehicle_error;
    if(!vehicle_registry.load((std::string(character_root)+"/assets/verda/vehicles/vehicle_manifest.tsv"),vehicle_error))
        TraceLog(LOG_ERROR,"Vehicle catalog: %s",vehicle_error.c_str());
    game::vehicles::VehicleSystem vehicles(vehicle_registry);
    game::vehicles::VehicleRenderer vehicle_renderer;
    vehicles.reconcile(verda_region);
    const std::string vehicle_state_path=map_path+".vehicles";
    bool vehicle_save_protected=false,vehicle_dirty=false;
    float vehicle_save_timer=0,vehicle_reconcile_timer=0,vehicle_orbit=0,vehicle_pitch=0;
    if(FileExists(vehicle_state_path.c_str()) && !vehicles.load_state(verda_region,vehicle_state_path,vehicle_error)) {
        vehicle_save_protected=true;TraceLog(LOG_ERROR,"Vehicle state protected: %s",vehicle_error.c_str());
    }
#ifdef OUTLAND_DEV_TOOLS
    bool has_previous_location=false;
    Vector3 previous_location{};
#endif
    world::terrain::TerrainWorld terrain_world;
    world::foliage::FoliageSystem foliage_system;
    audio::EnvironmentAudio environment_audio(
        std::string(GetApplicationDirectory()) + "assets/audio/environment"
    );

    game::combat::WeaponSystem weapons;
    game::combat::CombatWorld combat_world(verda_region);
    game::Health player_health;
    game::vehicles::VehicleAudio vehicle_audio;
    vehicles.bind_occupant_damage([&](float amount){player_health.damage(amount);});
    combat_world.bind_vehicles([&](Vector3 start,Vector3 end,bool occupants){return vehicles.trace(verda_region,start,end,occupants);},[&](const game::combat::BulletHit& hit,float amount){vehicles.damage(verda_region,hit,amount);vehicle_dirty=true;});
    combat_world.bind_actors([&](Vector3 start,Vector3 end) {
        const auto hit=npcs.trace_segment(start,end);
        return game::combat::BulletHit{hit.actor>=0 ? game::combat::HitKind::Npc : game::combat::HitKind::None,
            hit.fraction,hit.position,hit.normal,hit.actor,hit.headshot};
    },[&](int actor,float damage,Vector3 attacker) {
        if(actor>=0) npcs.damage(static_cast<std::size_t>(actor),damage,attacker);
    });
    float recoil_pitch = 0.0F, recoil_yaw = 0.0F;
    bool trigger_ready = false;

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
            if (IsKeyPressed(KEY_ESCAPE)) break;
            input_system.update(screen_width, screen_height, true, true);
#ifdef OUTLAND_DEV_TOOLS
            creator_controller.set_enabled(false);
            creator_touch_ui.update(creator_controller, screen_width, screen_height, true);
#endif
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

                weapons.reset(game_mode == game::GameMode::DevLab);
                if(game_mode!=game::GameMode::DevLab)loot_session.start(game_mode,map_path,weapons);
                combat_world.reset_targets();
                npcs.reset_session();
                player_health.reset();
                recoil_pitch = recoil_yaw = 0.0F;
                trigger_ready = false;
                inventory_open = false;
                interaction_remaining = 0;
#ifdef OUTLAND_DEV_TOOLS
                if(game_mode==game::GameMode::DevLab) {
                    dev_lab.begin_builder();
                    creator_controller.state().flying=true;creator_controller.state().noclip=true;
                    creator_touch_ui.show_all_assets();creator_touch_ui.set_inventory_open(true);
                    creator_controller.clear_selection();
                }
#endif
                input_system.update(screen_width, screen_height, true, true);
            }

            continue;
        }

        // ====================================================
        // RETURN HOME
        // ====================================================

        if (
            IsKeyPressed(KEY_ESCAPE)
        ) {
#ifdef OUTLAND_DEV_TOOLS
            if(creator_session.dirty() && !creator_session.save(verda_region))continue;
#endif
            if(game_mode!=game::GameMode::DevLab && !loot_session.save(weapons) && loot_session.can_persist()){
                interaction_message=loot_session.status();interaction_remaining=3;continue;
            }
            vehicles.leave_session();vehicle_audio.stop();vehicle_renderer.reset_camera();
            game_mode =
                game::GameMode::Home;
            input_system.cancel_controls();
            // Finish this frame so the Home screen does not reuse the same Escape edge.
            BeginDrawing();
            home_screen.draw(screen_width, screen_height);
            EndDrawing();

            continue;
        }

        // ====================================================
        // INPUT
        // ====================================================

        vehicles.begin_frame();
        bool creator_active = false;
        std::function<bool(Vector2)> reserved = [&](Vector2 point) {
            return CheckCollisionPointRec(point, audio_button);
        };
#ifdef OUTLAND_DEV_TOOLS
        bool dev_modal=false;
        if(game_mode==game::GameMode::DevLab) {
            const bool tools_were_open=dev_lab.tools_open();
            dev_lab.update(screen_width,screen_height,!dev_lab.building());
            dev_modal=tools_were_open || dev_lab.tools_open();
            if(dev_modal)input_system.cancel_controls();
            if(dev_lab.take_build_toggle()) {
                Vector3 exit{};
                if(!vehicles.driver() || vehicles.exit(verda_region,exit,true)) {
                    if(exit.x!=0||exit.y!=0||exit.z!=0)player.position=Vector3Add(exit,{0,1,0});
                    dev_lab.toggle_build();creator_touch_ui.set_inventory_open(false);
                    dev_modal=true;input_system.cancel_controls();vehicle_renderer.reset_camera();
                }
            }
            if(dev_lab.take_vehicle_spawn() && vehicles.spawn(verda_region,{player.position.x,player.position.y-1,player.position.z},player.yaw*RAD2DEG)){vehicle_dirty=true;creator_session.runtime_changed();}
            if(dev_lab.take_return()&&has_previous_location){vehicles.leave_session();std::swap(player.position,previous_location);player.vertical_velocity=0;player.grounded=true;vehicle_renderer.reset_camera();}
        }
        creator_active = game_mode == game::GameMode::DevLab && dev_lab.building();
        creator_controller.set_enabled(creator_active);
        creator_touch_ui.update(creator_controller, screen_width, screen_height, dev_modal || !IsWindowFocused());
        if (creator_active) {
            reserved = [&](Vector2 point) {
                return CheckCollisionPointRec(point, audio_button) || dev_lab.owns_point(point,screen_width,screen_height) ||
                    creator_touch_ui.owns_point(point, screen_width, screen_height);
            };
        }
#endif
#ifdef OUTLAND_DEV_TOOLS
        if(game_mode==game::GameMode::DevLab && !creator_active)reserved=[&](Vector2 point){return CheckCollisionPointRec(point,audio_button)||dev_lab.owns_point(point,screen_width,screen_height);};
#endif
        // The equipment panel is modal: BAG closes it and GUN changes equipment.
        if (inventory_open) {
            // The bag owns every touch. Keyboard I/Tab still pass through, then
            // gameplay movement/fire is cleared below.
            input_system.update(screen_width,screen_height,true,!IsWindowFocused(),[](Vector2){return true;});
            const bool switch_weapon = input_system.player().next_weapon;
            if (input_system.player().inventory) {
                inventory_open = false;
                input_system.cancel_controls();
            }
            input_system.player() = {};
            input_system.player().next_weapon = switch_weapon;
        } else {
            input_system.update(screen_width, screen_height, !creator_active,
#ifdef OUTLAND_DEV_TOOLS
                !IsWindowFocused() || dev_modal || (creator_active && creator_touch_ui.inventory_open()),
#else
                !IsWindowFocused(),
#endif
                reserved);
            if (input_system.player().inventory && game_mode != game::GameMode::DevLab) {
                inventory_open = true;
                loot_ui.opened();
                input_system.cancel_controls();
            }
        }
#ifdef OUTLAND_DEV_TOOLS
        if(game_mode==game::GameMode::DevLab && dev_lab.tools_open())input_system.cancel_controls();
#endif
        if(!player_health.alive() && !creator_active) input_system.player()={};
        const input::PlayerInput& controls = input_system.player();
        vehicle_reconcile_timer+=dt;
        if(vehicle_reconcile_timer>=.5F){vehicle_dirty|=vehicles.reconcile(verda_region);vehicle_reconcile_timer=0;}
        if(controls.interact && !creator_active && !inventory_open) {
            Vector3 exit{};
            if(vehicles.driver()){
                if(vehicles.exit(verda_region,exit)){player.position=Vector3Add(exit,{0,1,0});player.vertical_velocity=0;vehicle_renderer.reset_camera();interaction_message="Exited vehicle";}
                else interaction_message="Stop and leave space beside the vehicle to exit";
            }else {const int vehicle=vehicles.nearest(verda_region,{player.position.x,player.position.y-1,player.position.z});if(vehicle>=0 && vehicles.enter(verda_region,vehicle,{player.position.x,player.position.y-1,player.position.z})){vehicle_orbit=vehicle_pitch=0;input_system.cancel_controls();interaction_message="Driving - move to accelerate/steer; JUMP brakes; USE exits";}}
            interaction_remaining=3;
        }
        const bool driving=vehicles.driver()!=nullptr;
        game::vehicles::VehicleInput driving_input{-controls.move_y,-controls.move_x,controls.brake,controls.crouch,controls.sprint};
        const bool vehicle_paused=creator_active||inventory_open||!IsWindowFocused()
#ifdef OUTLAND_DEV_TOOLS
            || (game_mode==game::GameMode::DevLab && dev_lab.tools_open())
#endif
            ;
        vehicle_dirty|=vehicles.update(dt,driving_input,verda_region,{player.position.x,player.position.y-1,player.position.z},vehicle_paused);
        if(driving){vehicle_orbit-=controls.look_x*80*dt;vehicle_pitch=std::clamp(vehicle_pitch-controls.look_y*dt,-1.0F,1.0F);}

        interaction_remaining = std::max(0.0F, interaction_remaining - dt);
        const Vector3 loot_feet{player.position.x,player.position.y-1,player.position.z};
        const auto reachable_pickup=[&](Vector3 point){return !combat_world.trace_segment(
            {player.position.x,player.position.y+.5F,player.position.z},Vector3Add(point,{0,.1F,0}),false,false).hit();};
        if(!creator_active && game_mode != game::GameMode::DevLab) {
            loot_session.update(dt,verda_region,loot_feet,weapons);
            if(controls.next_weapon)loot_session.changed();
            if(controls.interact && !vehicles.driver() && vehicles.nearest(verda_region,loot_feet)<0){interaction_message=loot_session.interact(loot_feet,weapons,reachable_pickup);interaction_remaining=2.5F;}
            if(inventory_open){
                const auto action=loot_ui.update(loot_session.inventory(),screen_width,screen_height,IsWindowFocused());
                if(action.close){inventory_open=false;input_system.cancel_controls();}
                else if(player_health.alive() && action.use){interaction_message=loot_session.use(action.selected,weapons,player_health);interaction_remaining=3;}
                else if(player_health.alive() && (action.drop_one || action.drop_stack)){
                    const Vector3 destination{loot_feet.x+std::sin(player.yaw)*1.2F,loot_feet.y,loot_feet.z-std::cos(player.yaw)*1.2F};
                    interaction_message=loot_session.drop(action.selected,action.drop_stack,destination,weapons,reachable_pickup);interaction_remaining=3;
                }
            }
        }

        // ====================================================
        // DEV LAB
        // ====================================================

        if (
            game_mode ==
            game::GameMode::DevLab
        ) {


            if (
                dev_lab.teleport_requested()
            ) {
#ifdef OUTLAND_DEV_TOOLS
                previous_location=player.position;has_previous_location=true;
#endif
                vehicles.leave_session();vehicle_renderer.reset_camera();
                player.position =
                    dev_lab.spawn_position(verda_region);

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

        bool creator_flying=false;
#ifdef OUTLAND_DEV_TOOLS
        creator_flying=creator_active && creator_controller.state().flying;
#endif
        const Vector3 position_before_move=player.position;
        const float speed = creator_flying ? 12.0F:controls.crouch ? walk_speed * 0.45F : controls.aim ? walk_speed * 0.55F :
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
            0.01F && !driving
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

            player.position = creator_flying ? desired_position :
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

        bool vaulted = false;
        if (!driving && !creator_flying && controls.jump && player.grounded) {
            Vector3 vault_landing{};

            const Vector3 vault_forward{
                forward.x,
                0.0F,
                forward.z
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
                vaulted = true;
            }
        }

        // ====================================================
        // TERRAIN FOLLOW
        // ====================================================

        if (!creator_flying && player.grounded) {
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

        // ====================================================
        // JUMP + GRAVITY
        // ====================================================

        if (
            !creator_flying && controls.jump && !vaulted &&
            player.grounded
        ) {
            player.vertical_velocity =
                jump_speed;

            player.grounded =
                false;
        }

        if (!creator_flying && !player.grounded) {

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

#ifdef OUTLAND_DEV_TOOLS
        if(creator_flying) {
            if(IsWindowFocused() && !creator_touch_ui.inventory_open())player.position.y+=creator_touch_ui.actions().fly_vertical*speed*dt;
            player.vertical_velocity=0;player.grounded=false;player.third_person=false;
        }
#endif
        if(vehicles.driver()){player.position=Vector3Add(vehicles.seat(verda_region),{0,1,0});player.vertical_velocity=0;player.grounded=false;}
        // ====================================================
        // CAMERA POSITION
        // ====================================================

        environment_audio.update(player.position, player.grounded && !driving, !driving, verda_region);
        vehicle_audio.update(vehicles,verda_region,dt,vehicle_paused || environment_audio.muted(),environment_audio.volume());

        recoil_pitch *= std::exp(-dt * 5.0F);
        recoil_yaw *= std::exp(-dt * 7.0F);
        const float shot_pitch = std::clamp(player.pitch + recoil_pitch, -1.2F, 1.15F);
        const float shot_yaw = player.yaw + recoil_yaw;
        const Vector3 look_direction{std::sin(shot_yaw) * std::cos(shot_pitch),
            std::sin(shot_pitch), std::cos(shot_yaw) * std::cos(shot_pitch)};
        const Vector3 gun_right{std::cos(shot_yaw), 0.0F, -std::sin(shot_yaw)};

        const Vector3 head_position{
            player.position.x,
            player.position.y + (controls.crouch ? 0.15F : 0.75F),
            player.position.z
        };

        const float desired_fov = controls.aim ? 50.0F : 70.0F;
        camera.fovy += (desired_fov-camera.fovy) * (1.0F-std::exp(-dt*12.0F));
        if (player.third_person) {
            const float distance = controls.aim ? 2.5F : 5.0F;
            camera.position = Vector3Add(Vector3Subtract(head_position, Vector3Scale(look_direction,distance)),
                Vector3Add(Vector3Scale(gun_right,controls.aim ? .4F : .75F), {0,controls.aim ? .25F : .8F,0}));
            const auto obstruction = combat_world.trace_segment(head_position,camera.position,false,false);
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
        if(vehicles.driver())vehicle_renderer.camera(camera,vehicles,verda_region,dt,vehicle_orbit,vehicle_pitch,combat_world);
        const Vector3 camera_direction=Vector3Normalize(Vector3Subtract(camera.target,camera.position));

#ifdef OUTLAND_DEV_TOOLS
        {
            if (creator_active) {
                creator_controller.update(
                    verda_region,
                    camera.position,
                    camera_direction,
                    !creator_touch_ui.inventory_open() && !dev_modal && IsWindowFocused()
                );

                const auto& action=creator_touch_ui.actions();
                if(action.select)creator_controller.select_target(verda_region,camera.position,camera_direction);
                if(action.place && creator_session.edit(verda_region,[&]{return creator_controller.place_selected(verda_region);}))creator_controller.clear_selection();
                if(action.move)creator_session.edit(verda_region,[&]{return creator_controller.move_selected(verda_region);});
                if(action.duplicate)creator_session.edit(verda_region,[&]{return creator_controller.duplicate_selected(verda_region);});
                if(action.erase)creator_session.edit(verda_region,[&]{return creator_controller.delete_selected(verda_region);});
                if(action.rotate) {
                    if(creator_controller.selection().valid())creator_session.edit(verda_region,[&]{return creator_controller.rotate_selected(verda_region,15);});
                    else creator_controller.rotate_preview(15);
                }
                if(action.undo){creator_session.undo(verda_region);creator_controller.clear_selection();}
                if(action.redo){creator_session.redo(verda_region);creator_controller.clear_selection();}
                if(action.save)creator_session.save(verda_region);
                if(action.export_world) {
                    if(creator_session.export_world(verda_region))TraceLog(LOG_INFO,"World exported to %s",creator_session.export_path().c_str());
                }
                if(action.load){creator_session.load(verda_region);creator_controller.clear_selection();}
                vehicle_dirty|=vehicles.reconcile(verda_region);
                creator_touch_ui.set_status(creator_session.status());

            }
        }
#endif
#ifdef OUTLAND_DEV_TOOLS
        if(game_mode==game::GameMode::DevLab && vehicle_dirty)creator_session.runtime_changed();
        creator_session.update(dt,verda_region);
#endif
        vehicle_save_timer+=dt;
        if(vehicle_dirty && !vehicle_save_protected && vehicle_save_timer>=2){
            if(vehicles.save_state(verda_region,vehicle_state_path,vehicle_error))vehicle_dirty=false;
            else TraceLog(LOG_ERROR,"Vehicle state save: %s",vehicle_error.c_str());
            vehicle_save_timer=0;
        }
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
        weapon_input.fire=controls.fire && trigger_ready && !driving && !creator_active && !inventory_open;
        weapon_input.aim=controls.aim;
        weapon_input.reload=controls.reload;
        weapon_input.next_weapon=controls.next_weapon;
        weapon_input.sprint=controls.sprint && !controls.aim && movement_amount>.1F;
        weapon_input.grounded=player.grounded;
        weapon_input.movement=movement_amount;
        npcs.reconcile(verda_region, character_registry);
        weapons.update(dt,weapon_input,{gun_muzzle,gun_direction},combat_world);
        recoil_pitch=std::min(.20F,recoil_pitch+weapons.events().pitch_kick);
        recoil_yaw+=weapons.events().yaw_kick;
        environment_audio.play_combat(weapons.selected(),weapons.events());
        characters::NpcContext npc_context;
        npc_context.player_position={player.position.x,player.position.y-1.0F,player.position.z};
        npc_context.player_alive=player_health.alive();
        npc_context.threatening=weapons.events().shots>0;
        npc_context.paused=vehicle_paused;
        npc_context.visible=[&](Vector3 start,Vector3 end) {return !combat_world.trace_segment(start,end,false,false).hit();};
        npcs.update(dt,npc_context,verda_region);
        player_health.damage(npcs.events().player_damage);
        character_renderer.update_player(!player_health.alive() ? characters::AnimationAction::Death :
            weapons.events().shots>0 ? characters::AnimationAction::Attack :
            Vector3DistanceSqr(position_before_move,player.position)>.000001F && player.grounded ? (controls.sprint ? characters::AnimationAction::Run : characters::AnimationAction::Walk) :
            characters::AnimationAction::Idle,dt);

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

        terrain_world.update(camera.position);
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

        draw_model_backed_world_assets(verda_region, camera.position, character_registry);
        vehicle_renderer.draw(vehicles,verda_region,camera.position);

#ifdef OUTLAND_DEV_TOOLS
        if (
            game_mode ==
            game::GameMode::DevLab
        ) {
            creator_controller.draw_world_overlay(
                verda_region
            );

            draw_creator_model_preview(
                creator_controller, character_registry
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
        if(weapons.available() && !driving && !creator_active)game::combat::CombatRenderer::draw_gun(gun_muzzle,gun_direction,weapons.selected(),
            weapons.reload_remaining()/weapons.weapon().reload_seconds,weapons.muzzle_flash());
        if(!creator_active)loot_ui.draw_world(item_registry,loot_session.world(),loot_feet,loot_session.inventory().state().light);

        character_renderer.draw_npcs(npcs, camera.position);

        // ----------------------------------------------------
        // PLAYER BODY
        // ----------------------------------------------------

        if (player.third_person && !driving) {

            const Vector3 character_feet{player.position.x,player.position.y-1.0F,player.position.z};

            character_renderer.draw_player(character_feet, player.yaw);
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
        if (!creator_active && !inventory_open) {
            DrawRectangle(12,screen_height-103,320,91,Fade(BLACK,.65F));
            DrawText(weapons.available()?weapons.weapon().name:"NO WEAPON - OPEN BAG",24,screen_height-94,19,RAYWHITE);
            if (weapons.unlimited()) DrawText("AMMO UNLIMITED - DEV LAB",24,screen_height-68,18,YELLOW);
            else DrawText(TextFormat("%d / %d",weapons.ammo().loaded,weapons.ammo().reserve),
                24,screen_height-68,22,RAYWHITE);
            if (weapons.reload_remaining()>0) DrawText(TextFormat("RELOADING %.1fs",weapons.reload_remaining()),
                24,screen_height-39,16,ORANGE);
            else DrawText(weapons.ammo().loaded==0 && !weapons.unlimited() ?
                "EMPTY - R / LOAD TO RELOAD" : "F / FIRE   Q / AIM   R / LOAD   TAB / GUN",
                24,screen_height-39,12,RAYWHITE);
        }

        if (!creator_active) {
            DrawText(TextFormat("HEALTH %.0f / %.0f",player_health.current(),player_health.maximum()),
                16,16,20,player_health.alive() ? RAYWHITE : RED);
            if (!player_health.alive()) DrawText("YOU DIED - ESC / BACK TO HOME",screen_width/2-175,screen_height/2,22,RED);
        }
        if (!inventory_open) touch_hud.draw(
            controls,
            input_system.layout(),
            screen_width,
            screen_height,
            !creator_active
        );

        if (
            game_mode ==
            game::GameMode::DevLab && !creator_active
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
            // Builder toolbar reports the selected asset and operation status.

            creator_touch_ui.draw(
                creator_controller,
                screen_width,
                screen_height
            );
        }
#endif

        if(inventory_open)loot_ui.draw_inventory(item_registry,loot_session,weapons,screen_width,screen_height);
        if(!creator_active && !inventory_open){
            const int nearby=loot_session.world().nearest(loot_feet,2.8F,reachable_pickup);
            if(nearby>=0){const auto& pickup=loot_session.world().state().pickups[static_cast<std::size_t>(nearby)];
                const auto* item=item_registry.find(pickup.item);
                if(item)DrawText(TextFormat("E / USE: %s x%d",item->name.c_str(),pickup.quantity),screen_width/2-180,screen_height-140,18,YELLOW);
            }
        }
        if (interaction_remaining > 0)
            DrawText(interaction_message.c_str(), screen_width / 2 - 170, 100, 18, RAYWHITE);

        DrawRectangleRec(audio_button, Fade(BLACK, 0.60F));
        DrawText(environment_audio.ready() ?
                 (environment_audio.muted() ? "AUDIO OFF · M" : "AUDIO ON · M") : "AUDIO UNAVAILABLE",
                 screen_width - 165, 53, 14, RAYWHITE);

#ifdef OUTLAND_DEV_TOOLS
        if(game_mode==game::GameMode::DevLab)dev_lab.draw_tools(screen_width,screen_height,creator_active);
#endif
        if(const auto* driver=vehicles.driver())DrawText(TextFormat("%.0f km/h | JUMP: brake | CROUCH: park | SPRINT: horn | USE: exit",std::abs(driver->speed)*3.6F),20,screen_height-32,16,YELLOW);
        else if(!creator_active && !inventory_open && vehicles.nearest(verda_region,{player.position.x,player.position.y-1,player.position.z})>=0)DrawText("USE / E - Enter vehicle",screen_width/2-120,screen_height-100,20,YELLOW);
        DrawFPS(
            screen_width - 90,
            10
        );

        EndDrawing();
    }
#ifdef OUTLAND_DEV_TOOLS
    if(creator_session.dirty() && !creator_session.save(verda_region))TraceLog(LOG_ERROR,"Unsaved Creator edits: %s",creator_session.path().c_str());
#endif
    if(vehicle_dirty && !vehicle_save_protected && !vehicles.save_state(verda_region,vehicle_state_path,vehicle_error))TraceLog(LOG_ERROR,"Vehicle save failed: %s",vehicle_error.c_str());
    if(game_mode!=game::GameMode::Home && game_mode!=game::GameMode::DevLab && !loot_session.save(weapons))
        TraceLog(LOG_ERROR,"Inventory save failed: %s",loot_session.path().c_str());
}

// ============================================================
// SHUTDOWN
// ============================================================

void Renderer::shutdown() {
    if (!initialized_) {
        return;
    }

    world_asset_model_cache().clear();
    CloseWindow();

    initialized_ = false;
}

bool Renderer::initialized() const {
    return initialized_;
}

}
