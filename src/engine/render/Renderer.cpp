#include "outland/platform/Runtime.hpp"
#include "outland/engine/render/ViewFrustum.hpp"
#include "outland/engine/render/Renderer.hpp"
#include "outland/engine/audio/EnvironmentAudio.hpp"
#include "outland/game/combat/CombatRenderer.hpp"
#include "outland/game/combat/CombatFeedback.hpp"
#include "outland/engine/audio/GunAudio.hpp"
#include "outland/game/combat/Grenades.hpp"
#include "outland/game/combat/Sights.hpp"
#include "outland/game/inventory/LootSession.hpp"
#include "outland/game/inventory/LootUI.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/game/vehicles/VehicleRenderer.hpp"
#include "outland/game/vehicles/VehicleAudio.hpp"
#include "outland/assets/ModelCache.hpp"
#include "outland/engine/render/FrameTimings.hpp"
#include "outland/engine/render/RenderScale.hpp"
#include "outland/engine/render/StaticWorld.hpp"

#include "outland/input/InputSystem.hpp"
#include "outland/input/PointerEvents.hpp"
#include <unordered_set>
#include "outland/input/TouchHUD.hpp"
#include "outland/game/GameMode.hpp"
#include "outland/game/HomeScreen.hpp"
#include "outland/game/ModeRules.hpp"
#include "outland/game/ModeSession.hpp"
#include "outland/game/PlayUI.hpp"
#include "outland/game/MapScreen.hpp"
#include "outland/game/transit/BusLine.hpp"
#include "outland/world/Zones.hpp"
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
#include "outland/game/zombies/ZombieEcology.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <array>
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

struct WorldDrawStats { int models{0}, meshes{0}; double region{0}, assets{0}, cars{0}, signs{0}, bots{0}, npcs{0}, vehicles{0}; };
WorldDrawStats& world_draw_stats() { static WorldDrawStats stats; return stats; }
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
            else if(world_asset_model_cache().deferred()) {
                queue_.emplace_back(id,path);break; // retry, never cache a deferred preview as missing
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
    const characters::CharacterRegistry& registry,
    const ViewFrustum& view
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

            if(!view.sphere({asset.position.x,asset.position.y+asset.size.y*.5F,asset.position.z},
                std::max(2.0F,Vector3Length(asset.size)))) continue;
            Model* model =
                get_world_asset_model(
                    asset.model_path, registry
                );

            if (model == nullptr) {
                continue;
            }

            ++world_draw_stats().models; world_draw_stats().meshes += model->meshCount;
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

    // No multisampling by default: on phones it multiplies the pixel work of every frame, and the
    // world is drawn into a scaled render target anyway (OUTLAND_MSAA=1 brings it back).
    const char* msaa = std::getenv("OUTLAND_MSAA");
    SetConfigFlags(
        (msaa && *msaa == '1' ? FLAG_MSAA_4X_HINT : 0) |
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
#ifdef __EMSCRIPTEN__
    SetTargetFPS(0); // requestAnimationFrame owns pacing; no browser busy-wait.
#else
    SetTargetFPS(60);
#endif

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
    FrameTimings perf;
    RenderScale render_scale;
    float last_submit_seconds=0;
    StaticWorld static_world;
    RenderTexture2D scene_target{};
    float npc_reconcile_timer=.5F;

    input::PointerEvents pointer_events;
    TraceLog(LOG_INFO,pointer_events.installed() ? "Desktop click capture enabled":"Desktop click capture unavailable - using raylib input polling");
    input::InputSystem input_system;
    input::TouchHUD touch_hud;
    bool inventory_open = false;
    std::string interaction_message;
    float interaction_remaining = 0.0F;

    game::HomeScreen home_screen;
#ifdef OUTLAND_DEV_TOOLS
    dev::DevLab dev_lab;
#endif

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
    game::ModeSession mode_session;
    game::PlayUI play_ui;
    game::economy::Economy economy;
    std::string economy_error;
    if(!economy.load(item_registry,character_root+"/assets/verda/economy/offers.tsv",economy_error))
        TraceLog(LOG_WARNING,"Economy unavailable: %s",economy_error.c_str());
    bool shop_open=false;
    const game::economy::Shop* current_shop=nullptr;
    std::string shop_status;
    game::GameMode queued_mode=game::GameMode::Home;
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

    // Combat feedback (numbers, kill marker, hurt flash and arrow, shake) and frag grenades.
    game::combat::CombatFeedback combat_feedback;
    game::combat::Grenades grenades;
    game::combat::SightMounts sight_mounts;            // the optic on each gun
    std::array<int,game::combat::attachments.size()> sight_seen{}; // bag counts last frame
    float health_seen=100;          // player health after the last combat step
    Vector3 hurt_from{};            // who hurt the player this step, if anyone in particular
    bool hurt_directional=false;
    std::vector<Vector3> pending_kills; // the player's kills this step (marked after the hits)
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
    // Zombie Survival's infected: nests, hordes, noise; bodies near the player (NpcSystem).
    game::zombies::ZombieEcology zombies;
    std::uint64_t zombie_seed=0x2b7e1516U;
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
    std::vector<world::WorldAsset> match_fleet;
    bool match_fleet_active=false, fleet_was_dirty=false;
    const auto restore_match_fleet=[&]{
        if(!match_fleet_active) return;
        vehicles.restore_checkpoint(verda_region,match_fleet);
        vehicle_dirty=fleet_was_dirty;match_fleet.clear();match_fleet_active=false;
    };
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
    // Gunfire you hear: your own reports, everyone else's placed in the world, grenade blasts.
    audio::GunAudio gun_audio(std::string(GetApplicationDirectory()) + "assets/audio/weapons");
    std::uint64_t gunfire_heard=0; // sound-bus serial already turned into audio

    // The island map (M / MAP) and the player's settings it carries.
    game::MapScreen map_screen;
    // The island bus line and its drivers' animation clocks.
    game::transit::BusLine bus_line;
    std::uint64_t bus_seed=0;
    std::array<characters::AnimationController,8> bus_drivers{};
    game::GameSettings settings;
    const std::string settings_path=game::GameSettings::path_for(map_path);
    settings.load(settings_path);
    const auto apply_settings=[&]() {
        input_system.layout().look_sensitivity=settings.look_sensitivity;
        environment_audio.set_volume(settings.volume);
        if(environment_audio.muted()!=settings.muted) environment_audio.toggle_mute();
        render_scale.set_mode(static_cast<int>(settings.graphics));
    };
    const auto store_settings=[&]() {
        settings.volume=environment_audio.volume();settings.muted=environment_audio.muted();
        if(!settings.save(settings_path)) TraceLog(LOG_WARNING,"Settings could not be saved: %s",settings_path.c_str());
        platform::flush_saves();
    };
    apply_settings();
    // Entering a zone shows its name for a few seconds.
    int shown_zone=-1;
    float zone_banner=0;

    game::combat::WeaponSystem weapons;
    game::combat::CombatWorld combat_world(verda_region);
    game::Health player_health;
    game::vehicles::VehicleAudio vehicle_audio;
    vehicles.bind_occupant_damage([&](float amount){player_health.damage(amount);});
    combat_world.bind_vehicles([&](Vector3 start,Vector3 end,bool occupants){return vehicles.trace(verda_region,start,end,occupants);},[&](const game::combat::BulletHit& hit,float amount){vehicles.damage(verda_region,hit,amount);vehicle_dirty=true;});
    // The player's damage to a person (bullets and grenades): bots, residents, police, zombies.
    // Returns true when it kills. Hurting a resident is a crime if anyone saw it.
    const auto damage_actor=[&](int actor,float damage,Vector3 attacker) -> bool {
        if(actor<0 || static_cast<std::size_t>(actor)>=npcs.actors().size()) return false;
        // Battle Royale bodies belong to their bot brains; everyone else is an NpcSystem actor.
        const auto& body=npcs.actors()[static_cast<std::size_t>(actor)];
        const Vector3 where=body.position;
        bool killed=false;
        if(body.bot>=0) killed=br_bots.damage_bot(body.bot,damage,attacker,match_time);
        else if(npcs.damage(static_cast<std::size_t>(actor),damage,attacker)) {
            const auto& victim=npcs.actors()[static_cast<std::size_t>(actor)];
            killed=victim.health<=0;
            if(victim.resident>=0) crimes.push_back({victim.police ? game::law::CrimeKind::AttackOnPolice :
                victim.health<=0 ? game::law::CrimeKind::Murder : game::law::CrimeKind::Assault,victim.resident});
        }
        if(killed) pending_kills.push_back(where);
        return killed;
    };
    combat_world.bind_actors([&](Vector3 start,Vector3 end) {
        const auto hit=npcs.trace_segment(start,end);
        return game::combat::BulletHit{hit.actor>=0 ? game::combat::HitKind::Npc : game::combat::HitKind::None,
            hit.fraction,hit.position,hit.normal,hit.actor,hit.headshot,hit.legs ? 2 : 0};
    },[&](int actor,float damage,Vector3 attacker) {damage_actor(actor,damage,attacker);});
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
    float fp_aim_blend = 0.0F, crouch_blend = 0.0F;
    float recoil_debt = 0.0F, recoil_settle = 0.0F, recoil_quiet = 0.0F; // climb owed back after a burst
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
        platform::begin_frame();
        if(platform::save_requested() && game_mode!=game::GameMode::Home && game_mode!=game::GameMode::DevLab) {
            if(!loot_session.save(weapons) && loot_session.can_persist())
                TraceLog(LOG_WARNING,"Platform checkpoint failed");
            if(vehicle_dirty && !vehicle_save_protected && vehicles.save_state(verda_region,vehicle_state_path,vehicle_error)) vehicle_dirty=false;
            platform::flush_saves();
        }
        nav_searches_this_frame=0;
        world_asset_model_cache().begin_frame(1);
        character_renderer.begin_frame();
        perf.begin(GetTime());
        const double frame_start=GetTime();
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
        if (!map_screen.is_open() && (IsKeyPressed(KEY_N) ||
            (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), audio_button)))) {
            environment_audio.toggle_mute();store_settings();
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {environment_audio.set_volume(environment_audio.volume() - 0.1F);store_settings();}
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {environment_audio.set_volume(environment_audio.volume() + 0.1F);store_settings();}

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
            const game::GameMode selected = queued_mode != game::GameMode::Home ? queued_mode :
                home_screen.update(
                    screen_width,
                    screen_height
                );

            queued_mode=game::GameMode::Home;
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

                bool explicit_spawn=false;
                const auto session_seed=static_cast<std::uint64_t>(GetTime()*1000.0)+1;
                player.position=Vector3Add(game::session_spawn(verda_region,game_mode,session_seed),{0,1,0});
                player.yaw=player.pitch=0;

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
                        player.yaw=yaw_degrees*DEG2RAD;explicit_spawn=true;
                    }
                }
