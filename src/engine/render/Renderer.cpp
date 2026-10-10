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
#include "outland/input/PointerEvents.hpp"
#include <unordered_set>
#include "outland/input/TouchHUD.hpp"
#include "outland/game/GameMode.hpp"
#include "outland/game/HomeScreen.hpp"
#include "outland/game/ModeRules.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/sound/SoundBus.hpp"
#include "outland/world/navigation/NavGrid.hpp"
#include "outland/world/sky/DayNight.hpp"
#include "outland/game/law/Justice.hpp"
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
#include "outland/world/physics/MeshCollision.hpp"

#include "outland/game/culture/SignRenderer.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unordered_map>

namespace outland::engine {

namespace {

assets::ModelCache& world_asset_model_cache() {
    static assets::ModelCache cache;
    return cache;
}

// Window glass and painted fake-interior panels are knocked out of building models, so what
// players see matches MeshCollision: every window is an opening for movement, sight and bullets.
void knock_out_windows(const std::string& path, Model& model) {
    const auto* shape = world::physics::MeshCollisionLibrary::get(path);
    if (!shape || shape->knocked_out == 0) return;
    if (static_cast<int>(shape->mesh_knocked_out.size()) != model.meshCount) {
        TraceLog(LOG_WARNING, "OUTLAND window knock-out skipped (mesh order differs): %s", path.c_str());
        return;
    }
    // Partly knocked-out meshes: collapse those triangles to a point and re-upload the indices.
    for (int i = 0; i < model.meshCount; ++i) {
        const auto& gone = shape->mesh_knocked_triangles[static_cast<std::size_t>(i)];
        Mesh& mesh = model.meshes[i];
        if (gone.empty() || shape->mesh_knocked_out[static_cast<std::size_t>(i)]) continue;
        if (mesh.indices) {
            for (const auto t : gone) if (static_cast<int>(t) < mesh.triangleCount) mesh.indices[t * 3 + 1] = mesh.indices[t * 3 + 2] = mesh.indices[t * 3];
            rlUpdateVertexBufferElements(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES], mesh.indices, mesh.triangleCount * 3 * static_cast<int>(sizeof(unsigned short)), 0);
        } else if (mesh.vertices) {
            for (const auto t : gone) if (static_cast<int>(t) < mesh.triangleCount)
                for (int corner = 1; corner < 3; ++corner) for (int axis = 0; axis < 3; ++axis)
                    mesh.vertices[(t * 3 + corner) * 3 + axis] = mesh.vertices[t * 9 + axis];
            UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
        }
    }
    int kept = 0;
    for (int i = 0; i < model.meshCount; ++i) {
        if (shape->mesh_knocked_out[static_cast<std::size_t>(i)]) { UnloadMesh(model.meshes[i]); continue; }
        model.meshes[kept] = model.meshes[i];
        model.meshMaterial[kept] = model.meshMaterial[i];
        ++kept;
    }
    model.meshCount = kept;
}

Model* get_world_asset_model(const std::string& path, const characters::CharacterRegistry& registry);

#ifdef OUTLAND_DEV_TOOLS
// 3D previews for the Creator asset browser. Cards ask for a preview; missing ones are queued and
// rendered a couple per frame before drawing starts, so paging never stalls a phone.
class AssetThumbnails {
public:
    ~AssetThumbnails() { for (auto& [id, entry] : entries_) if (entry.texture.id) UnloadRenderTexture(entry.texture); }
    const Texture2D* get(const creator::CreatorAssetDefinition& asset) {
        if (asset.model_path.empty()) return nullptr;
        auto found = entries_.find(asset.id);
        if (found == entries_.end()) {
            if (std::find_if(queue_.begin(), queue_.end(), [&](const auto& q) { return q.first == asset.id; }) == queue_.end())
                queue_.emplace_back(asset.id, asset.model_path);
            return nullptr;
        }
        found->second.used = ++clock_;
        return found->second.texture.id ? &found->second.texture.texture : nullptr;
    }
    void render_pending(const characters::CharacterRegistry& registry, int budget = 2) {
        while (budget-- > 0 && !queue_.empty()) {
            const auto [id, path] = queue_.front();
            queue_.erase(queue_.begin());
            Entry entry{{}, ++clock_};
            if (Model* model = get_world_asset_model(path, registry)) {
                const auto bounds = assets::transformed_model_bounds(*model);
                const Vector3 size = Vector3Subtract(bounds.max, bounds.min);
                const float radius = std::max(.2F, Vector3Length(size) * .5F);
                const Vector3 centre = Vector3Scale(Vector3Add(bounds.min, bounds.max), .5F);
                Camera3D camera{};
                camera.fovy = 30; camera.projection = CAMERA_PERSPECTIVE; camera.up = {0, 1, 0};
                camera.target = centre;
                camera.position = Vector3Add(centre, Vector3Scale(Vector3Normalize({.85F, .6F, -1.1F}), radius / std::sin(15 * DEG2RAD) * 1.05F));
                entry.texture = LoadRenderTexture(160, 120);
                BeginTextureMode(entry.texture);
                ClearBackground({58, 64, 62, 255});
                BeginMode3D(camera);
                DrawModel(*model, {0, 0, 0}, 1, WHITE);
                EndMode3D();
                EndTextureMode();
            }
            if (entries_.size() >= 96) {
                auto oldest = std::min_element(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
                if (oldest->second.texture.id) UnloadRenderTexture(oldest->second.texture);
                entries_.erase(oldest);
            }
            entries_.emplace(id, entry);
        }
    }
private:
    struct Entry { RenderTexture2D texture; std::uint64_t used; };
    std::unordered_map<std::string, Entry> entries_;
    std::vector<std::pair<std::string, std::string>> queue_;
    std::uint64_t clock_{0};
};
#endif

Model* get_world_asset_model(const std::string& path, const characters::CharacterRegistry& registry) {
    const auto* character = registry.find_model(path);
    return world_asset_model_cache().load(path, [character, &path](Model& model) {
        if (character) return character->pool == characters::CharacterPool::Arms ||
            characters::CharacterRenderer::prepare(*character, model);
        knock_out_windows(path, model);
        return model.meshCount > 0;
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

            // Small props fade out early (a phone cannot afford a draw call per distant bollard);
            // building pieces keep long range so a modular building never comes apart at a distance.
            const float extent = std::max({asset.size.x, asset.size.y, asset.size.z});
            const float reach = asset.type == world::AssetType::Wall || extent > 8.0F ? max_distance :
                std::clamp(55.0F + extent * 12.0F, 55.0F, max_distance);
            if (distance_sq > max_distance_sq || distance_sq > reach * reach) {
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

    input::PointerEvents pointer_events;
    TraceLog(LOG_INFO,pointer_events.installed() ? "Desktop click capture enabled":"Desktop click capture unavailable - using raylib input polling");
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
    AssetThumbnails asset_thumbnails;
    creator_touch_ui.set_thumbnails([&](const creator::CreatorAssetDefinition& asset) { return asset_thumbnails.get(asset); });
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
    game::ai::BattleRoyaleBots br_bots;
    // Time of day: Explore follows island life's clock, Zombie Survival runs its own from dusk,
    // Battle Royale keeps fair afternoon light, the Dev Lab sets the hour by hand (F6 / F7).
    float zombie_hour=17.5F, dev_hour=12.0F, last_daylight=1.0F;
    // Crime and the police (Explore): crimes queue up during the frame and are judged after it.
    game::law::Justice justice;
    struct QueuedCrime {game::law::CrimeKind kind;int victim;};
    std::vector<QueuedCrime> crimes;
    std::vector<game::ai::BotTracer> police_tracers;
    std::vector<world::sky::LightSource> scene_lights;
    std::size_t lit_asset_count=static_cast<std::size_t>(-1);
    game::sound::SoundBus sound_bus; // every noise on Verda this session, heard by bots, NPCs and residents
    double match_time=0;
    int match_placement=-1;
    float damage_flash=0;
    Vector3 damage_source{};
    Vector3 last_player_feet{};
    // Model assets resolve beside the executable first (packaged builds), then the working directory.
    world::physics::MeshCollisionLibrary::add_root(GetApplicationDirectory());
#ifdef OUTLAND_DEV_TOOLS
    // OUTLAND_COLLISION_AUDIT=1: load every catalog model through raylib and confirm the collision
    // mesh matches what is drawn (mesh order and bounds), then exit.
    if (const char* audit = std::getenv("OUTLAND_COLLISION_AUDIT"); audit && *audit == '1') {
        int checked = 0, mismatched = 0, knocked = 0;
        const creator::CreatorAssetRegistry catalog;
        for (const auto& asset : catalog.assets()) {
            const auto* shape = world::physics::MeshCollisionLibrary::get(asset.model_path);
            if (!shape) continue;
            const std::string packaged = std::string(GetApplicationDirectory()) + asset.model_path;
            Model model = LoadModel((FileExists(packaged.c_str()) ? packaged : asset.model_path).c_str());
            const auto bounds = GetModelBoundingBox(model);
            const bool same = model.meshCount == static_cast<int>(shape->mesh_knocked_out.size()) &&
                Vector3Distance(bounds.min, shape->source_min) < .01F && Vector3Distance(bounds.max, shape->source_max) < .01F;
            if (!same) {
                ++mismatched;
                TraceLog(LOG_WARNING, "COLLISION AUDIT mismatch %s: meshes %d/%zu bounds (%.2f %.2f %.2f)-(%.2f %.2f %.2f) vs (%.2f %.2f %.2f)-(%.2f %.2f %.2f)",
                    asset.model_path.c_str(), model.meshCount, shape->mesh_knocked_out.size(), bounds.min.x, bounds.min.y, bounds.min.z,
                    bounds.max.x, bounds.max.y, bounds.max.z, shape->source_min.x, shape->source_min.y, shape->source_min.z,
                    shape->source_max.x, shape->source_max.y, shape->source_max.z);
            }
            knocked += shape->knocked_out > 0;
            ++checked;
            UnloadModel(model);
        }
        TraceLog(LOG_INFO, "COLLISION AUDIT: %d models checked, %d mismatched, %d with windows knocked out", checked, mismatched, knocked);
        CloseWindow();
        return;
    }
#endif
    game::life::LifeSimulation island_life;
    // Esperanto signage and the flag. Rebuilt when a mode starts and after Creator edits.
    game::culture::SignRenderer sign_renderer;
    sign_renderer.load(GetApplicationDirectory());
    std::size_t signed_world=static_cast<std::size_t>(-1);
    const auto world_size=[&]{
        std::size_t n=0;
        for(const auto& settlement:verda_region.settlements()) n+=settlement.assets.size()+settlement.buildings.size()+settlement.gameplay_markers.size();
        return n;
    };
    const auto rebuild_signs=[&]{
        world::physics::MeshCollisionLibrary::preload(verda_region);
        sign_renderer.set(game::culture::build_signage(verda_region,island_life.island().places.empty() ?
            game::life::build_island(verda_region,&character_registry) : island_life.island()));
        signed_world=world_size();
    };
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
        if(actor<0) return;
        // Battle Royale bodies belong to their bot brains; everyone else is an NpcSystem actor.
        const int bot=static_cast<std::size_t>(actor)<npcs.actors().size() ? npcs.actors()[static_cast<std::size_t>(actor)].bot : -1;
        if(bot>=0) br_bots.damage_bot(bot,damage,attacker,match_time);
        else if(npcs.damage(static_cast<std::size_t>(actor),damage,attacker)) {
            // Hurting a resident is a crime if anyone saw it.
            const auto& victim=npcs.actors()[static_cast<std::size_t>(actor)];
            if(victim.resident>=0) crimes.push_back({victim.police ? game::law::CrimeKind::AttackOnPolice :
                victim.health<=0 ? game::law::CrimeKind::Murder : game::law::CrimeKind::Assault,victim.resident});
        }
    });
    // Battle Royale bots see, walk and shoot through the same world as the player.
    game::ai::BotWorld bot_world;
    bot_world.environment.line_of_sight=[&](Vector3 a,Vector3 b){return !combat_world.trace_segment(a,b,false,false,true).hit();};
    bot_world.environment.move=[&](Vector3 current,Vector3 desired) {
        const int steps=std::max(1,static_cast<int>(std::ceil(Vector3Distance(current,desired)/.2F)));
        const auto origin=current;
        for(int i=1;i<=steps;++i) current=world::physics::WorldCollision::resolve_body_movement(
            current,Vector3Lerp(origin,desired,static_cast<float>(i)/steps),current.y,verda_region,.35F);
        current.y=world::physics::WorldCollision::ground_height(current,current.y,verda_region);
        return current;
    };
    bot_world.trace=[&](Vector3 a,Vector3 b){return combat_world.trace_segment(a,b,false,false,true);};
    bot_world.sounds=&sound_bus;
    // Walkable-space graph shared by bots and NPCs. Exploring new ground is spread over frames
    // (cell budget per search) and at most two searches run per frame across every mover.
    world::navigation::NavGrid nav_grid(verda_region);
    nav_grid.set_max_new_cells(250);
    nav_grid.set_max_expansions(8000);
    int nav_searches_this_frame=0;
    bool nav_was_building=false;
    const world::navigation::PathFinder nav_finder=[&](Vector3 from,Vector3 to,world::navigation::NavPath& path) {
        if(nav_searches_this_frame>=2) return false;
        ++nav_searches_this_frame;
        return nav_grid.find_path(from,to,path);
    };
    bot_world.environment.find_path=nav_finder;
    npcs.set_path_finder(nav_finder);
    bot_world.world_damage=[&](const game::combat::BulletHit& hit,float amount,Vector3 from) {
        if(hit.kind==game::combat::HitKind::Vehicle || hit.kind==game::combat::HitKind::VehicleWindow ||
            hit.kind==game::combat::HitKind::VehicleOccupant) combat_world.damage_hit(hit,amount,from);
    };
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
        nav_searches_this_frame=0;
        pointer_events.begin_frame();

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

#ifdef OUTLAND_DEV_TOOLS
                // Dev builds: OUTLAND_DEV_SPAWN="x,z,yaw_degrees" starts every mode at that spot
                // (screenshots, checking a far corner of the island without a long walk).
                if(const char* spawn=std::getenv("OUTLAND_DEV_SPAWN")) {
                    float x=0,z=0,yaw_degrees=0;
                    if(std::sscanf(spawn,"%f,%f,%f",&x,&z,&yaw_degrees)==3 && std::isfinite(x) && std::isfinite(z) && std::isfinite(yaw_degrees)) {
                        const float ground=world::terrain::TerrainHeight::sample(x,z);
                        player.position={x,world::physics::WorldCollision::ground_height({x,ground,z},ground+.5F,verda_region)+1.0F,z};
                        player.yaw=yaw_degrees*DEG2RAD;
                    }
                }
#endif
                weapons.reset(game_mode == game::GameMode::DevLab);
                if(game_mode!=game::GameMode::DevLab)loot_session.start(game_mode,map_path,weapons);
                combat_world.reset_targets();
                combat_world.set_training_range(game_mode == game::GameMode::DevLab);
                vehicles.set_training_structure(game_mode == game::GameMode::DevLab);
                npcs.reset_session();
                world::physics::MeshCollisionLibrary::preload(verda_region);
                // Explore lives on Verda: rebuild residents from the current (possibly Creator-edited) towns.
                if(game::rules_for(game_mode).civilian_life) {
                    island_life.build(verda_region,&character_registry);island_life.reset(npcs);
                } else island_life.clear(npcs);
                br_bots.clear();sound_bus.clear();nav_grid.clear(); // the map may have been edited
                zombie_hour=17.5F;lit_asset_count=static_cast<std::size_t>(-1);
                justice.reset(nullptr,nullptr);crimes.clear();police_tracers.clear();
                rebuild_signs();
                match_time=0;match_placement=-1;
                last_player_feet={player.position.x,player.position.y-1.0F,player.position.z};
                if(game::rules_for(game_mode).combat_bots) {
                    br_bots.start(verda_region,home_screen.bot_level(),23,static_cast<std::uint64_t>(GetTime()*1000.0)+1,last_player_feet);
                    br_bots.sync_bodies(npcs,&character_registry);
                }
                player_health.reset();
                recoil_pitch = recoil_yaw = 0.0F;
                trigger_ready = false;
                inventory_open = false;
                interaction_remaining = 0;
#ifdef OUTLAND_DEV_TOOLS
                if(game_mode==game::GameMode::DevLab) {
                    dev_lab.begin_builder();
                    creator_controller.state().flying=true;creator_controller.state().noclip=true;creator_controller.state().grid_step=1;
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
        // Leaving the builder: whatever was placed or removed changes where bodies can walk.
        if(nav_was_building && !creator_active) nav_grid.clear();
        nav_was_building=creator_active;
        creator_controller.set_enabled(creator_active);
        creator_touch_ui.update(creator_controller, screen_width, screen_height, dev_modal || !IsWindowFocused(),
            [&](Vector2 p){return CheckCollisionPointRec(p,audio_button) || dev_lab.owns_point(p,screen_width,screen_height) || input_system.navigation_owns_point(p,screen_width,screen_height);});
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
                    resolve_body_movement(
                        player.position,
                        desired_position,
                        player.position.y - 1.0F,
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
                    ) ||
                world::physics::WorldCollision::
                    mesh_vault_target(
                        player.position,
                        player.position.y - 1.0F,
                        vault_forward,
                        verda_region,
                        vault_landing
                    )
            ) {
                player.position.x =
                    vault_landing.x;

                player.position.z =
                    vault_landing.z;

                // Snap onto the ground (terrain or building floor) on the destination side.
                player.position.y =
                    world::physics::WorldCollision::ground_height(
                        player.position,
                        player.position.y - 1.0F,
                        verda_region
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
            // Terrain, model floors and stairs (up to a 0.5 m step). Walking off a floor falls.
            const float ground =
                world::physics::WorldCollision::ground_height(
                    player.position,
                    player.position.y - 1.0F,
                    verda_region
                );
            if (player.position.y - 1.0F - ground > 0.6F) {
                player.grounded = false;
                player.vertical_velocity = 0.0F;
            } else {
                player.position.y = ground + 1.0F;
            }
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
                world::physics::WorldCollision::ground_height(
                    player.position,
                    player.position.y - 1.0F,
                    verda_region,
                    0.25F
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

                if(creator_touch_ui.actions().world_pointer)creator_touch_ui.resolve_world_press(creator_controller,verda_region,
                    GetScreenToWorldRayEx(creator_touch_ui.actions().world_point,camera,screen_width,screen_height));
                const auto& action=creator_touch_ui.actions();
                if(action.select)creator_controller.select_target(verda_region,camera.position,camera_direction);
                if(action.place && creator_session.edit(verda_region,[&]{return creator_controller.place_selected(verda_region);}))creator_controller.clear_selection();
                if(action.move)creator_session.edit(verda_region,[&]{return creator_controller.move_selected(verda_region);});
                if(action.duplicate)creator_session.edit(verda_region,[&]{return creator_controller.duplicate_selected(verda_region);});
                if(action.erase)creator_session.edit(verda_region,[&]{return creator_controller.delete_selected(verda_region);});
                if(action.rotate) {
                    const float step=creator_controller.state().rotation_step();
                    if(creator_controller.selection().valid())creator_session.edit(verda_region,[&]{return creator_controller.rotate_selected(verda_region,step);});
                    else creator_controller.rotate_preview(step);
                }
                // Building purposes (Explore's homes, shops, pubs...): USE button, U / Shift+U.
                if(action.purpose || (IsKeyPressed(KEY_U) && creator_controller.selection().valid())) {
                    const int direction=IsKeyDown(KEY_LEFT_SHIFT) ? -1 : 1;
                    creator_session.edit(verda_region,[&]{return creator_controller.cycle_selected_purpose(verda_region,direction);});
                }
                {
                    bool applicable=false;
                    const auto purpose=creator_controller.selected_purpose(verda_region,applicable);
                    creator_touch_ui.set_purpose_label(applicable ? std::string("USE: ")+world::building_purpose_name(purpose) : std::string());
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
        // Session clock for sounds and the match; it stops while the game is paused.
        if(!vehicle_paused) match_time+=dt;
        sound_bus.advance(match_time);
        {
            const Vector3 feet{player.position.x,player.position.y-1.0F,player.position.z};
            if(player_health.alive() && weapons.events().shots>0)
                sound_bus.emit(game::sound::SoundKind::Gunshot,feet,
                    weapons.selected()==game::combat::WeaponId::Rifle ? game::sound::radius::rifle : game::sound::radius::pistol,0,match_time);
            const float moved=dt>0 ? std::hypot(feet.x-last_player_feet.x,feet.z-last_player_feet.z)/dt : 0.0F;
            if(player_health.alive() && !driving && player.grounded && moved>7.0F)
                sound_bus.emit(game::sound::SoundKind::Footstep,feet,game::sound::radius::sprint,0,match_time);
            if(const auto* car=vehicles.driver()) {
                if(std::abs(car->speed)>2.0F)
                    sound_bus.emit(game::sound::SoundKind::Engine,car->position,game::sound::radius::engine*std::clamp(std::abs(car->speed)/15.0F,.5F,2.0F),0,match_time);
                if(controls.sprint) sound_bus.emit(game::sound::SoundKind::Horn,car->position,game::sound::radius::horn,0,match_time);
            }
        }
        characters::NpcContext npc_context;
        npc_context.sounds=&sound_bus;
        npc_context.player_position={player.position.x,player.position.y-1.0F,player.position.z};
        npc_context.player_alive=player_health.alive();
        npc_context.threatening=weapons.events().shots>0;
        npc_context.paused=vehicle_paused;
        npc_context.visible=[&](Vector3 start,Vector3 end) {return !combat_world.trace_segment(start,end,false,false).hit();};
        npcs.update(dt,npc_context,verda_region);
        if(!island_life.empty()) {
            island_life.hear(sound_bus);
            island_life.update(dt,npc_context.player_position,npcs,vehicle_paused);
        }
        player_health.damage(npcs.events().player_damage);
        // Police gunfire is heard like any other and leaves tracers.
        for(auto& tracer:police_tracers) tracer.life-=dt;
        std::erase_if(police_tracers,[](const auto& tracer){return tracer.life<=0;});
        for(const auto& shot:npcs.events().shots) {
            sound_bus.emit(game::sound::SoundKind::Gunshot,shot.from,game::sound::radius::pistol,-2,match_time);
            police_tracers.push_back({shot.from,shot.to,.09F});
        }
        if(game::rules_for(game_mode).civilian_life && !island_life.empty()) {
            game::law::JusticeFrame frame;
            frame.dt=vehicle_paused ? 0.0F : dt;frame.now=match_time;
            frame.player={player.position.x,player.position.y-1.0F,player.position.z};
            frame.player_alive=player_health.alive();frame.player_fired=weapons.events().shots>0;
            frame.daylight=last_daylight;
            frame.line_of_sight=[&](Vector3 a,Vector3 b){return !combat_world.trace_segment(a,b,false,false,true).hit();};
            if(frame.player_fired) justice.crime(game::law::CrimeKind::ShotsFired,frame.player,-1,frame,island_life);
            for(const auto& c:crimes) justice.crime(c.kind,frame.player,c.victim,frame,island_life);
            if(!vehicle_paused) justice.update(frame,island_life,npcs);
        }
        crimes.clear();
        {
            const Vector3 feet{player.position.x,player.position.y-1.0F,player.position.z};
            if(br_bots.active() && !vehicle_paused) {
                game::ai::BattleRoyaleFrame frame;
                frame.dt=dt;frame.now=match_time;frame.player_feet=feet;
                frame.player_velocity=dt>0 ? Vector3Scale(Vector3Subtract(feet,last_player_feet),1.0F/dt) : Vector3{};
                frame.player_alive=player_health.alive();
                frame.player_fired=weapons.events().shots>0;
                frame.player_weapon=weapons.selected();
                frame.player_in_vehicle=driving;
                br_bots.update(frame,bot_world);
                br_bots.sync_bodies(npcs,&character_registry);
                const bool was_alive=player_health.alive();
                player_health.damage(br_bots.events().player_damage);
                if(br_bots.events().player_damage>0) {damage_flash=1.2F;damage_source=br_bots.events().player_damage_from;}
                if(was_alive && !player_health.alive() && match_placement<0) {
                    match_placement=br_bots.alive_bots()+1;
                    br_bots.report_player_death(br_bots.events().player_damage_by);
                }
            }
            last_player_feet=feet;
            damage_flash=std::max(0.0F,damage_flash-dt);
        }
        {
            // Gait follows real ground speed (walk 6 m/s is a jog, sprint 10 m/s a run); firing a
            // gun keeps the legs moving rather than switching to a melee "attack" pose.
            const float ground_speed=dt>0 ? std::hypot(player.position.x-position_before_move.x,player.position.z-position_before_move.z)/dt : 0.0F;
            const auto locomotion=!player.grounded ? characters::AnimationAction::Idle :
                ground_speed>3.2F ? characters::AnimationAction::Run :
                ground_speed>.15F ? characters::AnimationAction::Walk : characters::AnimationAction::Idle;
            character_renderer.update_player(!player_health.alive() ? characters::AnimationAction::Death : locomotion,dt,
                player.grounded ? ground_speed : -1.0F);
        }

        // ====================================================
        // DRAW
        // ====================================================

#ifdef OUTLAND_DEV_TOOLS
        if (game_mode == game::GameMode::DevLab && creator_touch_ui.inventory_open()) asset_thumbnails.render_pending(character_registry);
#endif
        // ----------------------------------------------------
        // TIME OF DAY
        // ----------------------------------------------------
        float hour=12;
        switch(game_mode) {
            case game::GameMode::Explore:hour=island_life.empty() ? 12.0F : island_life.clock().hour();break;
            case game::GameMode::ZombieSurvival:
                if(!vehicle_paused) zombie_hour=std::fmod(zombie_hour+dt/60.0F,24.0F); // an hour a minute
                hour=zombie_hour;break;
            case game::GameMode::BattleRoyale:hour=15.5F;break;
            default:
                if(IsKeyPressed(KEY_F6)) dev_hour=std::fmod(dev_hour+23,24.0F);
                if(IsKeyPressed(KEY_F7)) dev_hour=std::fmod(dev_hour+1,24.0F);
                hour=dev_hour;break;
        }
#ifdef OUTLAND_DEV_TOOLS
        if(const char* forced=std::getenv("OUTLAND_DEV_HOUR")) hour=static_cast<float>(std::atof(forced));
#endif
        const auto sky=world::sky::sky_at(hour);
        last_daylight=sky.daylight;
        {
            std::size_t assets=0;
            for(const auto& settlement:verda_region.settlements()) assets+=settlement.assets.size();
            if(assets!=lit_asset_count) {scene_lights=world::sky::collect_lights(verda_region);lit_asset_count=assets;}
            // Signs follow Creator edits once the builder is closed (a rebuild takes a moment).
            if(!creator_active && game_mode!=game::GameMode::Home && world_size()!=signed_world) rebuild_signs();
        }

        BeginDrawing();

        // The sky is drawn before the world, pre-divided by the ambient multiply that follows.
        ClearBackground(world::sky::before_multiply(sky.horizon,sky.ambient));
        DrawRectangleGradientV(0,0,screen_width,screen_height*3/5,world::sky::before_multiply(sky.zenith,sky.ambient),
            world::sky::before_multiply(sky.horizon,sky.ambient));

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
        sign_renderer.draw(camera.position,camera.target,static_cast<float>(GetTime()));

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

        // Training-only geometry is not authored map data; hide it in the builder.
        if(!creator_active) {
        // Central training structure (DEV only; downtown's main intersection is here otherwise).
        if (combat_world.training_range()) {
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
        }

        game::combat::CombatRenderer::draw_world(weapons,combat_world);
        }
        if(weapons.available() && !driving && !creator_active)game::combat::CombatRenderer::draw_gun(gun_muzzle,gun_direction,weapons.selected(),
            weapons.reload_remaining()/weapons.weapon().reload_seconds,weapons.muzzle_flash());
        if(!creator_active)loot_ui.draw_world(item_registry,loot_session.world(),loot_feet,loot_session.inventory().state().light);

        character_renderer.draw_npcs(npcs, camera.position);
        for(const auto& tracer:br_bots.tracers()) DrawLine3D(tracer.start,tracer.end,Color{255,214,140,230});
        for(const auto& tracer:police_tracers) DrawLine3D(tracer.start,tracer.end,Color{255,214,140,230});

        // ----------------------------------------------------
        // PLAYER BODY
        // ----------------------------------------------------

        if (player.third_person && !driving) {

            const Vector3 character_feet{player.position.x,player.position.y-1.0F,player.position.z};

            character_renderer.draw_player(character_feet, player.yaw);
        }

        EndMode3D();

        // Night: darken everything drawn so far, then add what shines (lamps, fires, headlights,
        // sun, moon, stars) on top, still depth-tested against the world.
        if(sky.ambient.r<255 || sky.ambient.g<255 || sky.ambient.b<255) {
            BeginBlendMode(BLEND_MULTIPLIED);
            DrawRectangle(0,0,screen_width,screen_height,sky.ambient);
            EndBlendMode();
        }
        BeginMode3D(camera);
        world::sky::draw_sky_objects(sky,camera.position);
        world::sky::draw_lights(scene_lights,sky,camera.position,static_cast<float>(GetTime()));
        if(const auto* car=vehicles.driver()) world::sky::draw_headlights(car->position,car->yaw,sky);
        EndMode3D();
        if(justice.wanted()>0 || justice.message_time()>0) {
            // Wanted stars and what the police just did.
            if(justice.wanted()>0) {
                const char* label="WANTED";
                const int x=screen_width/2-(MeasureText(label,20)+4*26)/2;
                DrawText(label,x,64,20,Color{255,90,80,255});
                for(int star=0;star<4;++star) {
                    const Vector2 c{static_cast<float>(x+MeasureText(label,20)+16+star*26),74};
                    DrawPoly(c,5,10,-90,star<justice.wanted() ? Color{255,215,80,255} : Color{90,90,90,200});
                }
            }
            if(justice.message_time()>0) {
                const auto& text=justice.message();
                DrawText(text.c_str(),screen_width/2-MeasureText(text.c_str(),18)/2,92,18,RAYWHITE);
            }
        }
        if(game_mode==game::GameMode::ZombieSurvival || game_mode==game::GameMode::DevLab) {
            const int minutes=static_cast<int>(sky.hour*60)%1440;
            const char* clock=TextFormat("%02d:%02d%s",minutes/60,minutes%60,game_mode==game::GameMode::DevLab ? "  F6/F7" : "");
            DrawText(clock,screen_width/2-MeasureText(clock,18)/2,40,18,RAYWHITE);
        }

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

        if(!island_life.empty() && !creator_active) {
            const auto clock_label=island_life.clock().label();
            const int clock_width=MeasureText(clock_label.c_str(),18);
            DrawText(clock_label.c_str(),screen_width/2-clock_width/2,10,18,BLACK);
            const Vector3 feet{player.position.x,player.position.y-1.0F,player.position.z};
            if(const auto* resident=island_life.nearest(feet,3.5F)) {
                // Names are Esperanto (Paŭlo, Kovač): drawn with the sign font, which has the letters.
                const auto line=island_life.describe(*resident);
                const Font& font=sign_renderer.font();
                const int width=static_cast<int>(MeasureTextEx(font,line.c_str(),17,1).x);
                DrawRectangle(screen_width/2-width/2-8,screen_height-122,width+16,26,Fade(BLACK,.55F));
                DrawTextEx(font,line.c_str(),{static_cast<float>(screen_width/2-width/2),static_cast<float>(screen_height-118)},17,1,RAYWHITE);
            }
        }

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
            if (br_bots.active()) {
                const int alive=br_bots.alive_bots()+(player_health.alive() ? 1 : 0);
                const char* status=TextFormat("ALIVE %d   KILLS %d   BOTS %s",alive,br_bots.player_kills(),game::ai::bot_level_name(br_bots.level()));
                DrawText(status,screen_width/2-MeasureText(status,20)/2,16,20,RAYWHITE);
                int line=0;
                // Kill feed under the match status, clear of the HUD labels and touch buttons.
                for(const auto& entry:br_bots.feed()) DrawText(entry.c_str(),screen_width/2-MeasureText(entry.c_str(),16)/2,44+20*line++,16,Color{235,215,180,255});
                if(damage_flash>0 && player_health.alive()) {
                    // Where the shot came from, relative to where the camera looks.
                    const Vector3 look=Vector3Subtract(camera.target,camera.position);
                    const Vector3 to=Vector3Subtract(damage_source,camera.position);
                    const float angle=std::atan2(to.x,to.z)-std::atan2(look.x,look.z);
                    const float sx=-std::sin(angle), sy=-std::cos(angle);
                    const Vector2 c{screen_width*.5F,screen_height*.5F};
                    const unsigned char alpha=static_cast<unsigned char>(std::min(1.0F,damage_flash)*220);
                    const Vector2 tip{c.x+sx*150,c.y+sy*150}, base{c.x+sx*118,c.y+sy*118};
                    const Vector2 side{-sy*20,sx*20};
                    DrawTriangle(tip,Vector2Add(base,side),Vector2Subtract(base,side),Color{230,40,30,alpha});
                    DrawTriangle(tip,Vector2Subtract(base,side),Vector2Add(base,side),Color{230,40,30,alpha});
                }
                if(player_health.alive() && br_bots.alive_bots()==0) {
                    const char* win="LAST ONE STANDING - YOU WIN";
                    DrawText(win,screen_width/2-MeasureText(win,34)/2,screen_height/2-60,34,GOLD);
                } else if(!player_health.alive()) {
                    const char* place=TextFormat("PLACED #%d",match_placement>0 ? match_placement : br_bots.alive_bots()+1);
                    DrawText(place,screen_width/2-MeasureText(place,26)/2,screen_height/2+34,26,RAYWHITE);
                }
            }
        }
        bool asset_drawer_open = false;
#ifdef OUTLAND_DEV_TOOLS
        asset_drawer_open = game_mode == game::GameMode::DevLab && creator_touch_ui.inventory_open();
#endif
        if (!inventory_open && !asset_drawer_open) touch_hud.draw(
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
        if(creator_active && !creator_touch_ui.inventory_open() && !dev_lab.tools_open()) {
            DrawLine(screen_width/2-6,screen_height/2,screen_width/2+6,screen_height/2,WHITE);
            DrawLine(screen_width/2,screen_height/2-6,screen_width/2,screen_height/2+6,WHITE);
        }
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