#endif
                weapons.reset(game_mode == game::GameMode::DevLab);
                if(game_mode!=game::GameMode::DevLab)loot_session.start(game_mode,map_path,weapons);
                map_screen.close();shown_zone=-1;
#ifdef OUTLAND_DEV_TOOLS
                // OUTLAND_DEV_ARMOURY=1: every gun, loaded, for trying them out.
                if(const char* armoury=std::getenv("OUTLAND_DEV_ARMOURY"); armoury && *armoury=='1' && game_mode!=game::GameMode::DevLab) {
                    for(const auto sight:game::combat::attachments) // one of each optic first, while the bag has room
                        if(loot_session.inventory().count(std::string(game::combat::sight_info(sight).item))==0)
                            loot_session.edit_inventory().add(item_registry,std::string(game::combat::sight_info(sight).item),1);
                    for(std::size_t i=0;i<game::combat::weapon_count;++i) {
                        const auto id=static_cast<game::combat::WeaponId>(i);
                        for(const auto& item:item_registry.items())
                            if(item.weapon==id && item.category==game::inventory::ItemCategory::Weapon && !loot_session.inventory().owns_weapon(item_registry,id)
                               && loot_session.edit_inventory().add(item_registry,item.id,1)==1)
                                weapons.set_loaded(id,game::combat::definition(id).magazine);
                    }
                    loot_session.changed();
                }
#endif
                combat_world.reset_targets();
                combat_world.set_training_range(game_mode == game::GameMode::DevLab);
                vehicles.set_training_structure(game_mode == game::GameMode::DevLab);
                if(bus_line.running()) bus_line.stop(verda_region,vehicles);
                vehicles.reconcile(verda_region);
                if(game_mode==game::GameMode::BattleRoyale) {
                    match_fleet=vehicles.checkpoint(verda_region);match_fleet_active=true;fleet_was_dirty=vehicle_dirty;
                    vehicles.reset_match_fleet(verda_region);
                }
                npcs.reset_session();
                world::physics::MeshCollisionLibrary::preload(verda_region);
                // Explore lives on Verda: rebuild residents from the current (possibly Creator-edited) towns.
                if(game::rules_for(game_mode).civilian_life) {
                    island_life.build(verda_region,&character_registry);
                    island_life.bind_vehicles(&vehicles,&verda_region); // residents drive the island's cars
                    island_life.reset(npcs,game::life::WorldClock::at(0,9,30));
                    // Linio 1: the island buses, each with a driver.
                    if(bus_line.build(verda_region,vehicles,&character_registry,++bus_seed)) bus_line.start(verda_region,vehicles);
                    economy.build(island_life.places());
                    // Start outside a populated public shop, using authored purpose/door data.
                    if(!explicit_spawn && !economy.shops().empty()) {
                        const auto start_feet=Vector3Subtract(player.position,{0,1,0});
                        const auto shop=std::min_element(economy.shops().begin(),economy.shops().end(),[&](const auto& a,const auto& b){
                            return Vector3DistanceSqr(start_feet,a.door)<Vector3DistanceSqr(start_feet,b.door);
                        });
                        const auto door=shop->door;
                        if(!world::physics::WorldCollision::body_blocked(door,door.y,verda_region,.4F))
                            player.position=Vector3Add(door,{0,1,0});
                    }
                } else {island_life.clear(npcs);island_life.bind_vehicles(nullptr,nullptr);}
                npc_reconcile_timer=.5F;
                if(game::rules_for(game_mode).zombie_ecology) {
                    zombies.build(verda_region,&character_registry,++zombie_seed);zombies.reset(npcs);
                    zombies.clear_area({player.position.x,player.position.y-1,player.position.z},70); // a safe start
                } else zombies.clear(npcs);
                br_bots.clear();sound_bus.clear();nav_grid.clear(); // the map may have been edited
                zombie_hour=17.5F;lit_asset_count=static_cast<std::size_t>(-1);
                justice.reset(nullptr,nullptr);crimes.clear();police_tracers.clear();
                rebuild_signs();static_world.invalidate();
                match_time=0;shop_open=false;current_shop=nullptr;
                last_player_feet={player.position.x,player.position.y-1.0F,player.position.z};
                if(game::rules_for(game_mode).combat_bots) {
                    br_bots.start(verda_region,home_screen.bot_level(),23,session_seed,last_player_feet);
                    br_bots.sync_bodies(npcs,&character_registry);
                }
                mode_session.start(game_mode,static_cast<int>(br_bots.bots().size()),home_screen.bot_level());
                player_health.reset();
                health_seen=player_health.current();hurt_directional=false;pending_kills.clear();
                combat_feedback.reset();
                grenades.reset(game_mode==game::GameMode::DevLab ? 99 : 0); // elsewhere the bag holds them
                sight_mounts.clear();sight_seen.fill(0);
                recoil_pitch = recoil_yaw = recoil_debt = recoil_settle = 0.0F;
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
            restore_match_fleet();
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

        if(mode_session.terminal()) {
            const auto action=play_ui.result_input(screen_width,screen_height,IsWindowFocused());
            if(action!=game::ResultAction::None) {
                if(!loot_session.save(weapons) && loot_session.can_persist()) {
                    interaction_message=loot_session.status();interaction_remaining=3;
                } else {
                    queued_mode=action==game::ResultAction::Restart ? game_mode : game::GameMode::Home;
                    restore_match_fleet();
                    vehicles.leave_session();vehicle_audio.stop();vehicle_renderer.reset_camera();
                    shop_open=inventory_open=false;input_system.cancel_controls();
                    game_mode=game::GameMode::Home;continue;
                }
            }
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
        // The map is modal like the bag; MAP / M opens it during play.
        if(!map_screen.is_open() && game_mode!=game::GameMode::DevLab && !shop_open && !inventory_open && !mode_session.terminal() &&
           (map_screen.tapped_map_button(screen_width,screen_height,IsWindowFocused()) || (IsWindowFocused() && IsKeyPressed(KEY_M)))) {
            map_screen.open();input_system.cancel_controls();
        }
        if(map_screen.is_open() && mode_session.terminal()) map_screen.close();
        // The equipment panel is modal: BAG closes it and GUN changes equipment.
        if(map_screen.is_open()) {
            input_system.update(screen_width,screen_height,true,true,[](Vector2){return true;});
            input_system.player()={};
            settings.volume=environment_audio.volume();settings.muted=environment_audio.muted();
            const auto change=map_screen.update(settings,screen_width,screen_height,IsWindowFocused());
            if(change!=game::MapScreen::Change::None) {apply_settings();store_settings();}
            if(change==game::MapScreen::Change::Close) input_system.cancel_controls();
        } else if(shop_open || mode_session.terminal()) {
            input_system.update(screen_width,screen_height,true,true,[](Vector2){return true;});
            input_system.player()={};
        } else if (inventory_open) {
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
        if((!player_health.alive() || mode_session.terminal()) && !creator_active) input_system.player()={};
        const input::PlayerInput& controls = input_system.player();
        vehicle_reconcile_timer+=dt;
        if(vehicle_reconcile_timer>=.5F){vehicle_dirty|=vehicles.reconcile(verda_region);vehicle_reconcile_timer=0;}
        const Vector3 shop_feet{player.position.x,player.position.y-1,player.position.z};
        const auto reachable_shop=[&](Vector3 door){return !combat_world.trace_segment(
            {player.position.x,player.position.y+.5F,player.position.z},Vector3Add(door,{0,.8F,0}),false,false).hit();};
        const auto* nearby_shop=game_mode==game::GameMode::Explore && !vehicles.driver() ? economy.nearest(shop_feet,reachable_shop) : nullptr;
        if(controls.interact && nearby_shop && !inventory_open && !shop_open && !mode_session.terminal()) {
            current_shop=nearby_shop;shop_open=true;shop_status="Select supplies, equipment or furniture. BUY / SELL transfers one item.";
            play_ui.opened();input_system.cancel_controls();
        }
        if(controls.interact && !creator_active && !inventory_open && !shop_open) {
            Vector3 exit{};
            if(vehicles.driver()){
                if(vehicles.exit(verda_region,exit)){player.position=Vector3Add(exit,{0,1,0});player.vertical_velocity=0;vehicle_renderer.reset_camera();interaction_message="Exited vehicle";}
                else interaction_message="Stop and leave space beside the vehicle to exit";
            }else {
                const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
                const int vehicle=vehicles.nearest(verda_region,feet);
                // Residents' cars: a moving one can't be boarded; taking one is a crime.
                const std::string id=vehicle>=0 ? vehicles.vehicles()[static_cast<std::size_t>(vehicle)].id : std::string();
                if(vehicle>=0 && island_life.car_moving(id) && std::abs(vehicles.vehicles()[static_cast<std::size_t>(vehicle)].speed)>3) interaction_message="It's moving";
                else if(vehicle>=0 && vehicles.enter(verda_region,vehicle,feet)){
                    vehicle_orbit=vehicle_pitch=0;input_system.cancel_controls();interaction_message="Driving - move to accelerate/steer; JUMP brakes; USE exits";
                    if(game::rules_for(game_mode).civilian_life) {
                        const auto taken=island_life.take_car(id);
                        if(taken.occupied) {crimes.push_back({game::law::CrimeKind::Assault,taken.owner});interaction_message="You pulled the driver out - carjacking";}
                        else if(taken.owner>=0) crimes.push_back({game::law::CrimeKind::CarTheft,-1});
                    }
                }
            }
            interaction_remaining=3;
        }
        const bool driving=vehicles.driver()!=nullptr;
        // Handbrake on JUMP (Space), boost on SPRINT (Shift), parking brake on CROUCH, horn on H.
        game::vehicles::VehicleInput driving_input{-controls.move_y,-controls.move_x,controls.brake,controls.crouch,IsKeyDown(KEY_H),controls.sprint};
        const bool vehicle_paused=creator_active||inventory_open||shop_open||mode_session.terminal()||!IsWindowFocused()
#ifdef OUTLAND_DEV_TOOLS
            || (game_mode==game::GameMode::DevLab && dev_lab.tools_open())
#endif
            ;
        const double cars_t0=GetTime();
        vehicle_dirty|=vehicles.update(dt,driving_input,verda_region,{player.position.x,player.position.y-1,player.position.z},vehicle_paused);
        world_draw_stats().vehicles=world_draw_stats().vehicles*.9+(GetTime()-cars_t0)*1000*.1;
        if(driving){vehicle_orbit-=controls.look_x*80*dt;vehicle_pitch=std::clamp(vehicle_pitch-controls.look_y*dt,-1.0F,1.0F);}

        interaction_remaining = std::max(0.0F, interaction_remaining - dt);
        const Vector3 loot_feet{player.position.x,player.position.y-1,player.position.z};
        const auto reachable_pickup=[&](Vector3 point){return !combat_world.trace_segment(
            {player.position.x,player.position.y+.5F,player.position.z},Vector3Add(point,{0,.1F,0}),false,false).hit();};
        if(!creator_active && game_mode != game::GameMode::DevLab) {
            loot_session.update(mode_session.terminal() ? 0.0F : dt,verda_region,loot_feet,weapons);
            if(game_mode==game::GameMode::Explore) {
                if(economy.update(loot_session.economy(),dt,vehicle_paused)) loot_session.changed();
                if(shop_open && current_shop) {
                    const auto action=play_ui.shop_input(economy,*current_shop,screen_width,screen_height,IsWindowFocused());
                    if(action.close) {shop_open=false;input_system.cancel_controls();}
                    else if(action.buy || action.sell) {
                        shop_status=loot_session.trade(economy,*current_shop,economy.offers()[action.offer],1,action.sell,weapons);
                    }
                }
            }
            if(controls.next_weapon)loot_session.changed();
            if(controls.interact && !shop_open && !vehicles.driver() && vehicles.nearest(verda_region,loot_feet)<0){interaction_message=loot_session.interact(loot_feet,weapons,reachable_pickup);interaction_remaining=2.5F;}
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

#ifdef OUTLAND_DEV_TOOLS
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

#endif
        // ====================================================
        // VIEW MODE
        // ====================================================

        if (controls.toggle_view) {
            player.third_person =
                !player.third_person;
        }

        // ====================================================
        // SIGHTS
        // ====================================================

        // Red dots and scopes in the bag mount on guns (one gun per copy); a newly found optic goes
        // on a gun still wearing irons, and X (or SIGHT) changes the held gun's. The Dev Lab has all.
        const bool optics_unlimited=game_mode==game::GameMode::DevLab;
        const game::combat::SightMounts::Owned sights_owned=[&](game::combat::Sight optic) {
            return optics_unlimited ? 99 : loot_session.inventory().count(std::string(game::combat::sight_info(optic).item));
        };
        if(weapons.available()) {
            sight_mounts.validate(sights_owned);
            for(std::size_t i=0;i<game::combat::attachments.size();++i) {
                const int held_now=sights_owned(game::combat::attachments[i]);
                if(held_now>sight_seen[i] && !optics_unlimited)
                    sight_mounts.acquire(game::combat::attachments[i],weapons.selected(),
                        [&](game::combat::WeaponId id){return loot_session.inventory().owns_weapon(item_registry,id);},sights_owned);
                sight_seen[i]=held_now;
            }
            if(controls.sight && !driving && !inventory_open && !shop_open) sight_mounts.cycle(weapons.selected(),sights_owned);
        }
        const auto sight=weapons.available() ? sight_mounts.mounted(weapons.selected()) : game::combat::Sight::Iron;
        const float sight_fov=weapons.available() ? game::combat::sight_fov(weapons.selected(),sight) : 70.0F;

        // ====================================================
        // CAMERA LOOK
        // ====================================================

        // Touch aim assist (as phones get in PUBG and Fortnite): with an enemy near the crosshair
        // the look slows, and while aiming or firing the sight drifts gently onto them. Walls hide
        // them from it; mouse players get none.
        float assist=1.0F;
        if (input_system.touch_device() && !driving && weapons.available() && player_health.alive() && !creator_active) {
            const Vector3 eye{player.position.x,player.position.y+.75F,player.position.z};
            const Vector3 view{std::sin(player.yaw)*std::cos(player.pitch),std::sin(player.pitch),std::cos(player.yaw)*std::cos(player.pitch)};
            float best=1e9F;Vector3 best_dir{};
            for(const auto& a:npcs.actors()) {
                if(a.state==characters::NpcState::Dead || a.health<=0) continue;
                const bool hostile=a.bot>=0 || a.pool==characters::CharacterPool::Hostile || a.pool==characters::CharacterPool::Creature ||
                    (a.police && a.state==characters::NpcState::Chase);
                if(!hostile) continue;
                const Vector3 chest{a.position.x,a.position.y+1.25F,a.position.z};
                const Vector3 to=Vector3Subtract(chest,eye);
                const float distance=Vector3Length(to);
                if(distance<1 || distance>90) continue;
                const Vector3 dir=Vector3Scale(to,1/distance);
                const float angle=std::acos(std::clamp(Vector3DotProduct(view,dir),-1.0F,1.0F));
                const float cone=(controls.aim ? 4.0F : 7.0F)*DEG2RAD+std::atan(.45F/distance);
                if(angle>cone || angle>=best) continue;
                if(combat_world.trace_segment(eye,chest,false,false,false).hit()) continue;
                best=angle;best_dir=dir;
            }
            if(best<1e8F) {
                assist=.55F;
                if(controls.aim || controls.fire) {
                    const float yaw_to=std::atan2(best_dir.x,best_dir.z), pitch_to=std::asin(std::clamp(best_dir.y,-1.0F,1.0F));
                    const float pull=std::min(1.0F,dt*2.2F);
                    player.yaw+=std::remainder(yaw_to-player.yaw,2*PI)*pull;
                    player.pitch+=(pitch_to-player.pitch)*pull;
                }
            }
        }
        // Aiming down sights slows the look in proportion to the zoom, as scopes do.
        const float look_speed =
            input_system
                .layout()
                .look_sensitivity *
            (controls.aim && !driving ? sight_fov / 70.0F : 1.0F) * assist;

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

        // The camera punch of each shot fades in a blink; the climb itself is in player.pitch.
        recoil_pitch *= std::exp(-dt * 14.0F);
        recoil_yaw *= std::exp(-dt * 14.0F);
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

        const float desired_fov = controls.aim && !driving ? sight_fov : 70.0F;
        camera.fovy += (desired_fov-camera.fovy) * (1.0F-std::exp(-dt*12.0F));
        // Through a scope (2x, 4x, sniper) the view is the scope itself, in either camera.
        const bool scoped=controls.aim && !driving && weapons.available() && game::combat::sight_info(sight).scope;
        if (player.third_person && !scoped) {
            const float distance = controls.aim ? 1.9F : 3.6F; // close over the right shoulder, as PUBG and Fortnite frame it
            camera.position = Vector3Add(Vector3Subtract(head_position, Vector3Scale(look_direction,distance)),
                Vector3Add(Vector3Scale(gun_right,controls.aim ? -.45F : -.7F), {0,controls.aim ? .25F : .7F,0}));
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
                if(action.place || action.move || action.duplicate || action.erase || action.rotate || action.undo || action.redo || action.load) {
                    foliage_system.invalidate();world::physics::WorldCollision::invalidate_index();
                }
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
        if(game_mode!=game::GameMode::BattleRoyale && vehicle_dirty && !vehicle_save_protected && vehicle_save_timer>=2){
            if(vehicles.save_state(verda_region,vehicle_state_path,vehicle_error))vehicle_dirty=false;
            else TraceLog(LOG_ERROR,"Vehicle state save: %s",vehicle_error.c_str());
            vehicle_save_timer=0;
        }
        const Vector3 far_point=Vector3Add(camera.position,Vector3Scale(camera_direction,500.0F));
        const auto aimed_hit=combat_world.trace_segment(camera.position,far_point);
        const Vector3 aim_point=aimed_hit.hit() ? aimed_hit.position : far_point;
        // First-person gun: at the hip, or with the sight line on the eye when aiming. Its muzzle
        // is also where every shot leaves from, in either view.
        const auto layout=game::combat::gun_layout(weapons.selected(),sight);
        const Vector3 view_up=Vector3Normalize(Vector3CrossProduct(Vector3CrossProduct(look_direction,{0,1,0}),look_direction));
        const bool sighted=controls.aim && !player.third_person;
        const float pistol_reach=weapons.weapon().hold==game::combat::WeaponHold::Pistol ? .14F : 0.0F;
        fp_aim_blend+=((sighted ? 1.0F : 0.0F)-fp_aim_blend)*(1.0F-std::exp(-dt*14.0F));
        Vector3 gun_grip=Vector3Add(head_position,Vector3Add(
            Vector3Scale(look_direction,Lerp(.30F+pistol_reach,.24F+pistol_reach*1.6F,fp_aim_blend)),
            Vector3Add(Vector3Scale(gun_right,Lerp(-.12F,0.0F,fp_aim_blend)),
                       Vector3Scale(view_up,Lerp(-.16F,-layout.sight,fp_aim_blend)))));
        Vector3 gun_muzzle=game::combat::CombatRenderer::muzzle(gun_grip,look_direction,weapons.selected());
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
        weapon_input.crouched=controls.crouch;
        weapon_input.fire_mode=controls.fire_mode;
        weapon_input.movement=movement_amount;
        npc_reconcile_timer+=dt;
        if(creator_active || npc_reconcile_timer>=.5F) {
            npcs.reconcile(verda_region,character_registry);
            npc_reconcile_timer=0;
        }
        weapons.update((shop_open || mode_session.terminal()) ? 0.0F : dt,weapon_input,{gun_muzzle,gun_direction},combat_world);
        // Battle-royale recoil: each shot lifts the aim itself (the player pulls it back down);
        // once the trigger is released, part of the climb settles back on its own.
        if(weapons.events().shots>0) {
            player.pitch=std::clamp(player.pitch+weapons.events().pitch_kick,-1.2F,1.0F);
            player.yaw+=weapons.events().yaw_kick;
            recoil_debt+=weapons.events().pitch_kick;recoil_quiet=0;recoil_settle=0;
            recoil_pitch=std::min(.035F,recoil_pitch+weapons.events().pitch_kick*.6F);
            recoil_yaw+=weapons.events().yaw_kick*.5F;
        } else {
            recoil_quiet+=dt;
            if(recoil_quiet>.1F && recoil_debt>0) {recoil_settle+=recoil_debt*weapons.weapon().recoil_recovery;recoil_debt=0;}
            if(recoil_settle>0) {
                const float step=recoil_settle*(1-std::exp(-dt*9.0F));
                player.pitch=std::clamp(player.pitch-step,-1.2F,1.0F);recoil_settle-=step;
                if(recoil_settle<.0003F) recoil_settle=0;
            }
        }
        for(const auto& note:weapons.events().hits) combat_feedback.hit(note.position,note.damage,note.headshot);
        // Grenades: G or NADE throws one where the camera looks; blasts hurt everyone in reach
        // whom no wall shields - the player too - and wreck cars.
        // Outside the Dev Lab grenades are bag items (loot, shops, the starter kit): throw what you carry.
        const bool bag_grenades=game_mode!=game::GameMode::DevLab;
        if(bag_grenades) grenades.set_count(loot_session.inventory().count("frag_grenade"));
        if(controls.grenade && !driving && !creator_active && !inventory_open && !shop_open && player_health.alive() && !mode_session.terminal()) {
            const Vector3 carried=dt>0 ? Vector3Scale(Vector3Subtract(player.position,position_before_move),1.0F/dt) : Vector3{};
            if(grenades.throw_grenade(head_position,look_direction,carried) && bag_grenades) {
                loot_session.edit_inventory().remove("frag_grenade",1);
                loot_session.changed();
            }
        }
        grenades.update(vehicle_paused || mode_session.terminal() ? 0.0F : dt,{[&](Vector3 a,Vector3 b,Vector3& point,Vector3& normal) {
            const auto hit=combat_world.trace_segment(a,b,false,false,true);
            if(!hit.hit()) return false;
            point=hit.position;normal=hit.normal;return true;
        }});
        for(const Vector3 blast:grenades.exploded()) {
            sound_bus.emit(game::sound::SoundKind::Gunshot,blast,400,0,match_time);
            gun_audio.explosion(blast,camera.position,Vector3Subtract(camera.target,camera.position));
            const Vector3 core=Vector3Add(blast,{0,.35F,0});
            const auto open=[&](Vector3 to){return !combat_world.trace_segment(core,to,false,false,false).hit();};
            const Vector3 chest{player.position.x,player.position.y-.2F,player.position.z};
            const float near=Vector3Distance(core,chest);
            combat_feedback.blast(std::clamp(1-near/45,0.0F,1.0F)*.6F);
            if(!driving && near<game::combat::Grenades::blast_radius && open(chest)) {
                player_health.damage(game::combat::Grenades::damage_at(near));
                hurt_from=blast;hurt_directional=true;
            }
            for(std::size_t i=0;i<npcs.actors().size();++i) {
                const auto& a=npcs.actors()[i];
                if(a.state==characters::NpcState::Dead || a.health<=0) continue;
                const Vector3 target{a.position.x,a.position.y+1.1F,a.position.z};
                const float d=Vector3Distance(core,target);
                if(d>=game::combat::Grenades::blast_radius || !open(target)) continue;
                const float amount=game::combat::Grenades::damage_at(d);
                combat_feedback.hit(target,amount,false);
                damage_actor(static_cast<int>(i),amount,blast);
            }
            for(std::size_t i=0;i<vehicles.vehicles().size();++i) {
                const auto* car=vehicles.asset(verda_region,vehicles.vehicles()[i].id);
                if(!car || car->vehicle.destroyed) continue;
                const float d=std::max(0.0F,Vector3Distance(core,Vector3Add(car->position,{0,.8F,0}))-1.5F);
                if(d>=game::combat::Grenades::blast_radius) continue;
                game::combat::BulletHit hit;hit.kind=game::combat::HitKind::Vehicle;hit.target=static_cast<int>(i);hit.zone=static_cast<int>(game::vehicles::VehicleZone::Body);
                vehicles.damage(verda_region,hit,game::combat::Grenades::damage_at(d)*.8F);
                vehicle_dirty=true;
            }
        }
        if(!pending_kills.empty()) gun_audio.kill_confirm();
        for(const Vector3 where:pending_kills) combat_feedback.kill(where);
        pending_kills.clear();
        environment_audio.play_combat(weapons.selected(),weapons.events());
        gun_audio.set_volume(environment_audio.volume(),environment_audio.muted());
        for(int shot=0;shot<weapons.events().shots;++shot) gun_audio.player_shot(weapons.selected());
        // Session clock for sounds and the match; it stops while the game is paused.
        if(!vehicle_paused) match_time+=dt;
        sound_bus.advance(match_time);
        {
            const Vector3 feet{player.position.x,player.position.y-1.0F,player.position.z};
            if(player_health.alive() && weapons.events().shots>0)
                sound_bus.emit(game::sound::SoundKind::Gunshot,feet,
                    weapons.weapon().hearing,0,match_time);
            const float moved=dt>0 ? std::hypot(feet.x-last_player_feet.x,feet.z-last_player_feet.z)/dt : 0.0F;
            if(player_health.alive() && !driving && player.grounded && moved>7.0F)
                sound_bus.emit(game::sound::SoundKind::Footstep,feet,game::sound::radius::sprint,0,match_time);
            if(const auto* car=vehicles.driver()) {
                if(std::abs(car->speed)>2.0F)
                    sound_bus.emit(game::sound::SoundKind::Engine,car->position,game::sound::radius::engine*std::clamp(std::abs(car->speed)/15.0F,.5F,2.0F),0,match_time);
                if(IsKeyDown(KEY_H)) sound_bus.emit(game::sound::SoundKind::Horn,car->position,game::sound::radius::horn,0,match_time);
            }
        }
        characters::NpcContext npc_context;
        npc_context.sounds=&sound_bus;
        npc_context.player_position={player.position.x,player.position.y-1.0F,player.position.z};
        npc_context.player_alive=player_health.alive();
        npc_context.threatening=weapons.events().shots>0;
        npc_context.paused=vehicle_paused;
        npc_context.visible=[&](Vector3 start,Vector3 end) {return !combat_world.trace_segment(start,end,false,false).hit();};
        const double npcs_t0=GetTime();
        npcs.update(dt,npc_context,verda_region);
        world_draw_stats().npcs=world_draw_stats().npcs*.9+(GetTime()-npcs_t0)*1000*.1;
        if(!island_life.empty()) {
            island_life.hear(sound_bus);
            island_life.update(dt,npc_context.player_position,npcs,vehicle_paused);
        }
        if(bus_line.running()) {
            bus_line.update(vehicle_paused ? 0.0F : dt,verda_region,vehicles,Vector3Subtract(player.position,{0,1,0}),&npcs);
            for(std::size_t i=0;i<bus_drivers.size() && i<bus_line.buses().size();++i) bus_drivers[i].advance(characters::AnimationAction::Idle,dt,0);
        }
        if(!zombies.empty()) {
            zombies.hear(sound_bus);
            zombies.update(vehicle_paused ? 0.0F : dt,{npc_context.player_position,last_daylight,match_time},npcs);
        }
        player_health.damage(npcs.events().player_damage);
        if(npcs.events().player_damage>0) {hurt_from=npcs.events().player_damage_from;hurt_directional=true;}
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
            if(game_mode==game::GameMode::BattleRoyale && !vehicle_paused) {
                game::ai::BattleRoyaleFrame frame;
                frame.dt=dt;frame.now=match_time;frame.player_feet=feet;
                frame.player_velocity=dt>0 ? Vector3Scale(Vector3Subtract(feet,last_player_feet),1.0F/dt) : Vector3{};
                frame.player_alive=player_health.alive();
                frame.player_fired=weapons.events().shots>0;
                frame.player_weapon=weapons.selected();
                frame.player_in_vehicle=driving;
                const double bots_t0=GetTime();
                br_bots.update(frame,bot_world);
                br_bots.sync_bodies(npcs,&character_registry);
                world_draw_stats().bots=world_draw_stats().bots*.9+(GetTime()-bots_t0)*1000*.1;
                player_health.damage(br_bots.events().player_damage);
                if(br_bots.events().player_damage>0) {hurt_from=br_bots.events().player_damage_from;hurt_directional=true;}
            }
            const bool finished=mode_session.terminal();
            mode_session.update(dt,feet,player_health,br_bots,vehicle_paused);
            if(game_mode==game::GameMode::BattleRoyale) br_bots.sync_bodies(npcs,&character_registry);
            if(!finished && mode_session.terminal()) {
                shop_open=inventory_open=false;play_ui.opened();input_system.cancel_controls();
                vehicle_audio.stop();
            }
            last_player_feet=feet;
            // Everyone else's gunfire, heard where it happened (bots, police): the bus says how far
            // it carries, which also tells pistol from rifle from sniper.
            for(const auto& heard:sound_bus.events()) {
                if(heard.serial<=gunfire_heard || heard.kind!=game::sound::SoundKind::Gunshot || heard.source==0) continue;
                const float r=heard.radius;
                const auto weapon=r>=400 ? game::combat::WeaponId::Sniper : r>=290 ? game::combat::WeaponId::Rifle :
                    r>=250 ? game::combat::WeaponId::Shotgun : r>=190 ? game::combat::WeaponId::Smg : game::combat::WeaponId::Pistol;
                gun_audio.world_shot(weapon,heard.position,camera.position,Vector3Subtract(camera.target,camera.position));
            }
            gunfire_heard=sound_bus.latest();
            gun_audio.update(vehicle_paused ? 0.0F : dt);
            // Whatever hurt the player this step - shots, bites, blasts, crashes, the zone - shows.
            const float drop=health_seen-player_health.current();
            if(drop>.01F && player_health.alive()) combat_feedback.hurt(drop,hurt_from,hurt_directional);
            health_seen=player_health.current();hurt_directional=false;
            combat_feedback.update(vehicle_paused ? 0.0F : dt);
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

        if(perf.enabled()) perf.mark(FrameStage::Simulation,GetTime());
        // The world is drawn at a scaled resolution and stretched to the screen; the HUD stays sharp.
        // The scale follows the frame rate unless OUTLAND_RENDER_SCALE fixes it.
        render_scale.update(GetFrameTime(),last_submit_seconds);
        world_draw_stats().models=world_draw_stats().meshes=0;
        const int scene_width=std::max(64,static_cast<int>(screen_width*render_scale.scale()));
        const int scene_height=std::max(64,static_cast<int>(screen_height*render_scale.scale()));
        if(scene_target.id==0 || scene_target.texture.width!=scene_width || scene_target.texture.height!=scene_height) {
            if(scene_target.id!=0) UnloadRenderTexture(scene_target);
    static_world.invalidate();
            scene_target=LoadRenderTexture(scene_width,scene_height);
            SetTextureFilter(scene_target.texture,TEXTURE_FILTER_BILINEAR);
        }
        if(!driving) {
            const Vector3 shake=combat_feedback.shake();
            camera.position=Vector3Add(camera.position,shake);camera.target=Vector3Add(camera.target,shake);
        }
        BeginTextureMode(scene_target);

        // The sky is drawn before the world, pre-divided by the ambient multiply that follows.
        ClearBackground(world::sky::before_multiply(sky.horizon,sky.ambient));
        DrawRectangleGradientV(0,0,scene_width,scene_height*3/5,world::sky::before_multiply(sky.zenith,sky.ambient),
            world::sky::before_multiply(sky.horizon,sky.ambient));

        BeginMode3D(camera);

        // ----------------------------------------------------
        // VERDA TERRAIN
        // ----------------------------------------------------

        const ViewFrustum view(camera,static_cast<float>(screen_width)/std::max(1,screen_height));
        terrain_world.update(camera.position);
        terrain_world.draw(&view);
        if(perf.enabled()) perf.mark(FrameStage::Terrain,GetTime());

        // ----------------------------------------------------
        // VERDA FOLIAGE
        // ----------------------------------------------------

        foliage_system.draw(
            camera.position, verda_region, &view
        );
        if(perf.enabled()) perf.mark(FrameStage::Foliage,GetTime());


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
        double sub_t0=GetTime();
        // Houses, street trees and roads come from static meshes, except while the Creator is
        // moving them about.
        if(creator_active) {verda_region.draw(camera.position,&view);static_world.invalidate();}
        else static_world.draw(verda_region,camera.position,&view);
        double sub_t1=GetTime();

        draw_model_backed_world_assets(verda_region, camera.position, character_registry, view);
        double sub_t2=GetTime();
        // Bus drivers at the wheel, drawn before the buses so their tinted glass lies over them.
        if(bus_line.running()) for(std::size_t i=0;i<bus_line.buses().size() && i<bus_drivers.size();++i) {
            const auto& bus=bus_line.buses()[i];
            if(!bus.running || bus.driver.empty() || Vector3DistanceSqr(bus.position,camera.position)>120*120) continue;
            const auto* asset=vehicles.asset(verda_region,bus.vehicle);
            const auto* type=asset ? vehicles.definition(*asset) : nullptr;
            if(!asset || !type) continue;
            const float scale=asset->size.x/type->width;
            const auto seat=game::vehicles::VehicleSystem::world_point(type->seat,asset->position,asset->rotation_y,scale);
            characters::BodyPose sitting;sitting.seated=true;
            character_renderer.draw(bus.driver,{seat.x,seat.y+.1F-.92F,seat.z},asset->rotation_y,1,&bus_drivers[i],sitting);
        }
        vehicle_renderer.draw(vehicles,verda_region,camera.position);
        double sub_t3=GetTime();
        sign_renderer.draw(camera.position,camera.target,static_cast<float>(GetTime()));
        double sub_t4=GetTime();
        static double sub_avg[4]{};
        for(int k=0;k<4;++k) {const double t[]{sub_t0,sub_t1,sub_t2,sub_t3,sub_t4};sub_avg[k]=sub_avg[k]*.9+(t[k+1]-t[k])*1000*.1;}
        world_draw_stats().region=sub_avg[0];world_draw_stats().assets=sub_avg[1];world_draw_stats().cars=sub_avg[2];world_draw_stats().signs=sub_avg[3];

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
        if(Model* grenade_model=grenades.live().empty() ? nullptr : get_world_asset_model("assets/verda/combat/frag_grenade.glb",character_registry))
            for(const auto& g:grenades.live()) {
                // Tumbling through the air, settling once it has stopped.
                const float spin=g.fuse*720.0F*std::min(1.0F,Vector3Length(g.velocity)/4.0F);
                DrawModelEx(*grenade_model,Vector3Subtract(g.position,{0,.05F,0}),Vector3Normalize({1,.3F,.2F}),spin,{1.6F,1.6F,1.6F},WHITE);
            }
        for(const auto& e:grenades.explosions()) {
            const float t=e.age/game::combat::Grenades::explosion_life;
            if(t<.25F) {
                // The fireball: a solid white-hot core inside an additive orange burst.
                const float k=t/.25F;
                DrawSphere(e.position,1.0F+k*3.2F,Color{236,110,24,static_cast<unsigned char>(235*(1-k*.6F))});
                if(k<.6F) DrawSphere(e.position,.6F+k*2.0F,Color{255,214,120,255});
                BeginBlendMode(BLEND_ADDITIVE);
                DrawSphere(e.position,.5F+k*1.4F,Color{static_cast<unsigned char>(200*(1-k)),static_cast<unsigned char>(150*(1-k)),static_cast<unsigned char>(60*(1-k)),255});
                EndBlendMode();
            }
            const float rise=std::min(1.0F,t*1.6F);
            DrawSphere({e.position.x,e.position.y+.6F+t*2.6F,e.position.z},1.0F+rise*2.4F,Fade(Color{72,70,66,255},.6F*(1-t)));
        }
        }
        // A pump or bolt works through the cycle after each shot of a slow gun.
        const auto& held_weapon=weapons.weapon();
        const float gun_work=!held_weapon.automatic && held_weapon.interval>.5F ?
            std::sin(std::clamp(1-weapons.cooldown()/held_weapon.interval,0.0F,1.0F)*PI)*(weapons.cooldown()>0 ? 1.0F : 0.0F) : 0.0F;
        const float gun_reload=weapons.reload_remaining()/held_weapon.reload_seconds;
        if(weapons.available() && !driving && !creator_active && !player.third_person && !scoped)
            game::combat::CombatRenderer::draw_gun(gun_grip,look_direction,view_up,weapons.selected(),gun_reload,weapons.muzzle_flash(),true,gun_work,sight);
        if(!creator_active)loot_ui.draw_world(item_registry,loot_session.world(),loot_feet,loot_session.inventory().state().light);

        if(perf.enabled()) perf.mark(FrameStage::World,GetTime());
        // People hold what they carry: police their pistols, Battle Royale bots their loadout.
        character_renderer.draw_npcs(npcs, camera.position, &view,
            [](const characters::NpcInstance& actor,characters::BodyPose& body) {
                const int weapon=actor.weapon>=0 ? actor.weapon : actor.armed ? 0 : -1;
                if(weapon<0 || weapon>=static_cast<int>(game::combat::weapon_count)) return;
                const auto id=static_cast<game::combat::WeaponId>(weapon);
                const auto gun=game::combat::gun_layout(id);
                body.hold=game::combat::definition(id).hold==game::combat::WeaponHold::Pistol ? characters::BodyHold::Pistol : characters::BodyHold::Shouldered;
                body.stock=gun.stock;body.support=gun.support;
                if(actor.state==characters::NpcState::Wander) body.aim_pitch=-.45F; // low ready while walking
            },
            [](const characters::NpcInstance& actor,const characters::HeldGun& held) {
                const int weapon=actor.weapon>=0 ? actor.weapon : 0;
                game::combat::CombatRenderer::draw_gun(held.grip,held.aim,held.up,static_cast<game::combat::WeaponId>(weapon),0,0,false);
            });
        // Their muzzles flash as they fire, so you see who is shooting at you.
        // No tracer lines (real rounds are not seen in flight); the muzzle flash shows the shooter.
        for(const auto& tracer:br_bots.tracers()) if(tracer.life>.05F) DrawSphere(tracer.start,.1F,Color{255,222,150,235});
        for(const auto& tracer:police_tracers) if(tracer.life>.05F) DrawSphere(tracer.start,.1F,Color{255,222,150,235});

        // ----------------------------------------------------
        // PLAYER BODY
        // ----------------------------------------------------

        if (player.third_person && !driving && !scoped) {

            const Vector3 character_feet{player.position.x,player.position.y-1.0F,player.position.z};

            characters::BodyPose body;
            crouch_blend+=((controls.crouch ? 1.0F : 0.0F)-crouch_blend)*(1.0F-std::exp(-dt*10.0F));
            body.crouch=crouch_blend;
            if(weapons.available() && !creator_active) {
                const auto gun=game::combat::gun_layout(weapons.selected());
                body.hold=held_weapon.hold==game::combat::WeaponHold::Pistol ? characters::BodyHold::Pistol : characters::BodyHold::Shouldered;
                body.stock=gun.stock;body.support=gun.support;
                // Sprinting carries the gun low; otherwise the chest follows the sights.
                body.aim_pitch=weapon_input.sprint ? -.55F : shot_pitch;
            }
            const auto held=character_renderer.draw_player(character_feet, player.yaw, body);
            if(held.held && !scoped) {
                const auto aim=weapon_input.sprint ? held.aim : Vector3Normalize(Vector3Subtract(aim_point,held.grip));
                game::combat::CombatRenderer::draw_gun(held.grip,aim,held.up,weapons.selected(),gun_reload,weapons.muzzle_flash(),false,gun_work,sight);
            }
        }

        if(perf.enabled()) perf.mark(FrameStage::Characters,GetTime());
        EndMode3D();

        // Night: darken everything drawn so far, then add what shines (lamps, fires, headlights,
        // sun, moon, stars) on top, still depth-tested against the world.
        if(sky.ambient.r<255 || sky.ambient.g<255 || sky.ambient.b<255) {
            BeginBlendMode(BLEND_MULTIPLIED);
            DrawRectangle(0,0,scene_width,scene_height,sky.ambient);
            EndBlendMode();
        }
        BeginMode3D(camera);
        world::sky::draw_sky_objects(sky,camera.position);
        world::sky::draw_lights(scene_lights,sky,camera.position,static_cast<float>(GetTime()));
        if(const auto* car=vehicles.driver()) world::sky::draw_headlights(car->position,car->yaw,sky);
        if(bus_line.running()) for(const auto& bus:bus_line.buses())
            if(bus.running && Vector3DistanceSqr(bus.position,camera.position)<350*350) world::sky::draw_headlights(bus.position,bus.yaw,sky);
        EndMode3D();
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(scene_target.texture,{0,0,static_cast<float>(scene_width),-static_cast<float>(scene_height)},
            {0,0,static_cast<float>(screen_width),static_cast<float>(screen_height)},{0,0},0,WHITE);
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
            if(!zombies.empty()) {
                // How bad it is out there, and a warning when a horde is on the move nearby.
                const auto infected=zombies.stats();
                const char* line=TextFormat("INFECTED NEAR %d  |  KILLED %d",infected.near,infected.killed);
                DrawText(line,screen_width/2-MeasureText(line,16)/2,62,16,Color{215,235,190,255});
                const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
                for(const auto& horde:zombies.hordes()) if(std::hypot(horde.position.x-feet.x,horde.position.z-feet.z)<400) {
                    const char* warning="A HORDE IS MOVING NEARBY";
                    DrawText(warning,screen_width/2-MeasureText(warning,18)/2,82,18,Color{255,120,90,255});
                    break;
                }
            }
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
        // A red dot shows its dot once the gun is up (in third person, as soon as aiming starts).
        const bool red_dot_up=controls.aim && !driving && weapons.available() && sight==game::combat::Sight::RedDot && !creator_active;
        if(scoped && !creator_active) game::combat::CombatRenderer::draw_sight_view(sight,screen_width,screen_height,1);
        else if(red_dot_up) game::combat::CombatRenderer::draw_sight_view(sight,screen_width,screen_height,player.third_person ? 1.0F : (fp_aim_blend-.6F)/.4F);
        const int gap=static_cast<int>((controls.aim ? 3 : 8)+movement_amount*5+recoil_pitch*180);
        const Color crosshair_color=weapons.hit_marker()>0 ?
            (weapons.last_headshot() ? ORANGE : GREEN) : RAYWHITE;
        if(!scoped && !red_dot_up) {
            DrawLine(center_x-gap-6,center_y,center_x-gap,center_y,crosshair_color);
            DrawLine(center_x+gap,center_y,center_x+gap+6,center_y,crosshair_color);
            DrawLine(center_x,center_y-gap-6,center_x,center_y-gap,crosshair_color);
            DrawLine(center_x,center_y+gap,center_x,center_y+gap+6,crosshair_color);
        }
        if (weapons.hit_marker()>0) {
            DrawLine(center_x-10,center_y-10,center_x-5,center_y-5,crosshair_color);
            DrawLine(center_x+5,center_y+5,center_x+10,center_y+10,crosshair_color);
            DrawLine(center_x-10,center_y+10,center_x-5,center_y+5,crosshair_color);
            DrawLine(center_x+5,center_y-5,center_x+10,center_y-10,crosshair_color);
        }
        // Kill confirmation: a red cross over the crosshair, and the streak.
        if (combat_feedback.kill_marker()>0) {
            const float k=combat_feedback.kill_marker()/game::combat::CombatFeedback::kill_life;
            const float r=12+10*(1-k);
            const Color red{235,52,40,static_cast<unsigned char>(255*std::min(1.0F,k*1.5F))};
            DrawLineEx({center_x-r,center_y-r},{center_x-r*.4F,center_y-r*.4F},3,red);
            DrawLineEx({center_x+r*.4F,center_y+r*.4F},{center_x+r,center_y+r},3,red);
            DrawLineEx({center_x-r,center_y+r},{center_x-r*.4F,center_y+r*.4F},3,red);
            DrawLineEx({center_x+r*.4F,center_y-r*.4F},{center_x+r,center_y-r},3,red);
            const int streak=combat_feedback.streak();
            const char* label=streak>=4 ? "RAMPAGE" : streak==3 ? "TRIPLE KILL" : streak==2 ? "DOUBLE KILL" : "ELIMINATED";
            DrawText(label,center_x-MeasureText(label,22)/2,center_y-70,22,red);
        }
        // Damage numbers float off whatever was hit: orange for heads, red for the killing blow.
        {
            const Vector3 view=Vector3Subtract(camera.target,camera.position);
            for (const auto& number:combat_feedback.numbers()) {
                if (Vector3DotProduct(view,Vector3Subtract(number.position,camera.position))<=0) continue;
                const Vector2 at=GetWorldToScreen(number.position,camera);
                const float life=std::clamp(number.life/game::combat::CombatFeedback::number_life,0.0F,1.0F);
                const unsigned char alpha=static_cast<unsigned char>(255*std::min(1.0F,life*2));
                const Color colour=number.kill ? Color{240,60,46,alpha} : number.headshot ? Color{255,170,40,alpha} : Color{255,255,255,alpha};
                const char* text=number.amount>0 ? TextFormat(number.kill ? "%.0f  X" : "%.0f",number.amount) : "X";
                const int size=number.headshot || number.kill ? 24 : 19;
                DrawText(text,static_cast<int>(at.x)-MeasureText(text,size)/2+1,static_cast<int>(at.y)+1,size,Color{0,0,0,static_cast<unsigned char>(alpha/2)});
                DrawText(text,static_cast<int>(at.x)-MeasureText(text,size)/2,static_cast<int>(at.y),size,colour);
            }
        }
        if (!creator_active && !inventory_open) {
            DrawRectangle(12,screen_height-103,320,91,Fade(BLACK,.65F));
            DrawText(weapons.available()?weapons.weapon().name:"NO WEAPON - OPEN BAG",24,screen_height-94,19,RAYWHITE);
            DrawText(TextFormat("NADES %d  G",grenades.count()),222,screen_height-68,16,grenades.count()>0 ? Color{190,220,150,255} : GRAY);
            if(weapons.available()) DrawText(TextFormat("%s%s",game::combat::sight_info(sight).name,sight==game::combat::Sight::Sniper ? "" : "  X"),
                200,screen_height-92,14,sight==game::combat::Sight::Iron ? GRAY : Color{235,120,110,255});
            if (weapons.unlimited()) DrawText("AMMO UNLIMITED - DEV LAB",24,screen_height-68,18,YELLOW);
            else DrawText(TextFormat("%d / %d",weapons.ammo().loaded,weapons.ammo().reserve),
                24,screen_height-68,22,RAYWHITE);
            if (weapons.reload_remaining()>0) DrawText(TextFormat("RELOADING %.1fs",weapons.reload_remaining()),
                24,screen_height-39,16,ORANGE);
            else DrawText(weapons.ammo().loaded==0 && !weapons.unlimited() ?
                "EMPTY - R / LOAD TO RELOAD" : "F / FIRE   Q / AIM   R / LOAD   TAB / GUN",
                24,screen_height-39,12,RAYWHITE);
        }

        if (!creator_active && player_health.alive()) {
            // Hurt: the screen's edges flash red, and an arrow points to whoever did it. Low on
            // health, the edges pulse.
            const float low=std::clamp(1-player_health.current()/(player_health.maximum()*.3F),0.0F,1.0F);
            const float pulse=low*(.22F+.12F*std::sin(static_cast<float>(GetTime())*6));
            const float edge=std::clamp(combat_feedback.flash()*.55F+pulse,0.0F,.75F);
            if (edge>0) {
                const Color red{200,20,16,static_cast<unsigned char>(255*edge)}, clear{200,20,16,0};
                const int band_w=screen_width/6, band_h=screen_height/5;
                DrawRectangleGradientH(0,0,band_w,screen_height,red,clear);
                DrawRectangleGradientH(screen_width-band_w,0,band_w,screen_height,clear,red);
                DrawRectangleGradientV(0,0,screen_width,band_h,red,clear);
                DrawRectangleGradientV(0,screen_height-band_h,screen_width,band_h,clear,red);
            }
            if (combat_feedback.indicator()>0) {
                const Vector3 look=Vector3Subtract(camera.target,camera.position);
                const Vector3 to=Vector3Subtract(combat_feedback.source(),camera.position);
                const float angle=std::atan2(to.x,to.z)-std::atan2(look.x,look.z);
                const float sx=-std::sin(angle), sy=-std::cos(angle);
                const Vector2 c{screen_width*.5F,screen_height*.5F};
                const unsigned char alpha=static_cast<unsigned char>(std::min(1.0F,combat_feedback.indicator())*220);
                const Vector2 tip{c.x+sx*150,c.y+sy*150}, base{c.x+sx*118,c.y+sy*118};
                const Vector2 side{-sy*20,sx*20};
                DrawTriangle(tip,Vector2Add(base,side),Vector2Subtract(base,side),Color{230,40,30,alpha});
                DrawTriangle(tip,Vector2Subtract(base,side),Vector2Add(base,side),Color{230,40,30,alpha});
            }
        }
        if (!creator_active) {
            DrawText(TextFormat("HEALTH %.0f / %.0f",player_health.current(),player_health.maximum()),
                16,16,20,player_health.alive() ? RAYWHITE : RED);

            if (br_bots.active()) {
                const int alive=br_bots.alive_bots()+(player_health.alive() ? 1 : 0);
                const char* status=TextFormat("ALIVE %d   KILLS %d   BOTS %s",alive,br_bots.player_kills(),game::ai::bot_level_name(br_bots.level()));
                DrawText(status,screen_width/2-MeasureText(status,20)/2,16,20,RAYWHITE);
                int line=0;
                // Kill feed under the match status, clear of the HUD labels and touch buttons.
                for(const auto& entry:br_bots.feed()) DrawText(entry.c_str(),screen_width/2-MeasureText(entry.c_str(),16)/2,44+20*line++,16,Color{235,215,180,255});
            }
        }
        bool asset_drawer_open = false;
#ifdef OUTLAND_DEV_TOOLS
        asset_drawer_open = game_mode == game::GameMode::DevLab && creator_touch_ui.inventory_open();
#endif
        if (!inventory_open && !shop_open && !mode_session.terminal() && !asset_drawer_open) touch_hud.draw(
            controls,
            input_system.layout(),
            screen_width,
            screen_height,
            !creator_active,
            weapons.available() && weapons.weapon().automatic && !driving ? (weapons.single_fire() ? "SINGLE" : "AUTO") : nullptr,
            weapons.available() && !driving && sight!=game::combat::Sight::Sniper ?
                (sight==game::combat::Sight::RedDot ? "DOT" : sight==game::combat::Sight::Scope2x ? "2X" : sight==game::combat::Sight::Scope4x ? "4X" : "IRON") : nullptr
        );

#ifdef OUTLAND_DEV_TOOLS
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

#endif

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
        if(!creator_active && !inventory_open && !shop_open && !mode_session.terminal()){
            const int nearby=loot_session.world().nearest(loot_feet,2.8F,reachable_pickup);
            if(nearby>=0){const auto& pickup=loot_session.world().state().pickups[static_cast<std::size_t>(nearby)];
                const auto* item=item_registry.find(pickup.item);
                if(item)DrawText(TextFormat("E / USE: %s x%d",item->name.c_str(),pickup.quantity),screen_width/2-180,screen_height-140,18,YELLOW);
            }
        }
        if (interaction_remaining > 0)
            DrawText(interaction_message.c_str(), screen_width / 2 - MeasureText(interaction_message.c_str(),18)/2, screen_height-205, 18, RAYWHITE);

        DrawRectangleRec(audio_button, Fade(BLACK, 0.60F));
        DrawText(environment_audio.ready() ?
                 (environment_audio.muted() ? "AUDIO OFF · N" : "AUDIO ON · N") : "AUDIO UNAVAILABLE",
                 screen_width - 165, 53, 14, RAYWHITE);
        if(game_mode!=game::GameMode::DevLab) {
            const auto map_button=game::MapScreen::map_button(screen_width,screen_height);
            DrawRectangleRec(map_button,Fade(BLACK,.60F));
            DrawText("MAP · M",static_cast<int>(map_button.x)+14,53,14,RAYWHITE);
            // Entering a zone: its name, what it means, and what it is like.
            const int here=static_cast<int>(world::zones::zone_at(player.position));
            if(here!=shown_zone) {shown_zone=here;zone_banner=4.5F;}
            if(zone_banner>0 && !map_screen.is_open()) {
                zone_banner-=GetFrameTime();
                const auto& zone=world::zones::zones[static_cast<std::size_t>(here)];
                const float alpha=std::clamp(std::min(zone_banner,4.5F-zone_banner)*2,0.0F,1.0F);
                const Font& font=sign_renderer.font();
                const std::string title="ZONO "+std::string(zone.name);
                const float big=34,small=18;
                const float tw=MeasureTextEx(font,title.c_str(),big,1).x;
                const std::string sub=std::string(zone.meaning)+"  ·  "+std::string(zone.character);
                const float sw=MeasureTextEx(font,sub.c_str(),small,1).x;
                const float top=screen_height*.27F;
                DrawRectangle(static_cast<int>(screen_width/2-std::max(tw,sw)/2-18),static_cast<int>(top-10),static_cast<int>(std::max(tw,sw)+36),static_cast<int>(big+small+30),Fade(BLACK,.5F*alpha));
                DrawRectangle(static_cast<int>(screen_width/2-std::max(tw,sw)/2-18),static_cast<int>(top-10),6,static_cast<int>(big+small+30),Fade(zone.color,alpha));
                DrawTextEx(font,title.c_str(),{screen_width/2-tw/2,top},big,1,Fade(RAYWHITE,alpha));
                DrawTextEx(font,sub.c_str(),{screen_width/2-sw/2,top+big+6},small,1,Fade(Color{220,226,206,255},alpha));
            }
        }

#ifdef OUTLAND_DEV_TOOLS
        if(game_mode==game::GameMode::DevLab)dev_lab.draw_tools(screen_width,screen_height,creator_active);
#endif
        if(const auto* driver=vehicles.driver())DrawText(TextFormat("%.0f km/h%s | JUMP: handbrake | SPRINT: boost | CROUCH: park | H: horn | USE: exit",std::abs(driver->speed)*3.6F,driver->boosting ? "  BOOST" : ""),20,screen_height-32,16,YELLOW);
        else if(!creator_active && !inventory_open && vehicles.nearest(verda_region,{player.position.x,player.position.y-1,player.position.z})>=0)DrawText("USE / E - Enter vehicle",screen_width/2-120,screen_height-100,20,YELLOW);
        if(game_mode!=game::GameMode::DevLab && !mode_session.terminal() && !shop_open && !inventory_open) {
            const int objective_bottom=game::PlayUI::draw_objective(mode_session,screen_width);
            if(game_mode==game::GameMode::BattleRoyale) {
                DrawText(TextFormat("ZONE %.0fm | distance %.0fm | %s",mode_session.radius(),mode_session.distance(loot_feet),mode_session.distance(loot_feet)>mode_session.radius() ? "DANGER - move toward capital" : "inside safe zone"),20,objective_bottom+6,16,mode_session.distance(loot_feet)>mode_session.radius() ? RED : RAYWHITE);
            }
            if(game_mode==game::GameMode::Explore) {
                DrawText(TextFormat("%lld kronoj | stores: BUY supplies / SELL scavenged goods",static_cast<long long>(loot_session.economy().money)),20,objective_bottom+6,16,GOLD);
                if(nearby_shop) DrawText(TextFormat("E / USE: %s",nearby_shop->name.c_str()),screen_width/2-160,screen_height-165,20,GOLD);
            }
        }
        if(shop_open && current_shop) play_ui.draw_shop(economy,*current_shop,loot_session.economy(),item_registry,loot_session.inventory(),shop_status,screen_width,screen_height);
        if(mode_session.terminal()) play_ui.draw_result(mode_session,screen_width,screen_height);
        if(map_screen.is_open()) {
            game::MapScreen::View view;
            view.player=player.position;view.yaw=player.yaw;view.font=&sign_renderer.font();
            if(game_mode==game::GameMode::BattleRoyale) view.circle_radius=mode_session.radius();
            for(const auto& stop:bus_line.stops()) view.stops.push_back(stop.shelter);
            if(bus_line.running()) for(const auto& bus:bus_line.buses()) if(bus.running) view.buses.push_back({bus.position,bus.yaw});
            map_screen.draw(verda_region,view,settings,screen_width,screen_height);
        }
        DrawFPS(
            screen_width - 90,
            10
        );

        if(perf.enabled()) {
            DrawRectangle(8,screen_height-102,std::min(screen_width-16,670),70,Fade(BLACK,.8F));
            DrawText(TextFormat("CPU ms: sim %.1f | terrain %.1f | grass %.1f | world %.1f | bodies %.1f",
                perf.average(FrameStage::Simulation),perf.average(FrameStage::Terrain),perf.average(FrameStage::Foliage),
                perf.average(FrameStage::World),perf.average(FrameStage::Characters)),16,screen_height-94,14,RAYWHITE);
            DrawText(TextFormat("peak ms: %.1f / %.1f / %.1f / %.1f / %.1f | terrain %d | grass work %d (max 24)",
                perf.peak(FrameStage::Simulation),perf.peak(FrameStage::Terrain),perf.peak(FrameStage::Foliage),
                perf.peak(FrameStage::World),perf.peak(FrameStage::Characters),terrain_world.chunk_count(),foliage_system.evaluations_last_frame()),16,screen_height-70,14,GOLD);
            DrawText("Submission timings only; compare FPS on device. OUTLAND_PERF=0 hides this panel.",16,screen_height-48,12,LIGHTGRAY);
            // The same numbers in the log every two seconds, for runs nobody is watching.
            static double next_log=0;
            if(GetTime()>=next_log) {
                next_log=GetTime()+2;
                TraceLog(LOG_INFO,"PERF fps %d frame %.1f ms scale %.2f models %d meshes %d [region %.2f assets %.2f cars %.2f signs %.2f] {bots %.2f npcs %.2f vehicles %.2f} | sim %.2f terrain %.2f grass %.2f world %.2f bodies %.2f",GetFPS(),GetFrameTime()*1000,render_scale.scale(),world_draw_stats().models,world_draw_stats().meshes,world_draw_stats().region,world_draw_stats().assets,world_draw_stats().cars,world_draw_stats().signs,world_draw_stats().bots,world_draw_stats().npcs,world_draw_stats().vehicles,
                    perf.average(FrameStage::Simulation),perf.average(FrameStage::Terrain),perf.average(FrameStage::Foliage),
                    perf.average(FrameStage::World),perf.average(FrameStage::Characters));
            }
        }
        last_submit_seconds=static_cast<float>(GetTime()-frame_start);
        EndDrawing();
    }
    if(scene_target.id!=0) UnloadRenderTexture(scene_target);
    static_world.invalidate();
#ifdef OUTLAND_DEV_TOOLS
    if(creator_session.dirty() && !creator_session.save(verda_region))TraceLog(LOG_ERROR,"Unsaved Creator edits: %s",creator_session.path().c_str());
#endif
    restore_match_fleet();
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
