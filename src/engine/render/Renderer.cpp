#include "outland/platform/Runtime.hpp"
#include "outland/engine/render/ViewFrustum.hpp"
#include "outland/engine/render/Renderer.hpp"
#include "outland/engine/audio/EnvironmentAudio.hpp"
#include "outland/game/combat/CombatRenderer.hpp"
#include "outland/game/combat/CombatFeedback.hpp"
#include "outland/engine/audio/GunAudio.hpp"
#include "outland/game/combat/Grenades.hpp"
#include "outland/game/combat/Sights.hpp"
#include "outland/game/build/BuildMode.hpp"
#include "outland/game/hud/Instruments.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/casino/PokerTable.hpp"
#include "outland/game/mafia/Mafio.hpp"
#include "outland/game/career/Career.hpp"
#include "outland/game/life/WalkingMan.hpp"
#include "outland/game/ranch/Ranches.hpp"
#include "outland/game/freight/Freight.hpp"
#include "outland/game/mafia/MafioRenderer.hpp"
#include "outland/game/services/EmergencyServices.hpp"
#include "outland/game/ZoneWall.hpp"
#include "outland/game/inventory/LootSave.hpp"
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
#include "outland/game/PauseMenu.hpp"
#include "outland/game/feel/Haptics.hpp"
#include "outland/game/transit/BusLine.hpp"
#include "outland/world/Zones.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/aircraft/HelicopterDrop.hpp"
#include "outland/game/aircraft/DropRenderer.hpp"
#include "outland/game/economy/Penthouse.hpp"
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
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/foliage/FoliageSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/physics/MeshCollision.hpp"

#include "outland/game/culture/SignRenderer.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/game/zombies/ZombieEcology.hpp"
#include "outland/game/LanScreen.hpp"
#include "outland/game/lan/LanPlay.hpp"
#include "outland/net/LanSession.hpp"
#include <random>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cstring>
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

// The Building Parts kit's walls, floors and roofs are single planes with no thickness, faced one
// way: drawn with back-face culling they vanish from the other side, so modular shops and houses
// look unbuilt from the street. Give each such mesh a back side (the same triangles, wound the
// other way, normals flipped).
void double_sided(Model& model) {
    for (int i = 0; i < model.meshCount; ++i) {
        Mesh& old = model.meshes[i];
        if (!old.vertices || old.vertexCount <= 0 || old.vertexCount * 2 > 65535) continue;
        const int vertices = old.vertexCount, triangles = old.triangleCount;
        Mesh mesh{};
        mesh.vertexCount = vertices * 2;
        mesh.triangleCount = triangles * 2;
        const auto copy = [&](const float* from, int components, bool flip) -> float* {
            if (!from) return nullptr;
            auto* out = static_cast<float*>(RL_MALLOC(sizeof(float) * static_cast<std::size_t>(components * vertices * 2)));
            for (int v = 0; v < vertices * components; ++v) { out[v] = from[v]; out[v + vertices * components] = flip ? -from[v] : from[v]; }
            return out;
        };
        mesh.vertices = copy(old.vertices, 3, false);
        mesh.normals = copy(old.normals, 3, true);
        mesh.texcoords = copy(old.texcoords, 2, false);
        mesh.texcoords2 = copy(old.texcoords2, 2, false);
        if (old.colors) {
            mesh.colors = static_cast<unsigned char*>(RL_MALLOC(static_cast<std::size_t>(4 * vertices * 2)));
            std::memcpy(mesh.colors, old.colors, static_cast<std::size_t>(4 * vertices));
            std::memcpy(mesh.colors + 4 * vertices, old.colors, static_cast<std::size_t>(4 * vertices));
        }
        mesh.indices = static_cast<unsigned short*>(RL_MALLOC(sizeof(unsigned short) * static_cast<std::size_t>(triangles * 6)));
        for (int t = 0; t < triangles; ++t)
            for (int corner = 0; corner < 3; ++corner) {
                const int index = old.indices ? old.indices[t * 3 + corner] : t * 3 + corner;
                mesh.indices[t * 3 + corner] = static_cast<unsigned short>(index);
                mesh.indices[(triangles + t) * 3 + (2 - corner)] = static_cast<unsigned short>(index + vertices);
            }
        UploadMesh(&mesh, false);
        UnloadMesh(old);
        model.meshes[i] = mesh;
    }
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
        if (path.find("/Building Parts/") != std::string::npos) double_sided(model);
        return model.meshCount > 0;
    });
}

void draw_model_backed_world_assets(
    const world::VerdaRegion& region,
    const Vector3& camera_position,
    const characters::CharacterRegistry& registry,
    const ViewFrustum& view,
    float reach = 1.0F
) {
    // `reach` stretches the distance for buildings (high above the island in the drop).
    const float max_distance = 320.0F * std::clamp(reach, 1.0F, 3.0F);
    const float max_distance_sq =
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
    TraceLog(LOG_INFO,pointer_events.installed() ? "Click capture enabled":"Click capture unavailable - using raylib input polling");
    input::InputSystem input_system;
    input::TouchHUD touch_hud;
    // Touch buttons on phones and touch screens; keyboard and mouse look everywhere else.
    const bool touch_screen=input::PointerEvents::touch_screen();
    bool touch_seen=false, mouse_was_captured=false, mouse_wanted=false;
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    constexpr bool can_quit_game=true;
#else
    constexpr bool can_quit_game=false;
#endif
    bool inventory_open = false;
    std::string interaction_message;
    float interaction_remaining = 0.0F;

    game::HomeScreen home_screen;
    // LAN play (desktop and Android): the session, its lobby screen, and the other people in the match.
    net::LanSession lan;
    game::LanScreen lan_screen;
    game::lan::LanPlay lan_play;
    bool lan_screen_open=false,lan_match=false,lan_restart=false,lan_death_sent=false,lan_host_lost=false,lan_fired=false;
    std::uint64_t lan_seed=0;
    float lan_state_wait=0,lan_snapshot_wait=0;
    std::int32_t lan_last_attacker=-1;   // who hurt the player last: a slot, or -bot id
#ifdef OUTLAND_DEV_TOOLS
    bool lan_dev_done=false;
#endif
    const std::string lan_name=[&]{
        if(const char* name=std::getenv("OUTLAND_LAN_NAME"); name && *name) return std::string(name).substr(0,16);
        return "LUDANTO "+std::to_string(10+static_cast<int>(std::random_device{}()%90));
    }();
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
    // In a LAN match the others see what the player takes and drops.
    loot_session.taken=[&](const std::string& pickup,int remaining) {
        if(lan_match && lan.in_match()) lan.send(net::Msg::Taken,net::Taken{pickup,static_cast<std::uint16_t>(std::max(0,remaining))});
    };
    loot_session.dropped=[&](const game::inventory::WorldPickup& pickup) {
        if(lan_match && lan.in_match())
            lan.send(net::Msg::Dropped,net::Dropped{pickup.id,pickup.item,static_cast<std::uint16_t>(std::max(0,pickup.quantity)),
                                                     static_cast<std::uint16_t>(std::max(0,pickup.loaded_rounds)),pickup.position});
    };
    game::inventory::LootUI loot_ui(character_root);
    characters::NpcSystem npcs;
    game::ai::BattleRoyaleBots br_bots;
    game::ai::BotDrivers bot_drivers; // Battle Royale bots on the road
    std::vector<int> bots_looted;      // fallen bots whose gear already lies on the ground
    // The Battle Royale drop (game::aircraft): the helicopters' lines (one each, all different
    // directions), the one the player rides, the player's jump and the bots' jumps. `drop_clock`
    // runs from the start of the match; while `dropping` the player rides, falls or glides instead
    // of walking.
    std::vector<game::aircraft::FlightPath> drop_fleet{game::aircraft::FlightPath{}};
    int drop_ride=0;
    constexpr int royale_bots=99;          // 100 players with you
    constexpr float switch_window=30.0F;   // seconds to change helicopter after the start
    game::aircraft::Skydiver skydiver;
    std::vector<game::aircraft::BotJump> bot_jumps;
    std::vector<char> bot_landed;
    float drop_clock=0;
    std::uint64_t drop_seed=1;
    Vector3 drop_camera{};
    int zone_seen_phase=-1;bool zone_seen_shrinking=false;float circle_banner_time=0;std::string circle_banner;
    bool drop_camera_ready=false;
    bool dropping=false, drop_flying=false, drop_drivers_pending=false;
    // Explore: La Mafio, a Krimulo crew in every town (bots in br_bots), and the Green Hand's
    // poker table in Verda City.
    game::mafia::Mafio mafio;
    // Explore: the player's life in a gang or on the police (respect, jobs, calls), saved beside
    // their loot profile. A join at a hideout or a station takes a second USE within a few seconds.
    game::career::Career career;
    std::string career_path,career_saved;
    float career_save_wait=0;
    bool career_dead=false;
    struct JoinOffer { int crew{-1}; std::string patrol_car; double until{-1}; } join_offer;
    game::casino::Poker poker;
    game::casino::PokerTable poker_table;
    bool poker_open=false;
    int poker_buy_in=0; // the wallet changes only when the player leaves (by the net win or loss)
    // Explore: the Policio's patrol cars and Hospitalo de Verda's ambulances.
    game::services::EmergencyServices services;
    bool respawn_at_hospital=false; // the player died in Explore: the ambulance took them in
    // Explore: the Stela Turo penthouse, for sale; once bought, home (sleep there, wake there).
    game::economy::Penthouse penthouse;
    bool was_in_penthouse=false;
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
    // Paul Acosta, "the walking man": Ruldomaro to the casino and the bar and home again (Explore).
    game::life::WalkingMan walking_man;
    // The Vakeroj on their ranches, their cattle and side-by-sides; the big rigs (Explore).
    game::ranch::Ranches ranches;
    game::freight::Freight freight;
    std::vector<characters::AnimationController> ranch_seats; // drivers' idle
    mafio.set_outsiders([&](int a,int b,double now){return walking_man.hostile_to(a,b,now) || ranches.hostile_to(a,b,now);});
    bot_drivers.reserve(std::string(game::life::WalkingMan::car_marker));
    bot_drivers.reserve("vehicle_spawn_sxs_");
    bot_drivers.reserve("vehicle_spawn_semi_");
    // A cattleman hat on a head (the crown from the body's pose): its band ~10 cm below the top.
    const auto draw_cowboy_hat=[&](const characters::HeldGun& head) {
        Model* hat=get_world_asset_model("assets/verda/ranch/cowboy_hat.glb",character_registry);
        if(!hat || !head.hat) return;
        const Vector3 up=Vector3Normalize(head.crown_up);
        Vector3 forward=Vector3Subtract(head.crown_forward,Vector3Scale(up,Vector3DotProduct(head.crown_forward,up)));
        if(Vector3Length(forward)<1e-3F) return;
        forward=Vector3Normalize(forward);
        const Vector3 right=Vector3CrossProduct(up,forward);
        // A little back from the head-top joint (heads are deeper behind it), sized for hair.
        constexpr float size=1.12F;
        const Vector3 at=Vector3Subtract(Vector3Subtract(head.crown,Vector3Scale(up,.125F)),Vector3Scale(forward,.025F));
        Matrix m=MatrixIdentity();
        m.m0=right.x*size;m.m1=right.y*size;m.m2=right.z*size;
        m.m4=up.x*size;m.m5=up.y*size;m.m6=up.z*size;
        m.m8=forward.x*size;m.m9=forward.y*size;m.m10=forward.z*size;
        m.m12=at.x;m.m13=at.y;m.m14=at.z;
        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(m));
        DrawModel(*hat,{0,0,0},1,WHITE);
        rlPopMatrix();
    };
    const auto krimulo_hostile=[&](int bot){return !mafio.active() || mafio.provoked(mafio.gang_of(bot),match_time) ||
        walking_man.hostile_to(bot,game::ai::BattleRoyaleBots::player_id,match_time) ||
        ranches.hostile_to(bot,game::ai::BattleRoyaleBots::player_id,match_time);};

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
        bot_drivers.clear(br_bots,vehicles);
        if(!match_fleet_active) return;
        vehicles.restore_checkpoint(verda_region,match_fleet);
        vehicle_dirty=fleet_was_dirty;match_fleet.clear();match_fleet_active=false;
    };
    float vehicle_save_timer=0,vehicle_reconcile_timer=0,vehicle_orbit=0,vehicle_pitch=0,vehicle_look_idle=0;
    if(FileExists(vehicle_state_path.c_str()) && !vehicles.load_state(verda_region,vehicle_state_path,vehicle_error)) {
        vehicle_save_protected=true;TraceLog(LOG_ERROR,"Vehicle state protected: %s",vehicle_error.c_str());
    }
#ifdef OUTLAND_DEV_TOOLS
    bool has_previous_location=false;
    Vector3 previous_location{};
#endif
    world::terrain::TerrainWorld terrain_world;
    world::foliage::FoliageSystem foliage_system;
    // Beside the executable on desktop; on Android the executable is the system's app process,
    // so the runtime tree is the working directory (platform::initialize) - use what exists.
    const auto audio_directory=[](const char* local){
        const std::string packaged=std::string(GetApplicationDirectory())+local;
        return DirectoryExists(packaged.c_str()) ? packaged : std::string(local);
    };
    audio::EnvironmentAudio environment_audio(audio_directory("assets/audio/environment"));
    // Gunfire you hear: your own reports, everyone else's placed in the world, grenade blasts,
    // reloads, rounds going past, hits, pickups, the zone and your heart when you are hurt.
    audio::GunAudio gun_audio(audio_directory("assets/audio/weapons"),audio_directory("assets/audio/environment"));
    float hurt_sound_wait=0,zone_sound_wait=0,heart_sound_wait=0;
    bool reload_sounding=false;
    auto equipped_weapon=game::combat::WeaponId::Rifle;
    std::unordered_map<int,double> footstep_heard; // last step played per source
    std::uint64_t gunfire_heard=0; // sound-bus serial already turned into audio

    // The island map (M / MAP) and the player's settings it carries.
    game::MapScreen map_screen;
    game::PauseMenu pause_menu;
    // The island bus line and its drivers' animation clocks.
    game::transit::BusLine bus_line;
    std::uint64_t bus_seed=0;
    // Explore's BUILD mode: the player's roads, homes and bus stops, saved per profile; buses
    // calling at the player's stops bring newcomers to the empty homes.
    game::build::PlayerBuilds player_builds;
    game::build::BuildMode build_mode;
    std::string builds_path;
    std::vector<float> bus_dwell_seen;
    std::array<characters::AnimationController,8> bus_drivers{};
    // Phone vibration for shots, hits, kills, crashes, blasts and hard landings.
    game::feel::Haptics haptics;
    game::GameSettings settings;
    const std::string settings_path=game::GameSettings::path_for(map_path);
    settings.load(settings_path);
    const auto apply_settings=[&]() {
        input_system.set_look({settings.look_sensitivity,settings.mouse_sensitivity,settings.invert_y,settings.aim_toggle});
        input_system.set_bindings(settings.bindings);
        input_system.set_button_size(settings.button_size);
        haptics.set_enabled(settings.vibration);
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
    game::Health player_health{game::player_max_health};
    // Armour from the bag: the best vest carried soaks part of every bullet, blast and bite.
    const auto armour_level=[&]{
        if(game_mode==game::GameMode::DevLab) return 0;
        const auto& bag=loot_session.inventory();
        return bag.count("vest_heavy")>0 ? 2 : bag.count("vest_light")>0 ? 1 : 0;
    };
    const auto hurt_player=[&](float amount){
        static constexpr float soak[]{0.0F,.30F,.45F};
        if(amount<=0) return;
        player_health.damage(amount*(1-soak[armour_level()]));
        if(amount>=1 && hurt_sound_wait<=0) {gun_audio.hurt(armour_level()>0);hurt_sound_wait=.15F;}
        haptics.hit_taken(amount);
    };
    game::vehicles::VehicleAudio vehicle_audio;
    vehicles.bind_occupant_damage([&](float amount){player_health.damage(amount);});
    combat_world.bind_vehicles([&](Vector3 start,Vector3 end,bool occupants){return vehicles.trace(verda_region,start,end,occupants);},[&](const game::combat::BulletHit& hit,float amount){
        vehicles.damage(verda_region,hit,amount);vehicle_dirty=true;
        // Through the glass into a bot at the wheel.
        if((hit.kind==game::combat::HitKind::VehicleWindow || hit.kind==game::combat::HitKind::VehicleOccupant) &&
           hit.target>=0 && static_cast<std::size_t>(hit.target)<vehicles.vehicles().size()) {
            const int driver=bot_drivers.driver_of(vehicles.vehicles()[static_cast<std::size_t>(hit.target)].id);
            if(driver>=0 && mafio.active()) {mafio.provoke(driver,match_time);walking_man.provoke(driver,match_time);ranches.provoke(driver,match_time);}
            if(driver>=0 && br_bots.damage_bot(driver,amount*.8F,hit.position,match_time)) {
                pending_kills.push_back(hit.position);
                career.killed(mafio.active() ? mafio.gang_of(driver) : -1,vehicles.driver()!=nullptr);
            }
        }
    });
    // The player's damage to a person (bullets and grenades): bots, residents, police, zombies.
    // Returns true when it kills. Hurting a resident is a crime if anyone saw it.
    const auto damage_actor=[&](int actor,float damage,Vector3 attacker) -> bool {
        if(actor<0 || static_cast<std::size_t>(actor)>=npcs.actors().size()) return false;
        // Battle Royale bodies belong to their bot brains; everyone else is an NpcSystem actor.
        const auto& body=npcs.actors()[static_cast<std::size_t>(actor)];
        const Vector3 where=body.position;
        if(lan_match && game::lan::LanPlay::is_remote_body(body.bot)) {
            // Another player: the hit is theirs to take, and their death theirs to tell.
            if(lan.in_match()) lan.send(net::Msg::Hit,net::Hit{static_cast<std::int32_t>(lan.slot()),damage,attacker,static_cast<std::uint8_t>(weapons.selected())},
                                        game::lan::LanPlay::slot_of(body.bot));
            return false;
        }
        if(lan_match && lan.active() && !lan.hosting() && body.bot>=0 && game_mode==game::GameMode::BattleRoyale) {
            // A bot: the host runs them, and says whether this killed.
            if(lan.in_match()) lan.send(net::Msg::BotHit,net::BotHit{static_cast<std::uint16_t>(body.bot),damage,attacker},0);
            return false;
        }
        bool killed=false;
        if(body.bot>=0) {
            if(mafio.active()) mafio.provoke(body.bot,match_time); // hurt a Krimulo: the gang shoots back
            walking_man.provoke(body.bot,match_time);             // or Paul Acosta: so does he
            ranches.provoke(body.bot,match_time);                 // or a Vakero: the whole family
            killed=br_bots.damage_bot(body.bot,damage,attacker,match_time);
        }
        else if(npcs.damage(static_cast<std::size_t>(actor),damage,attacker)) {
            const auto& victim=npcs.actors()[static_cast<std::size_t>(actor)];
            killed=victim.health<=0;
            if(victim.resident>=0) crimes.push_back({victim.police ? game::law::CrimeKind::AttackOnPolice :
                victim.health<=0 ? game::law::CrimeKind::Murder : game::law::CrimeKind::Assault,victim.resident});
        }
        if(killed) pending_kills.push_back(where);
        if(killed && body.bot>=0) career.killed(mafio.active() ? mafio.gang_of(body.bot) : -1,vehicles.driver()!=nullptr);
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
    float fp_aim_blend = 0.0F, crouch_blend = 0.0F, ads_view_blend = 0.0F;
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

        // The pause menu stops the world (a sliver of time keeps every rate finite).
        const float dt =
            pause_menu.is_open() && !lan_match ? 1e-5F :
            std::min(
                GetFrameTime(),
                0.05F
            );
        if(GetTouchPointCount()>0) touch_seen=true;
        const bool touch_ui=settings.touch_controls==game::GameSettings::TouchControls::On ||
            (settings.touch_controls==game::GameSettings::TouchControls::Auto && (touch_screen || touch_seen));
        settings.mouse_look=!touch_ui;
        input_system.set_touch_controls(touch_ui);
        const bool mouse_captured=!touch_ui && input::PointerEvents::mouse_captured();
        input_system.set_mouse_look(mouse_captured,input::PointerEvents::mouse_motion());

        const int screen_width =
            GetScreenWidth();

        const int screen_height =
            GetScreenHeight();

        const Rectangle audio_button{static_cast<float>(screen_width - 174), 44.0F, 162.0F, 34.0F};
        if (!map_screen.is_open() && !pause_menu.is_open() && IsKeyPressed(KEY_N)) {
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
            // ESC on the home screen quits a PC game; a browser tab or a phone app is closed by its own controls.
            if (can_quit_game && !lan_screen_open && IsKeyPressed(KEY_ESCAPE)) break;
            input::PointerEvents::capture_mouse(false);mouse_wanted=mouse_was_captured=false;
            if(pause_menu.is_open()) pause_menu.close();
            input_system.update(screen_width, screen_height, true, true);
#ifdef OUTLAND_DEV_TOOLS
            creator_controller.set_enabled(false);
            creator_touch_ui.update(creator_controller, screen_width, screen_height, true);
#endif
            environment_audio.update(player.position, player.grounded, false, verda_region);
            game::GameMode selected=game::GameMode::Home;
            lan.poll(GetTime());
            std::optional<net::Start> lan_start;
#ifdef OUTLAND_DEV_TOOLS
            // OUTLAND_DEV_LAN=host[:explore|:zombie] or join: through the LAN screen unattended (two games on one
            // machine); the host starts once OUTLAND_DEV_LAN_PLAYERS (2) are in the lobby.
            if(const char* dev_lan=std::getenv("OUTLAND_DEV_LAN"); dev_lan && *dev_lan && !lan_dev_done) {
                const std::string want=dev_lan;
                if(!lan_screen_open) {lan_screen_open=true;lan.browse();}
                if(want.rfind("host",0)==0) {
                    const auto mode=want.find("explore")!=std::string::npos ? game::GameMode::Explore :
                                    want.find("zombie")!=std::string::npos ? game::GameMode::ZombieSurvival : game::GameMode::BattleRoyale;
                    const char* count=std::getenv("OUTLAND_DEV_LAN_PLAYERS");
                    if(!lan.active()) lan.host(lan_name,static_cast<std::uint8_t>(mode),static_cast<std::uint8_t>(home_screen.bot_level()),GetTime());
                    else if(static_cast<int>(lan.roster().players.size())>=(count ? std::max(1,std::atoi(count)) : 2)) {lan.start_match(424242);lan_dev_done=true;}
                } else if(!lan.active() && !lan.hosts().empty()) {lan.join(lan.hosts().front(),lan_name,GetTime());lan_dev_done=true;}
            }
#endif
            if(lan_screen_open) {
                // LAN PLAY: host or join, then the lobby until the host starts.
                using Kind=game::LanScreen::Kind;
                const auto action=lan_screen.update(screen_width,screen_height,lan);
                const auto lobby=lan.roster();
                switch(action.kind) {
                case Kind::Back: lan.stop();lan_screen_open=false;break;
                case Kind::Leave: lan.stop();lan.browse();break;
                case Kind::Host:
                    lan.host(lan_name,static_cast<std::uint8_t>(game::GameMode::BattleRoyale),static_cast<std::uint8_t>(home_screen.bot_level()),GetTime());
                    break;
                case Kind::Join:
                    if(action.index>=0 && action.index<static_cast<int>(lan.hosts().size()))
                        lan.join(lan.hosts()[static_cast<std::size_t>(action.index)],lan_name,GetTime());
                    break;
                case Kind::CycleMode: {
                    const auto mode=static_cast<game::GameMode>(lobby.mode);
                    const auto next=mode==game::GameMode::BattleRoyale ? game::GameMode::ZombieSurvival :
                                    mode==game::GameMode::ZombieSurvival ? game::GameMode::Explore : game::GameMode::BattleRoyale;
                    lan.set_lobby(static_cast<std::uint8_t>(next),lobby.level);
                    break;
                }
                case Kind::CycleLevel: lan.set_lobby(lobby.mode,static_cast<std::uint8_t>((lobby.level+1)%3));break;
                case Kind::Start: lan.start_match((static_cast<std::uint64_t>(std::random_device{}())<<20)+static_cast<std::uint64_t>(GetTime()*1000.0)+1);break;
                default: break;
                }
                lan_start=lan.take_start();
                if(lan_start) lan_screen_open=false;
            } else {
                selected=queued_mode!=game::GameMode::Home ? queued_mode : home_screen.update(screen_width,screen_height);
                if(home_screen.take_lan()) {lan_screen_open=true;lan.browse();}
            }
            queued_mode=game::GameMode::Home;
            if(lan_start) {
                lan_match=true;lan_seed=lan_start->seed;
                home_screen.set_bot_level(static_cast<game::ai::BotLevel>(std::min<int>(lan_start->level,2)));
                selected=static_cast<game::GameMode>(lan_start->mode);
                if(selected!=game::GameMode::ZombieSurvival && selected!=game::GameMode::Explore) selected=game::GameMode::BattleRoyale;
            } else if(selected!=game::GameMode::Home && !lan_restart) {
                // An ordinary match: done with LAN.
                if(lan_match || lan.active()) lan.stop();
                lan_match=false;
            }
            if(selected!=game::GameMode::Home) lan_restart=false;
            BeginDrawing();
            if(lan_screen_open) lan_screen.draw(screen_width,screen_height,lan,lan_name);
            else home_screen.draw(screen_width,screen_height);
            EndDrawing();

            if (
                selected !=
                game::GameMode::Home
            ) {
                game_mode =
                    selected;

                bool explicit_spawn=false;
                // A LAN match: one seed for everyone (the same towns, loot, bots, helicopters and zone).
                const auto session_seed=lan_match ? lan_seed : static_cast<std::uint64_t>(GetTime()*1000.0)+1;
                player.position=Vector3Add(game::session_spawn(verda_region,game_mode,session_seed),{0,1,0});
                player.yaw=player.pitch=0;

                player.vertical_velocity =
                    0.0F;

                player.grounded =
                    true;

#ifdef OUTLAND_DEV_TOOLS
                // Dev builds: OUTLAND_DEV_SPAWN="x,z,yaw_degrees[,height]" starts every mode at that spot
                // (screenshots, checking a far corner of the island without a long walk).
                if(const char* spawn=std::getenv("OUTLAND_DEV_SPAWN")) {
                    // An optional fourth value: look for the floor below that height (an upper storey).
                    float x=0,z=0,yaw_degrees=0,height=-1e9F;
                    if(std::sscanf(spawn,"%f,%f,%f,%f",&x,&z,&yaw_degrees,&height)>=3 && std::isfinite(x) && std::isfinite(z) && std::isfinite(yaw_degrees)) {
                        const float ground=world::terrain::TerrainHeight::sample(x,z);
                        const float probe=std::isfinite(height) && height>ground ? height : ground+.5F;
                        player.position={x,world::physics::WorldCollision::ground_height({x,probe,z},probe,verda_region)+1.0F,z};
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
                    // A backpack first: every gun does not fit in the bare bag.
                    if(loot_session.inventory().count("backpack")==0) loot_session.edit_inventory().add(item_registry,"backpack",1);
                    loot_session.edit_inventory().equip_container(item_registry,"backpack");
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
                if(freight.running()) freight.stop(verda_region,vehicles);
                ranches.clear(vehicles);
                vehicles.reconcile(verda_region);
                if(game_mode==game::GameMode::BattleRoyale) {
                    match_fleet=vehicles.checkpoint(verda_region);match_fleet_active=true;fleet_was_dirty=vehicle_dirty;
                    vehicles.reset_match_fleet(verda_region);
                }
                npcs.reset_session();
                lan_play.clear();lan_state_wait=lan_snapshot_wait=0;lan_last_attacker=-1;lan_death_sent=lan_host_lost=false;
                loot_session.drop_prefix=lan_match ? "drop_s"+std::to_string(lan.slot())+"_" : std::string{};
                build_mode.set_active(false);bus_dwell_seen.clear();
                if(game::rules_for(game_mode).civilian_life) {
                    builds_path=game::build::PlayerBuilds::path_beside(game::inventory::LootSave::profile_path(map_path,game_mode));
                    std::string builds_error;
                    if(!player_builds.load(builds_path,builds_error)) {TraceLog(LOG_WARNING,"Player builds: %s",builds_error.c_str());player_builds.clear();}
                } else player_builds.clear();
                player_builds.apply(verda_region); // other modes take the player's town away again
                world::physics::MeshCollisionLibrary::preload(verda_region);
                // Explore lives on Verda: rebuild residents from the current (possibly Creator-edited) towns.
                if(game::rules_for(game_mode).civilian_life) {
                    island_life.build(verda_region,&character_registry);
                    // Families who came by bus to the player's homes in earlier sessions.
                    for(const auto& piece:player_builds.placed()) if(piece.home() && piece.residents>0)
                        island_life.move_in(player_builds.building(piece),piece.residents,static_cast<std::uint64_t>(piece.serial)*7919U);
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
                if(poker_open) {
                    loot_session.economy().money=std::max<std::int64_t>(0,loot_session.economy().money+poker.cash_out()-poker_buy_in);
                    poker_open=false;poker_buy_in=0;
                }
                walking_man.clear();
                career=game::career::Career{};career_path.clear();mafio.set_player_gang(-1);services.assign_player_car({});
                mafio.clear(br_bots,bot_drivers,vehicles);
                services.clear(vehicles,npcs);
                bot_drivers.clear(br_bots,vehicles);bots_looted.clear();
                br_bots.clear();sound_bus.clear();nav_grid.clear(); // the map may have been edited
                zombie_hour=17.5F;lit_asset_count=static_cast<std::size_t>(-1);
                justice.reset(nullptr,nullptr);crimes.clear();police_tracers.clear();
                rebuild_signs();static_world.invalidate();
                match_time=0;shop_open=false;current_shop=nullptr;
                last_player_feet={player.position.x,player.position.y-1.0F,player.position.z};
                zone_seen_phase=-1;circle_banner_time=0;
                dropping=drop_flying=drop_drivers_pending=false;bot_jumps.clear();bot_landed.clear();drop_clock=0;skydiver.board();
                bool helicopter_drop=game_mode==game::GameMode::BattleRoyale;
#ifdef OUTLAND_DEV_TOOLS
                // OUTLAND_DEV_NO_DROP=1: start Battle Royale on the ground, as before the helicopters.
                if(const char* no_drop=std::getenv("OUTLAND_DEV_NO_DROP"); (no_drop && *no_drop=='1') || explicit_spawn) helicopter_drop=false;
#endif
                if(helicopter_drop) {
                    // Everyone rides one of the helicopters, each on its own route across the island,
                    // and jumps. The bots land within gliding reach of a line; each is "mounted"
                    // (hidden, inert) until it touches down.
                    drop_fleet=game::aircraft::make_fleet(session_seed);drop_seed=session_seed;
                    drop_ride=static_cast<int>(session_seed%drop_fleet.size());
                    const auto& fleet=drop_fleet;
                    br_bots.start(verda_region,home_screen.bot_level(),royale_bots,session_seed,{1e6F,0,1e6F},[&fleet](Vector3 feet) {
                        return std::any_of(fleet.begin(),fleet.end(),[&](const auto& path) {
                            const float along=path.along(feet);
                            return std::abs(path.offset(feet))<game::aircraft::drop_reach*.85F && along>path.land_from && along<path.land_to;
                        });
                    });
                    std::vector<std::pair<int,Vector3>> landings;
                    for(const auto& bot:br_bots.bots()) landings.emplace_back(bot.id(),bot.position());
                    bot_jumps=game::aircraft::plan_bot_jumps(drop_fleet,landings,session_seed);
                    bot_landed.assign(bot_jumps.size(),0);
                    for(const auto& jump:bot_jumps) br_bots.set_mounted(jump.bot,true);
                    br_bots.sync_bodies(npcs,&character_registry);
                    dropping=drop_flying=drop_drivers_pending=true;
                    const auto& ride=drop_fleet[static_cast<std::size_t>(drop_ride)];
                    player.position=game::aircraft::helicopter_door(ride,0);
                    player.yaw=std::atan2(ride.direction.x,ride.direction.z);player.pitch=-.3F;
                    player.grounded=false;player.vertical_velocity=0;player.third_person=true;
                    last_player_feet={player.position.x,player.position.y-1.0F,player.position.z};
                } else if(game::rules_for(game_mode).combat_bots) {
                    br_bots.start(verda_region,home_screen.bot_level(),royale_bots,session_seed,last_player_feet);
                    // Some of them find a car near where they land and drive in toward the zone.
                    bot_drivers.start(br_bots,vehicles,verda_region,session_seed,mode_session.circle.start_radius,14);
                    br_bots.sync_bodies(npcs,&character_registry);
                }
                if(game::rules_for(game_mode).civilian_life && !island_life.empty()) {
                    // La Mafio: every town's crew moves into its building.
                    mafio.start(island_life.places(),verda_region,br_bots,session_seed);
#ifdef OUTLAND_DEV_TOOLS
                    for(const auto& crew:mafio.crews()) TraceLog(LOG_INFO,"OUTLAND crew %s at %s: door %.1f %.1f yaw %.0f",
                        std::string(game::mafia::gangs[static_cast<std::size_t>(crew.gang)].name).c_str(),crew.town.c_str(),crew.door.x,crew.door.z,crew.yaw);
#endif
                    if(const auto* casino=mafio.casino();casino &&
                       walking_man.start(island_life.places(),verda_region,casino->door,casino->table,br_bots,vehicles,session_seed,island_life.clock())) {
                        // His own face, not a gang member's.
                        const Vector3 at=walking_man.position(br_bots);
                        npcs.spawn_bot(walking_man.bot(),game::life::WalkingMan::character,at,0);
                    }
                    // The Vakeroj on their ranches (each in their own clothes and hat), then the big rigs.
                    ranches.start(verda_region,br_bots,vehicles,&character_registry,session_seed,island_life.clock());
                    for(const auto& v:ranches.vakeros())
                        if(const auto* bot=br_bots.find(v.bot);bot && !v.character.empty()) npcs.spawn_bot(v.bot,v.character,bot->position(),bot->yaw());
                    if(freight.build(verda_region,vehicles,bus_line,&ranches,&character_registry,session_seed)) freight.start(verda_region,vehicles);
                    TraceLog(LOG_INFO,"OUTLAND ranches: %d ranches, %d Vakeroj, %d cattle rigs/supply rigs %d, %d trailer reversals",
                             static_cast<int>(ranches.ranches().size()),static_cast<int>(ranches.vakeros().size()),
                             static_cast<int>(freight.rigs().size()),freight.running() ? 1 : 0,freight.reversals());
                    br_bots.sync_bodies(npcs,&character_registry);
                    walking_man.dress(npcs);
                    services.start(vehicles,verda_region,&character_registry);
#ifdef OUTLAND_DEV_TOOLS
                    for(const auto& u:services.units()) if(u.kind==game::services::UnitKind::Police)
                        TraceLog(LOG_INFO,"OUTLAND police car %s at %.1f %.1f",u.vehicle.c_str(),u.home.x,u.home.z);
#endif
                    // The player's career: gang or badge, from the last session.
                    career=game::career::Career{};
                    career_path=game::inventory::LootSave::profile_path(map_path,game_mode)+".career";
                    if(FileExists(career_path.c_str())) if(char* text=LoadFileText(career_path.c_str())) {
                        if(!career.parse(text)) TraceLog(LOG_WARNING,"Career: unreadable %s",career_path.c_str());
                        UnloadFileText(text);
                    }
                    career_saved=career.serialise();career_dead=false;join_offer={};
                    if(career.state().gang>=static_cast<int>(game::mafia::gangs.size())) career=game::career::Career{};
#ifdef OUTLAND_DEV_TOOLS
                    // OUTLAND_DEV_CAREER=gang:N or police: start this session in a gang or on the force.
                    if(const char* dev=std::getenv("OUTLAND_DEV_CAREER"); dev && *dev) {
                        std::string why;const std::string want=dev;
                        career=game::career::Career{};
                        if(want.rfind("gang:",0)==0) {
                            const int gang=std::clamp(std::atoi(want.c_str()+5),0,static_cast<int>(game::mafia::gangs.size())-1);
                            career.join_gang(gang,std::string(game::mafia::gangs[static_cast<std::size_t>(gang)].name),why);
                        } else if(want=="police") {
                            const Vector3 here{player.position.x,player.position.y-1,player.position.z};
                            const game::services::Unit* best=nullptr;
                            for(const auto& u:services.units()) if(u.kind==game::services::UnitKind::Police &&
                                (!best || Vector3Distance(u.home,here)<Vector3Distance(best->home,here))) best=&u;
                            if(best) career.join_police(best->vehicle,why);
                        }
                    }
#endif
                    mafio.set_player_gang(career.state().side==game::career::Side::Gang ? career.state().gang : -1);
                    services.assign_player_car(career.on_duty() ? career.state().patrol_car : std::string());
                    // Back from the dead: out of the hospital's front door.
                    penthouse.locate(verda_region);was_in_penthouse=false;
                    if(respawn_at_hospital && penthouse.found() && game::economy::Penthouse::owned(loot_session.economy())) {
                        // Owners wake up at home, in the penthouse, mended.
                        const auto bed=penthouse.to_world(game::economy::Penthouse::wake);
                        player.position={bed.x,bed.y+1,bed.z};player.yaw=0;
                        interaction_message="You wake up at home in the Stela Turo penthouse";interaction_remaining=5;
                    } else if(respawn_at_hospital && services.has_hospital()) {
                        const auto door=services.hospital_door();
                        player.position={door.x,world::physics::WorldCollision::ground_height({door.x,door.y+1.2F,door.z},door.y+1.2F,verda_region)+1,door.z};
                        interaction_message="An ambulance brought you to Hospitalo de Verda";interaction_remaining=5;
                    }
                }
                respawn_at_hospital=false;
                mode_session.start(game_mode,static_cast<int>(br_bots.bots().size()),home_screen.bot_level());
                if(lan_match) mode_session.add_participants(std::max(0,static_cast<int>(lan.roster().players.size())-1));
#ifdef OUTLAND_DEV_TOOLS
                // OUTLAND_DEV_ZONE=<metres>: start the Battle Royale circle smaller, to see its wall up close.
                if(const char* zone=std::getenv("OUTLAND_DEV_ZONE")) mode_session.circle.start_radius=std::max(30.0F,static_cast<float>(std::atof(zone)));
#endif
                // Where each circle will close: random, inside the last, centred on land.
                if(game_mode==game::GameMode::BattleRoyale)
                    mode_session.plan_circles(session_seed,[](float x,float z){
                        return world::terrain::TerrainHeight::sample(x,z)>world::layout::sea_level+4;});
                player_health.reset();
                health_seen=player_health.current();hurt_directional=false;pending_kills.clear();
                combat_feedback.reset();
                grenades.reset(game_mode==game::GameMode::DevLab ? 99 : 0); // elsewhere the bag holds them
                sight_mounts.clear();sight_seen.fill(0);
                recoil_pitch = recoil_yaw = recoil_debt = recoil_settle = 0.0F;
                trigger_ready = false;
                inventory_open = false;
                interaction_remaining = 0;
                // On foot in a LAN match, everyone starts a few steps apart rather than inside each other.
                if(lan_match && lan.slot()>0 && !dropping) {
                    const float angle=static_cast<float>(lan.slot())*.785F;
                    const float x=player.position.x+std::cos(angle)*2.5F,z=player.position.z+std::sin(angle)*2.5F,top=player.position.y+.2F;
                    const Vector3 feet{x,world::physics::WorldCollision::ground_height({x,top,z},top,verda_region),z};
                    if(std::isfinite(feet.y) && std::abs(feet.y-(player.position.y-1))<1.5F &&
                       !world::physics::WorldCollision::body_blocked(feet,feet.y,verda_region,.4F)) player.position=Vector3Add(feet,{0,1,0});
                }
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
        // PAUSE MENU AND RETURN HOME
        // ====================================================

        // Back to the home screen, saving first (false if the save failed and play goes on).
        const auto go_home=[&]() {
#ifdef OUTLAND_DEV_TOOLS
            if(creator_session.dirty() && !creator_session.save(verda_region)) return false;
#endif
            if(game_mode!=game::GameMode::DevLab && !loot_session.save(weapons) && loot_session.can_persist()){
                interaction_message=loot_session.status();interaction_remaining=3;return false;
            }
            restore_match_fleet();
            vehicles.leave_session();vehicle_audio.stop();vehicle_renderer.reset_camera();
            shop_open=inventory_open=false;
            if(lan_match) {lan.stop();lan_play.clear(&npcs);lan_match=false;}
            game_mode=game::GameMode::Home;
            input_system.cancel_controls();
            input::PointerEvents::capture_mouse(false);mouse_wanted=mouse_was_captured=false;
            return true;
        };
        // ESC, the MENU button, or the browser taking the mouse back (its ESC) pauses. ESC first
        // closes the poker table, the map, the bag or a shop.
        {
            const bool lost_mouse=mouse_wanted && mouse_was_captured && !mouse_captured;
            mouse_was_captured=mouse_captured;
            const bool escape=IsKeyPressed(KEY_ESCAPE);
            const bool menu_tapped=!pause_menu.is_open() && pause_menu.tapped_menu_button(screen_width,screen_height,IsWindowFocused());
            if(!pause_menu.is_open() && !poker_open && !map_screen.is_open()) {
                if(escape && inventory_open) {inventory_open=false;input_system.cancel_controls();}
                else if(escape && shop_open) {shop_open=false;input_system.cancel_controls();}
                else if(escape && build_mode.active()) {}
                else if(escape || lost_mouse || menu_tapped) {pause_menu.open();input_system.cancel_controls();}
            }
        }
        if(pause_menu.is_open()) {
            settings.volume=environment_audio.volume();settings.muted=environment_audio.muted();
            const auto result=pause_menu.update(settings,screen_width,screen_height,IsWindowFocused(),GetFrameTime(),can_quit_game);
            if(result==game::PauseMenu::Result::SettingsChanged) {apply_settings();store_settings();}
            if(result==game::PauseMenu::Result::Resume) input_system.cancel_controls();
            if(result==game::PauseMenu::Result::Save) {
                bool saved=game_mode==game::GameMode::DevLab || loot_session.save(weapons) || !loot_session.can_persist();
                if(vehicle_dirty && !vehicle_save_protected && vehicles.save_state(verda_region,vehicle_state_path,vehicle_error)) vehicle_dirty=false;
                if(!builds_path.empty() && game::rules_for(game_mode).civilian_life) {
                    std::string builds_error;
                    if(!player_builds.save(builds_path,builds_error)) {saved=false;TraceLog(LOG_WARNING,"Player builds: %s",builds_error.c_str());}
                }
                store_settings();
                pause_menu.set_status(saved ? "GAME SAVED" : "SAVE FAILED - "+loot_session.status());
            }
            if(result==game::PauseMenu::Result::QuitHome || result==game::PauseMenu::Result::QuitGame) {
                if(!go_home()) {pause_menu.open();pause_menu.set_status("COULD NOT SAVE - "+interaction_message);continue;}
                if(result==game::PauseMenu::Result::QuitGame) break;
                // Finish this frame so the Home screen does not reuse the same click.
                BeginDrawing();
                home_screen.draw(screen_width, screen_height);
                EndDrawing();
                continue;
            }
        } else pause_menu.update(settings,screen_width,screen_height,false,GetFrameTime(),can_quit_game);

        if(mode_session.terminal()) {
            const auto action=play_ui.result_input(screen_width,screen_height,IsWindowFocused());
            if(action!=game::ResultAction::None) {
                if(!loot_session.save(weapons) && loot_session.can_persist()) {
                    interaction_message=loot_session.status();interaction_remaining=3;
                } else {
                    queued_mode=action==game::ResultAction::Restart ? game_mode : game::GameMode::Home;
                    respawn_at_hospital=action==game::ResultAction::Restart && game_mode==game::GameMode::Explore && !player_health.alive();
                    if(lan_match) {
                        const bool again=action==game::ResultAction::Restart && lan.in_match();
                        if(again && game_mode!=game::GameMode::BattleRoyale) lan_restart=true; // back into the same world, with the others
                        else if(again) {queued_mode=game::GameMode::Home;lan.end_match();lan_screen_open=true;lan_match=false;} // the lobby, for the next one
                        else {lan.stop();lan_match=false;}
                        lan_play.clear(&npcs);
                    }
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
        if(!map_screen.is_open() && !pause_menu.is_open() && game_mode!=game::GameMode::DevLab && !shop_open && !poker_open && !inventory_open && !mode_session.terminal() &&
           (map_screen.tapped_map_button(screen_width,screen_height,IsWindowFocused()) || (IsWindowFocused() && settings.bindings.pressed(input::Action::Map,mouse_captured)))) {
            map_screen.open();input_system.cancel_controls();
        }
        if(map_screen.is_open() && mode_session.terminal()) map_screen.close();
        // The equipment panel is modal: BAG closes it and GUN changes equipment.
        if(pause_menu.is_open()) {
            input_system.update(screen_width,screen_height,true,true,[](Vector2){return true;});
            input_system.player()={};
        } else if(map_screen.is_open()) {
            input_system.update(screen_width,screen_height,true,true,[](Vector2){return true;});
            input_system.player()={};
            settings.volume=environment_audio.volume();settings.muted=environment_audio.muted();
            const auto change=map_screen.update(settings,screen_width,screen_height,IsWindowFocused());
            if(change!=game::MapScreen::Change::None) {apply_settings();store_settings();}
            if(change==game::MapScreen::Change::Close) input_system.cancel_controls();
        } else if(shop_open || poker_open || mode_session.terminal()) {
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
            const bool town_building=game::rules_for(game_mode).civilian_life;
            if(town_building) reserved=[&,base=reserved](Vector2 point){return base(point) || build_mode.owns_point(point,screen_width,screen_height);};
            input_system.set_driving(vehicles.driver()!=nullptr); // steer left, pedals right, pan above
            {
                const bool armed_at_wheel=vehicles.driver()!=nullptr && weapons.available() &&
                    (weapons.selected()==game::combat::WeaponId::Pistol || weapons.selected()==game::combat::WeaponId::Smg);
                input_system.set_drive_by(armed_at_wheel);touch_hud.set_drive_by(armed_at_wheel);
            }
            input_system.set_look_stick(creator_active);
            input_system.update(screen_width, screen_height, !creator_active && !build_mode.active(),
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
        {
            // Mouse look: the pointer is captured while playing and freed for every menu and panel.
            bool dev_ui=false;
#ifdef OUTLAND_DEV_TOOLS
            dev_ui=game_mode==game::GameMode::DevLab && (dev_lab.tools_open() || creator_active);
#endif
            const bool want=!touch_ui && IsWindowFocused() && !pause_menu.is_open() && !map_screen.is_open() && !inventory_open &&
                !shop_open && !poker_open && !mode_session.terminal() && !dev_ui;
            input::PointerEvents::capture_mouse(want);
            mouse_wanted=want;
            build_mode.set_mouse_aim(mouse_captured);
        }
        // The drop: the stick steers the jump and JUMP (or USE) leaves the helicopter, then opens
        // the parachute; everything else waits for the ground (looking round still works).
        game::aircraft::DiveInput dive_input;
        bool drop_action=false,drop_switch=false;
        if(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed) {
            auto& held=input_system.player();
            dive_input.forward=-held.move_y;dive_input.strafe=held.move_x;
            drop_action=held.jump || held.interact;
            drop_switch=held.reload;
            const float dx=held.look_dx,dy=held.look_dy,lx=held.look_x,ly=held.look_y;
            held={};
            held.look_dx=dx;held.look_dy=dy;held.look_x=lx;held.look_y=ly;
        }
        const input::PlayerInput& controls = input_system.player();
        {
            // BUILD (K): lay roads, homes and bus stops in Explore, on foot.
            const bool can_build=game::rules_for(game_mode).civilian_life && !vehicles.driver() && !map_screen.is_open() &&
                !inventory_open && !shop_open && !poker_open && !mode_session.terminal() && player_health.alive();
            build_mode.tick(dt);
            bool toggled_now=false;
            if(can_build && build_mode.toggled(screen_width,screen_height,IsWindowFocused())) {
                build_mode.set_active(!build_mode.active());input_system.cancel_controls();toggled_now=true;
            }
            if(!can_build && build_mode.active()) build_mode.set_active(false);
            if(build_mode.active() && !toggled_now) {
                const auto change=build_mode.update(verda_region,player_builds,camera.position,Vector3Subtract(camera.target,camera.position),
                    {player.position.x,player.position.y-1,player.position.z},screen_width,screen_height,IsWindowFocused());
                if(change.changed) {
                    player_builds.apply(verda_region);
                    world::physics::MeshCollisionLibrary::preload(verda_region);
                    nav_grid.clear();rebuild_signs();static_world.invalidate();
                    if(change.transit && bus_line.running()) {
                        // New stops and streets join Linio 1: the line is laid out again.
                        bus_line.stop(verda_region,vehicles);
                        if(bus_line.build(verda_region,vehicles,&character_registry,++bus_seed)) bus_line.start(verda_region,vehicles);
                        bus_dwell_seen.clear();
                    }
                    std::string builds_error;
                    if(!player_builds.save(builds_path,builds_error)) TraceLog(LOG_WARNING,"Player builds: %s",builds_error.c_str());
                }
            }
        }
        vehicle_reconcile_timer+=dt;
        if(vehicle_reconcile_timer>=.5F){vehicle_dirty|=vehicles.reconcile(verda_region);vehicle_reconcile_timer=0;}
        const Vector3 shop_feet{player.position.x,player.position.y-1,player.position.z};
        const auto reachable_shop=[&](Vector3 door){return !combat_world.trace_segment(
            {player.position.x,player.position.y+.5F,player.position.z},Vector3Add(door,{0,.8F,0}),false,false).hit();};
        const auto* nearby_shop=game_mode==game::GameMode::Explore && !vehicles.driver() ? economy.nearest(shop_feet,reachable_shop) : nullptr;
        // The Green Hand's poker table in Verda City: sit down with up to 300 kr.
        const auto* table_crew=game_mode==game::GameMode::Explore ? mafio.casino() : nullptr;
        const bool at_table=table_crew && !vehicles.driver() && !poker_open && player_health.alive() &&
            Vector3Distance(shop_feet,table_crew->table)<2.8F && !mafio.provoked(table_crew->gang,match_time);
        if(controls.interact && at_table && !inventory_open && !shop_open && !mode_session.terminal()) {
            const auto money=loot_session.economy().money;
            if(money<20) {interaction_message="The Green Hand's table: 20 kr to sit down";interaction_remaining=3;}
            else {
                poker_buy_in=static_cast<int>(std::min<std::int64_t>(money,300));
                poker.sit(poker_buy_in,{"VITO","BRUNO","LA KALVA"},static_cast<std::uint64_t>(GetTime()*1000.0)+1);
                poker_open=true;poker_table.opened();input_system.cancel_controls();
            }
        }
        if(poker_open) {
            const auto choice=poker_table.input(poker,screen_width,screen_height,IsWindowFocused());
            if(choice==game::casino::PokerInput::Fold) poker.act(game::casino::Action::Fold);
            if(choice==game::casino::PokerInput::Call) poker.act(game::casino::Action::Call);
            if(choice==game::casino::PokerInput::Raise) poker.act(game::casino::Action::Raise);
            if(choice==game::casino::PokerInput::Deal) poker.deal();
            poker.update(dt);
            // Leaving (or trouble at the table) cashes the chips back into the wallet.
            if(choice==game::casino::PokerInput::Leave || !player_health.alive() || (table_crew && mafio.provoked(table_crew->gang,match_time))) {
                const int chips=poker.cash_out();
                loot_session.economy().money=std::max<std::int64_t>(0,loot_session.economy().money+chips-poker_buy_in);loot_session.changed();
                interaction_message=chips>=poker_buy_in ? TextFormat("You leave the table %d kr up",chips-poker_buy_in) :
                    TextFormat("You leave the table %d kr down",poker_buy_in-chips);
                interaction_remaining=3;poker_buy_in=0;
                poker_open=false;input_system.cancel_controls();
            }
        }
        if(controls.interact && nearby_shop && !inventory_open && !shop_open && !poker_open && !at_table && !mode_session.terminal()) {
            current_shop=nearby_shop;shop_open=true;shop_status="Select supplies, equipment or furniture. BUY / SELL transfers one item.";
            play_ui.opened();input_system.cancel_controls();
        }
        // Joining a gang (at its hideout's door) or the police (at a station): USE twice.
        if(controls.interact && !creator_active && !inventory_open && !shop_open && !poker_open && !at_table && !vehicles.driver() &&
           game_mode==game::GameMode::Explore && !career_path.empty() && player_health.alive()) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const int crew=mafio.active() ? mafio.turf_at(feet,7) : -1;
            const auto* station=crew<0 && services.active() ? services.station_near(feet,14) : nullptr;
            const bool confirming=match_time<join_offer.until && ((crew>=0 && join_offer.crew==crew) || (station && join_offer.patrol_car==station->vehicle));
            if(crew>=0 && !career.in_gang(mafio.crews()[static_cast<std::size_t>(crew)].gang) && !mafio.provoked(mafio.crews()[static_cast<std::size_t>(crew)].gang,match_time)) {
                const int gang=mafio.crews()[static_cast<std::size_t>(crew)].gang;
                const std::string name(game::mafia::gangs[static_cast<std::size_t>(gang)].name);
                if(confirming) {
                    std::string why;
                    if(career.join_gang(gang,name,why)) {mafio.set_player_gang(gang);services.assign_player_car({});gun_audio.kill_confirm();interaction_message.clear();}
                    else interaction_message=why;
                    join_offer={};
                } else {
                    join_offer={crew,{},match_time+5};
                    interaction_message="USE again to join "+name+(career.state().record ? std::string() : " - a record for life: the police will never take you");
                }
                interaction_remaining=interaction_message.empty() ? 0.0F : 4.0F;input_system.player().interact=false;
            } else if(station && !career.on_duty()) {
                if(career.state().record) {interaction_message="The Policio checked your record: no Krimuloj on the force";join_offer={};}
                else if(confirming) {
                    std::string why;
                    if(career.join_police(station->vehicle,why)) {
                        services.assign_player_car(station->vehicle);mafio.set_player_gang(-1);gun_audio.kill_confirm();interaction_message.clear();
                        // Issued: a sidearm and two magazines.
                        auto& bag=loot_session.edit_inventory();
                        if(!bag.owns_weapon(item_registry,game::combat::WeaponId::Pistol)) bag.add(item_registry,"pistol",1);
                        bag.add(item_registry,"pistol_ammo",30);loot_session.changed();
                    } else interaction_message=why;
                    join_offer={};
                } else {
                    join_offer={-1,station->vehicle,match_time+5};
                    interaction_message="USE again to join the Policio de Verda - you get a patrol car and radio calls";
                }
                interaction_remaining=interaction_message.empty() ? 0.0F : 4.0F;input_system.player().interact=false;
            }
        }
        if(controls.interact && !creator_active && !inventory_open && !shop_open && !poker_open && !at_table) {
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
                    vehicle_orbit=vehicle_pitch=0;input_system.cancel_controls();interaction_message="Driving - steer left, GAS/BRAKE right (W/S), drag above the pedals to look; USE exits";
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
        // Throttle: W/S (move_y) or the touch pedals - BRAKE slows, then reverses.
        const float pedal_throttle=std::clamp(-controls.move_y+controls.gas-controls.brake_pedal,-1.0F,1.0F);
        game::vehicles::VehicleInput driving_input{pedal_throttle,-controls.move_x,controls.brake,controls.crouch,IsKeyDown(KEY_H),controls.sprint};
        // In a LAN match the world goes on for everyone, whatever menu this player has open.
        const bool vehicle_paused=lan_match ? creator_active : creator_active||inventory_open||shop_open||poker_open||mode_session.terminal()||!IsWindowFocused()
#ifdef OUTLAND_DEV_TOOLS
            || (game_mode==game::GameMode::DevLab && dev_lab.tools_open())
#endif
            ;
        const double cars_t0=GetTime();
        vehicle_dirty|=vehicles.update(dt,driving_input,verda_region,{player.position.x,player.position.y-1,player.position.z},vehicle_paused);
        if(vehicles.events().crash>0) {gun_audio.crash(vehicles.events().crash);haptics.crash(vehicles.events().crash);}
        world_draw_stats().vehicles=world_draw_stats().vehicles*.9+(GetTime()-cars_t0)*1000*.1;
        if(driving){
            vehicle_orbit-=controls.look_x*80*dt+controls.look_dx*RAD2DEG;
            vehicle_pitch=std::clamp(vehicle_pitch-controls.look_y*dt-controls.look_dy,-1.0F,1.0F);
            // Let go of the look and the camera swings back behind the car.
            if(controls.look_x!=0 || controls.look_y!=0 || controls.look_dx!=0 || controls.look_dy!=0) vehicle_look_idle=0; else vehicle_look_idle+=dt;
            if(vehicle_look_idle>1.2F) {
                const float back=1.0F-std::exp(-dt*2.5F);
                vehicle_orbit-=std::remainder(vehicle_orbit,360.0F)*back;vehicle_pitch-=vehicle_pitch*back;
            }
        }

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
            // The penthouse: buy it (anywhere on its floor), or sleep in its bed once it is yours.
            bool home_used=false;
            if(game::rules_for(game_mode).civilian_life && penthouse.inside(loot_feet) && !shop_open && !poker_open && !inventory_open) {
                const bool owned=game::economy::Penthouse::owned(loot_session.economy());
                if(!was_in_penthouse) {interaction_message=owned ? "Home: the Stela Turo penthouse" : "Stela Turo penthouse - for sale";interaction_remaining=3;}
                if(controls.interact && loot_session.world().nearest(loot_feet,2.8F,reachable_pickup)<0) {
                    if(!owned) {
                        interaction_message=game::economy::Penthouse::buy(loot_session.economy());loot_session.changed();
                        if(game::economy::Penthouse::owned(loot_session.economy())) {gun_audio.kill_confirm();haptics.hit_given(true);}
                        interaction_remaining=4;home_used=true;
                    } else if(penthouse.near_bed(loot_feet)) {
                        // A night's sleep: morning, mended.
                        const auto clock=island_life.clock();
                        const int days=clock.minute_of_day()>=7*60 ? clock.day()+1 : clock.day();
                        island_life.set_clock(game::life::WorldClock::at(days,7,0));
                        player_health.reset();health_seen=player_health.current();
                        interaction_message="Slept until morning - "+island_life.clock().label();interaction_remaining=4;home_used=true;
                    }
                }
            }
            was_in_penthouse=penthouse.inside(loot_feet);
            if(controls.interact && !home_used && !shop_open && !poker_open && !at_table && !vehicles.driver() && vehicles.nearest(verda_region,loot_feet)<0){
                interaction_message=loot_session.interact(loot_feet,weapons,reachable_pickup);interaction_remaining=2.5F;
                if(interaction_message.starts_with("Picked up")) gun_audio.pickup();
            }
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
        if (settings.aim_assist && input_system.touch_device() && !driving && weapons.available() && player_health.alive() && !creator_active) {
            const Vector3 eye{player.position.x,player.position.y+.75F,player.position.z};
            const Vector3 view{std::sin(player.yaw)*std::cos(player.pitch),std::sin(player.pitch),std::cos(player.yaw)*std::cos(player.pitch)};
            float best=1e9F;Vector3 best_dir{};
            for(const auto& a:npcs.actors()) {
                if(a.state==characters::NpcState::Dead || a.health<=0) continue;
                const bool hostile=(a.bot>=0 && krimulo_hostile(a.bot)) || a.pool==characters::CharacterPool::Hostile || a.pool==characters::CharacterPool::Creature ||
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
        // Aiming down sights slows the look in proportion to the zoom, as scopes do, and by the
        // player's aiming sensitivity. Mouse and swipes turn by how far they moved (look_dx/dy);
        // the Creator's look stick turns at a rate.
        const float aim_factor =
            (controls.aim && !driving ? sight_fov / 70.0F * settings.aim_sensitivity : 1.0F) * assist;
        const float look_speed =
            input_system
                .layout()
                .look_sensitivity *
            aim_factor;

        player.yaw -=
            controls.look_x *
            look_speed *
            dt +
            controls.look_dx * aim_factor;

        player.pitch -=
            controls.look_y *
            look_speed *
            dt +
            controls.look_dy * aim_factor;

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

                // A hard landing thuds and is felt.
                gun_audio.land(-player.vertical_velocity);
                haptics.land(-player.vertical_velocity);

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
        // THE DROP: aboard, in freefall, under the canopy
        // ====================================================
        if(dropping) {
            using game::aircraft::DiveStage;
            const bool drop_paused=pause_menu.is_open() || mode_session.terminal() || !IsWindowFocused();
            if(!drop_paused) drop_clock+=dt;
            if(skydiver.stage()==DiveStage::Aboard) {
                // At the helicopter's open door. RELOAD (R / LOAD) moves to the next helicopter while
                // they are all still out over the sea; JUMP leaves; nobody rides past the last land.
                if(!drop_paused && drop_switch && drop_clock<switch_window && drop_fleet.size()>1) {
                    drop_ride=(drop_ride+1)%static_cast<int>(drop_fleet.size());
                    const auto& next=drop_fleet[static_cast<std::size_t>(drop_ride)];
                    player.yaw=std::atan2(next.direction.x,next.direction.z);
                    interaction_message=TextFormat("Boarded helicopter %d: %s",drop_ride+1,game::aircraft::heading_name(next.heading));
                    interaction_remaining=2.5F;
                }
                const auto& ride=drop_fleet[static_cast<std::size_t>(drop_ride)];
                player.position=game::aircraft::helicopter_door(ride,drop_clock);
                if(!drop_paused && ((drop_action && drop_clock>1.5F) || drop_clock>=game::aircraft::last_jump_time(ride))) {
                    skydiver.jump(player.position,Vector3Scale(ride.direction,ride.speed*.6F),player.yaw);
                    haptics.land(10);
                }
            } else if(skydiver.airborne() && !drop_paused) {
                dive_input.yaw=player.yaw;dive_input.pitch=player.pitch;dive_input.deploy=drop_action;
                const Vector3 body=skydiver.position();
                const float below=world::physics::WorldCollision::ground_height(body,body.y-1.0F,verda_region);
                // The sea has no floor to land on: treat the water as the ground.
                skydiver.update(dt,dive_input,std::max(below,world::layout::sea_level+.3F));
                player.position=skydiver.position();
                if(skydiver.opened_now()) {gun_audio.chute_open();haptics.explosion(25);}
                if(skydiver.landed_now()) {
                    gun_audio.land(std::max(5.0F,skydiver.landing_speed()));haptics.land(9);
                    player.grounded=true;
                    interaction_message="Landed. Find a gun, the circle is coming.";interaction_remaining=3.5F;
                }
            }
            if(skydiver.stage()!=DiveStage::Landed) {player.vertical_velocity=0;player.grounded=false;}
            // Bots touch down where they planned and come to life.
            for(std::size_t i=0;i<bot_jumps.size() && i<bot_landed.size();++i)
                if(!bot_landed[i] && drop_clock>=bot_jumps[i].land_time) {bot_landed[i]=1;br_bots.set_mounted(bot_jumps[i].bot,false);}
            drop_flying=drop_clock<game::aircraft::last_jump_time(drop_fleet)+game::aircraft::flight_linger;
            const bool all_down=std::all_of(bot_landed.begin(),bot_landed.end(),[](char landed){return landed!=0;});
            if(drop_drivers_pending && all_down) {
                // Down on the ground: some of them find a car and drive in toward the zone.
                bot_drivers.start(br_bots,vehicles,verda_region,drop_seed,mode_session.radius(),14,mode_session.center());
                drop_drivers_pending=false;
            }
            if(skydiver.stage()==DiveStage::Landed && all_down && !drop_flying) dropping=false;
        }
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
        // Aiming down sights, as in PUBG: in either camera the view moves to the eye behind the
        // gun's sights (iron sights, a red dot, a scope); a quick move, not a cut.
        const bool ads=controls.aim && !driving && weapons.available() && !creator_active;
        ads_view_blend+=((ads ? 1.0F : 0.0F)-ads_view_blend)*(1.0F-std::exp(-dt*16.0F));
        if(!ads && ads_view_blend<.02F) ads_view_blend=0;
        const bool ads_view=ads_view_blend>.85F;
        if (player.third_person && !scoped) {
            const float distance = 3.6F; // over the right shoulder, as PUBG and Fortnite frame it
            camera.position = Vector3Add(Vector3Subtract(head_position, Vector3Scale(look_direction,distance)),
                Vector3Add(Vector3Scale(gun_right,-.7F), {0,.7F,0}));
            const auto obstruction = combat_world.trace_segment(head_position,camera.position,false,false);
            if (obstruction.hit()) {
                camera.position=Vector3Lerp(head_position,camera.position,
                    std::max(0.0F,obstruction.fraction-.08F));
            }
            camera.target=Vector3Add(head_position,Vector3Scale(look_direction,25.0F));
            if(ads_view_blend>0) {
                const float k=ads_view_blend*ads_view_blend*(3-2*ads_view_blend);
                camera.position=Vector3Lerp(camera.position,head_position,k);
                camera.target=Vector3Add(camera.position,look_direction);
            }
        } else {
            camera.position=head_position;
            camera.target=Vector3Add(head_position,look_direction);
        }

        // The drop: the camera rides behind the helicopter's door, then follows the jumper, further
        // back under the canopy so it is in the frame; it eases between them.
        if(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed) {
            using game::aircraft::DiveStage;
            const Vector3 view_dir{std::sin(player.yaw)*std::cos(player.pitch),std::sin(player.pitch),std::cos(player.yaw)*std::cos(player.pitch)};
            const bool aboard=skydiver.stage()==DiveStage::Aboard;
            const bool canopy=skydiver.stage()==DiveStage::Canopy;
            const Vector3 focus=aboard ? game::aircraft::helicopter_position(drop_fleet[static_cast<std::size_t>(drop_ride)],drop_clock) :
                Vector3Add(player.position,{0,canopy ? 2.6F*skydiver.canopy() : 0.3F,0});
            // Aboard, far enough back to see the helicopter; in freefall above the diver's back.
            const float back=aboard ? 48.0F : canopy ? 9.5F+3.5F*skydiver.canopy() : 7.0F;
            const float lift=aboard ? 14.0F : canopy ? 1.5F : 3.4F;
            const Vector3 wanted=Vector3Add(Vector3Subtract(focus,Vector3Scale(view_dir,back)),{0,lift,0});
            drop_camera=drop_camera_ready ? Vector3Lerp(drop_camera,wanted,1-std::exp(-dt*(aboard ? 10.0F : 7.0F))) : wanted;
            // Freefall shakes a little with speed.
            const float shake=skydiver.stage()==DiveStage::Freefall ? std::min(1.0F,-skydiver.velocity().y/60)*.06F : 0;
            const float t=static_cast<float>(GetTime());
            camera.position=Vector3Add(drop_camera,{shake*std::sin(t*31),shake*std::sin(t*27+1),shake*std::cos(t*29)});
            camera.target=Vector3Add(focus,Vector3Scale(view_dir,20));
            drop_camera_ready=true;
            camera.fovy+=((skydiver.stage()==DiveStage::Freefall ? 80.0F : 70.0F)-camera.fovy)*(1-std::exp(-dt*3));
        } else drop_camera_ready=false;
        // High above the island the town draws further out, so the ground below reads.
        const float camera_height=camera.position.y-world::terrain::TerrainHeight::sample(camera.position.x,camera.position.z);
        // High in the drop the land is streamed farther (coarser) round the camera, and roads,
        // houses and trees reach out with it - never past its edge, or they hang in the sky.
        terrain_world.set_radius(dropping && camera_height>60 ? world::terrain::TerrainWorld::far_radius : world::terrain::TerrainWorld::ground_radius);
        const float draw_reach=dropping ? std::clamp(1+(camera_height-40)/280,1.0F,std::max(1.0F,(terrain_world.reach()-24)/340)) : 1.0F;

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
        const bool sighted=ads;
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
        Vector3 gun_direction=Vector3Normalize(Vector3Subtract(aim_point,gun_muzzle));
        // Drive-by: a pistol or SMG fires out of the driver's window, where the camera looks. The
        // shot leaves from outside the car's body so it never hits the player's own car.
        const bool drive_by=driving && weapons.available() && !creator_active &&
            (weapons.selected()==game::combat::WeaponId::Pistol || weapons.selected()==game::combat::WeaponId::Smg);
        if(drive_by) {
            const auto* car=vehicles.driver();
            const Vector3 seat{car->position.x,car->position.y+1.35F,car->position.z};
            const Vector3 sight_from=Vector3Add(camera.position,Vector3Scale(camera_direction,Vector3Distance(camera.position,seat)+3.0F));
            const auto past_car=combat_world.trace_segment(sight_from,Vector3Add(camera.position,Vector3Scale(camera_direction,500.0F)),true,true,true);
            const Vector3 target=past_car.hit() ? past_car.position : Vector3Add(camera.position,Vector3Scale(camera_direction,500.0F));
            Vector3 out=Vector3Subtract(target,seat);out.y=0;
            out=Vector3Length(out)>.01F ? Vector3Normalize(out) : Vector3{0,0,1};
            gun_muzzle=Vector3Add(seat,Vector3Scale(out,2.8F));
            gun_direction=Vector3Normalize(Vector3Subtract(target,gun_muzzle));
        }
        if (!controls.fire) trigger_ready=true;
        game::combat::WeaponInput weapon_input;
        weapon_input.fire=controls.fire && trigger_ready && (!driving || drive_by) && !creator_active && !inventory_open;
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
        weapons.update((shop_open || poker_open || mode_session.terminal()) ? 0.0F : dt,weapon_input,{gun_muzzle,gun_direction},combat_world);
        // Battle-royale recoil: each shot lifts the aim itself (the player pulls it back down);
        // once the trigger is released, part of the climb settles back on its own.
        // The view takes 65% of the kick (the rest shows in the crosshair), so a spray stays on target.
        if(weapons.events().shots>0) {
            const float pitch_kick=weapons.events().pitch_kick*.65F, yaw_kick=weapons.events().yaw_kick*.65F;
            player.pitch=std::clamp(player.pitch+pitch_kick,-1.2F,1.0F);
            player.yaw+=yaw_kick;
            recoil_debt+=pitch_kick;recoil_quiet=0;recoil_settle=0;
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
        // The rocket launcher: each shot sends a rocket off (flown and burst by the grenade system).
        for(const auto& launch:weapons.events().launches) {
            grenades.launch_rocket(launch.origin,launch.velocity);
            if(lan_match && lan.in_match()) lan.send(net::Msg::Grenade,net::Grenade{launch.origin,launch.velocity,1});
        }
        // Grenades: G or NADE throws one where the camera looks; blasts hurt everyone in reach
        // whom no wall shields - the player too - and wreck cars.
        // Outside the Dev Lab grenades are bag items (loot, shops, the starter kit): throw what you carry.
        const bool bag_grenades=game_mode!=game::GameMode::DevLab;
        if(bag_grenades) grenades.set_count(loot_session.inventory().count("frag_grenade"));
        // Quick heal (H / HEAL): the medical item that best covers the damage; boost heals slowly.
        if(controls.heal && !creator_active && !shop_open && !poker_open && player_health.alive() && !mode_session.terminal() &&
           game_mode!=game::GameMode::DevLab) {
            if(const auto said=loot_session.quick_heal(weapons,player_health); !said.empty()) {interaction_message=said;interaction_remaining=2.5F;}
        }
        if(!mode_session.terminal() && !vehicle_paused) loot_session.tick(dt,player_health);
        if(auto picked=loot_session.take_auto_message(); !picked.empty()) {interaction_message=picked;interaction_remaining=1.6F;}
        if(controls.grenade && !driving && !creator_active && !inventory_open && !shop_open && !poker_open && player_health.alive() && !mode_session.terminal()) {
            const Vector3 carried=dt>0 ? Vector3Scale(Vector3Subtract(player.position,position_before_move),1.0F/dt) : Vector3{};
            if(grenades.throw_grenade(head_position,look_direction,carried)) {
                gun_audio.grenade_pin();
                if(lan_match && lan.in_match() && !grenades.live().empty())
                    lan.send(net::Msg::Grenade,net::Grenade{grenades.live().back().position,grenades.live().back().velocity});
                if(bag_grenades) {
                    loot_session.edit_inventory().remove("frag_grenade",1);
                    loot_session.changed();
                }
            }
        }
        grenades.update(vehicle_paused || mode_session.terminal() ? 0.0F : dt,{[&](Vector3 a,Vector3 b,Vector3& point,Vector3& normal) {
            const auto hit=combat_world.trace_segment(a,b,false,false,true);
            if(!hit.hit()) return false;
            point=hit.position;normal=hit.normal;return true;
        },[&](Vector3 a,Vector3 b,Vector3& point,Vector3& normal) {
            // A rocket also goes off on people (and the targets in the Dev Lab).
            const auto hit=combat_world.trace_segment(a,b,true,true,true);
            if(!hit.hit()) return false;
            point=hit.position;normal=hit.normal;return true;
        }});
        for(std::size_t blast_index=0;blast_index<grenades.exploded().size();++blast_index) {
            const Vector3 blast=grenades.exploded()[blast_index];
            // Another player's grenade hurts the player here; its hits on everyone else are theirs to send.
            const bool thrown_elsewhere=blast_index<grenades.exploded_remote().size() && grenades.exploded_remote()[blast_index];
            const bool rocket=blast_index<grenades.exploded_rocket().size() && grenades.exploded_rocket()[blast_index];
            const float reach=game::combat::Grenades::blast_radius_of(rocket);
            sound_bus.emit(game::sound::SoundKind::Gunshot,blast,400,0,match_time);
            gun_audio.explosion(blast,camera.position,Vector3Subtract(camera.target,camera.position));
            const Vector3 core=Vector3Add(blast,{0,.35F,0});
            const auto open=[&](Vector3 to){return !combat_world.trace_segment(core,to,false,false,false).hit();};
            const Vector3 chest{player.position.x,player.position.y-.2F,player.position.z};
            const float near=Vector3Distance(core,chest);
            haptics.explosion(near);
            combat_feedback.blast(std::clamp(1-near/45,0.0F,1.0F)*.6F);
            if(!driving && near<reach && open(chest)) {
                hurt_player(game::combat::Grenades::damage_at(near,rocket));
                hurt_from=blast;hurt_directional=true;
            }
            for(std::size_t i=0;i<npcs.actors().size() && !thrown_elsewhere;++i) {
                const auto& a=npcs.actors()[i];
                if(a.state==characters::NpcState::Dead || a.health<=0) continue;
                const Vector3 target{a.position.x,a.position.y+1.1F,a.position.z};
                const float d=Vector3Distance(core,target);
                if(d>=reach || !open(target)) continue;
                const float amount=game::combat::Grenades::damage_at(d,rocket);
                combat_feedback.hit(target,amount,false);
                damage_actor(static_cast<int>(i),amount,blast);
            }
            for(std::size_t i=0;i<vehicles.vehicles().size();++i) {
                const auto* car=vehicles.asset(verda_region,vehicles.vehicles()[i].id);
                if(!car || car->vehicle.destroyed) continue;
                const float d=std::max(0.0F,Vector3Distance(core,Vector3Add(car->position,{0,.8F,0}))-1.5F);
                if(d>=reach) continue;
                game::combat::BulletHit hit;hit.kind=game::combat::HitKind::Vehicle;hit.target=static_cast<int>(i);hit.zone=static_cast<int>(game::vehicles::VehicleZone::Body);
                vehicles.damage(verda_region,hit,game::combat::Grenades::damage_at(d,rocket)*(rocket ? 2.0F : .8F)); // a rocket wrecks a car
                vehicle_dirty=true;
            }
        }
        if(!pending_kills.empty()) {gun_audio.kill_confirm();haptics.hit_given(true);}
        for(const Vector3 where:pending_kills) combat_feedback.kill(where);
        pending_kills.clear();
        environment_audio.play_combat(weapons.selected(),weapons.events());
        gun_audio.set_volume(environment_audio.volume(),environment_audio.muted());
        for(int shot=0;shot<weapons.events().shots;++shot) {gun_audio.player_shot(weapons.selected());haptics.shot(weapons.selected());}
        // Drawing another gun cuts a reload short; each gun reloads with its own sound.
        if(weapons.selected()!=equipped_weapon) {equipped_weapon=weapons.selected();gun_audio.cancel_reload();gun_audio.equip();reload_sounding=false;}
        if(weapons.events().reload_started) {gun_audio.reload(weapons.selected());reload_sounding=true;}
        else if(reload_sounding && weapons.reload_remaining()<=0) {
            if(!weapons.events().reload_finished) gun_audio.cancel_reload();
            reload_sounding=false;
        }
        if(!weapons.events().hits.empty() && weapons.events().target_hits==0) {
            bool headshot=false;
            for(const auto& note:weapons.events().hits) headshot|=note.headshot;
            gun_audio.hitmarker(headshot);
        }
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
        if(freight.running()) freight.update(vehicle_paused ? 0.0F : dt,verda_region,vehicles,Vector3Subtract(player.position,{0,1,0}),&npcs,&ranches);
        freight.follow_trailers(verda_region,vehicles);
        if(bus_line.running()) {
            bus_line.update(vehicle_paused ? 0.0F : dt,verda_region,vehicles,Vector3Subtract(player.position,{0,1,0}),&npcs);
            for(std::size_t i=0;i<bus_drivers.size() && i<bus_line.buses().size();++i) bus_drivers[i].advance(characters::AnimationAction::Idle,dt,0);
            // A bus pulling in at one of the player's stops brings a family for an empty home nearby.
            const auto& buses=bus_line.buses();
            bus_dwell_seen.resize(buses.size(),0.0F);
            for(std::size_t i=0;i<buses.size();++i) {
                const bool arrived=buses[i].dwell>0 && bus_dwell_seen[i]<=0;
                bus_dwell_seen[i]=buses[i].dwell;
                if(!arrived || island_life.empty()) continue;
                const game::transit::BusStop* stop=nullptr;float nearest_stop=30;
                for(const auto& candidate:bus_line.stops())
                    if(const float d=Vector3Distance(candidate.kerb,buses[i].position);d<nearest_stop) {nearest_stop=d;stop=&candidate;}
                if(!stop || !stop->asset.starts_with("player_stop")) continue;
                const int home_index=player_builds.vacant_home_near(stop->shelter,600);
                if(home_index<0) continue;
                auto* home=player_builds.find(player_builds.placed()[static_cast<std::size_t>(home_index)].id());
                const int beds=game::build::piece_info(home->piece).beds;
                const int members=std::clamp(2+static_cast<int>((home->serial*37+static_cast<int>(bus_seed))%3)+(beds>4 ? 2 : 0),1,beds);
                const auto ids=island_life.move_in(player_builds.building(*home),members,static_cast<std::uint64_t>(home->serial)*7919U,&stop->shelter);
                if(ids.empty()) continue;
                home->residents=static_cast<int>(ids.size());
                build_mode.notify(TextFormat("The %s family (%d) got off the bus and moved in",
                    island_life.residents()[static_cast<std::size_t>(ids.front())].family_name.c_str(),home->residents),6);
                std::string builds_error;
                if(!player_builds.save(builds_path,builds_error)) TraceLog(LOG_WARNING,"Player builds: %s",builds_error.c_str());
            }
        }
        if(!zombies.empty()) {
            zombies.hear(sound_bus);
            zombies.update(vehicle_paused ? 0.0F : dt,{npc_context.player_position,last_daylight,match_time},npcs);
        }
        hurt_player(npcs.events().player_damage);
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
            // The Krimuloj paid the police off: shooting on their turf, or in a fight with a gang, is
            // nobody's business.
            bool gang_business=mafio.active() && mafio.turf_at(frame.player,60)>=0;
            for(int g=0;g<static_cast<int>(game::mafia::gangs.size());++g) gang_business|=mafio.provoked(g,match_time);
            if(frame.player_fired && !gang_business && !career.on_duty()) justice.crime(game::law::CrimeKind::ShotsFired,frame.player,-1,frame,island_life);
            for(const auto& c:crimes) {
                // An officer who hurts the public (or another officer) is off the force.
                career.crime(c.kind==game::law::CrimeKind::Assault || c.kind==game::law::CrimeKind::Murder || c.kind==game::law::CrimeKind::AttackOnPolice);
                // Officers and paramedics from the services saw what happened to them.
                if(game::services::EmergencyServices::crew_member(c.victim)) justice.witnessed(c.kind,frame.player,frame,island_life);
                else justice.crime(c.kind,frame.player,c.victim,frame,island_life);
            }
            frame.patrol_officers=services.officers_on_foot(npcs);
            if(!vehicle_paused) justice.update(frame,island_life,npcs);
            if(services.active() && !vehicle_paused) {
                game::services::ServicesFrame call;
                call.dt=dt;call.now=match_time;call.player=frame.player;
                call.player_velocity=dt>0 ? Vector3Scale(Vector3Subtract(frame.player,last_player_feet),1.0F/dt) : Vector3{};
                call.player_alive=player_health.alive();call.player_driving=driving;
                call.wanted=justice.wanted();call.last_known=justice.last_known();
                call.sight=frame.line_of_sight;
                services.update(call,vehicles,verda_region,npcs,&island_life);
                if(!services.events().message.empty()) {interaction_message=services.events().message;interaction_remaining=3;}
            }
        }
        crimes.clear();
        // LAN play: what the others did since the last frame.
        const bool lan_royale=lan_match && game_mode==game::GameMode::BattleRoyale;
        if(lan_match) {
            lan.poll(GetTime());
            if(!lan.active() && !lan_host_lost) {
                // The host has gone (or this was the host and the network went): the match goes on here alone.
                lan_host_lost=true;interaction_message="LAN: the connection is gone - the match goes on here";interaction_remaining=4;
                for(const auto& remote:std::vector<game::lan::Remote>(lan_play.remotes())) lan_play.remove(remote.slot,npcs,vehicles);
            }
            const Vector3 ear_forward=Vector3Subtract(camera.target,camera.position);
            for(const auto& message:lan.take_inbox()) {
                const auto from=message.header.from;
                switch(message.header.type) {
                case net::Msg::State: {
                    net::PlayerState state;
                    if(net::decode(message.payload,state)) lan_play.received(from,lan.name_of(from),state,GetTime());
                    break;
                }
                case net::Msg::Hit: {
                    net::Hit hit;
                    if(net::decode(message.payload,hit) && player_health.alive() && !mode_session.terminal()) {
                        hurt_player(hit.amount);hurt_from=hit.from;hurt_directional=true;lan_last_attacker=hit.attacker;
                    }
                    break;
                }
                case net::Msg::Died: {
                    net::Died died;
                    if(!net::decode(message.payload,died)) break;
                    const auto victim=lan.name_of(from);
                    if(died.killer>=0 && died.killer==static_cast<std::int32_t>(lan.slot())) {br_bots.credit_player_kill(victim);gun_audio.kill_confirm();}
                    else if(died.killer>=0) br_bots.add_feed(lan.name_of(static_cast<std::uint8_t>(died.killer))+" ELIMINATED "+victim);
                    else br_bots.add_feed(victim+" IS OUT");
                    break;
                }
                case net::Msg::Taken: {
                    net::Taken taken;
                    if(net::decode(message.payload,taken)) loot_session.edit_world().taken_elsewhere(taken.pickup,taken.remaining);
                    break;
                }
                case net::Msg::Dropped: {
                    net::Dropped dropped;
                    if(net::decode(message.payload,dropped))
                        loot_session.edit_world().drop(item_registry,dropped.item,dropped.quantity,dropped.position,dropped.loaded,dropped.pickup);
                    break;
                }
                case net::Msg::BotSnapshot: {
                    net::BotSnapshot snapshot;
                    if(!lan_royale || lan.hosting() || !net::decode(message.payload,snapshot)) break;
                    lan_play.apply(snapshot,br_bots,vehicles,verda_region,dt);
                    for(const auto& b:snapshot.bots)
                        if(b.flags & net::BotFiring) if(const auto* bot=br_bots.find(b.id))
                            gun_audio.world_shot(bot->weapon(),{b.feet.x,b.feet.y+1.4F,b.feet.z},camera.position,ear_forward);
                    break;
                }
                case net::Msg::BotHit: {
                    net::BotHit hit;
                    if(!lan_royale || !lan.hosting() || !net::decode(message.payload,hit)) break;
                    const auto who=lan.name_of(from);
                    if(br_bots.damage_bot_by(hit.bot,hit.amount,hit.from,match_time,who)) {
                        lan.send(net::Msg::KillCredit,net::KillCredit{hit.bot},from);
                        // Everyone else reads it in the feed.
                        if(!br_bots.feed().empty())
                            for(const auto& player_entry:lan.roster().players)
                                if(player_entry.slot!=0 && player_entry.slot!=from) lan.send(net::Msg::Feed,net::Feed{br_bots.feed().back()},player_entry.slot);
                    }
                    break;
                }
                case net::Msg::KillCredit: {
                    net::KillCredit credit;
                    if(net::decode(message.payload,credit)) {br_bots.credit_kill(credit.bot);gun_audio.kill_confirm();}
                    break;
                }
                case net::Msg::Feed: {
                    net::Feed feed;
                    if(net::decode(message.payload,feed)) br_bots.add_feed(feed.text);
                    break;
                }
                case net::Msg::Grenade: {
                    net::Grenade grenade;
                    if(!net::decode(message.payload,grenade)) break;
                    if(grenade.rocket) grenades.launch_rocket(grenade.origin,grenade.velocity,true);
                    else grenades.spawn_remote(grenade.origin,grenade.velocity);
                    break;
                }
                case net::Msg::Leave: {
                    const auto* remote=lan_play.find(from);
                    br_bots.add_feed((remote ? remote->name : lan.name_of(from))+" LEFT");
                    lan_play.remove(from,npcs,vehicles);
                    break;
                }
                default: break;
                }
            }
            for(const auto& [where,weapon]:lan_play.take_shots())
                gun_audio.world_shot(static_cast<game::combat::WeaponId>(std::min<int>(weapon,static_cast<int>(game::combat::weapon_count)-1)),where,camera.position,ear_forward);
            lan_play.update(dt,npcs,&character_registry,vehicles,verda_region);
        }
        // A LAN client mirrors the host's bots instead of running its own (unless the host has gone).
        const bool bots_mirrored=lan_royale && lan.active() && !lan.hosting();
        {
            const Vector3 feet{player.position.x,player.position.y-1.0F,player.position.z};
            if(game_mode==game::GameMode::BattleRoyale && bots_mirrored) {
                br_bots.sync_bodies(npcs,&character_registry);
            } else if(game_mode==game::GameMode::BattleRoyale && !vehicle_paused) {
                game::ai::BattleRoyaleFrame frame;
                frame.dt=dt;frame.now=match_time;frame.player_feet=feet;
                frame.player_velocity=dt>0 ? Vector3Scale(Vector3Subtract(feet,last_player_feet),1.0F/dt) : Vector3{};
                frame.player_alive=player_health.alive();
                frame.player_fired=weapons.events().shots>0;
                frame.player_weapon=weapons.selected();
                frame.player_in_vehicle=driving;
                if(lan_royale) frame.others=lan_play.others();
                const double bots_t0=GetTime();
                bot_drivers.update(dt,br_bots,vehicles,verda_region,feet,player_health.alive(),mode_session.radius(),bot_world.environment.line_of_sight,mode_session.center());
                br_bots.update(frame,bot_world);
                br_bots.sync_bodies(npcs,&character_registry);
                world_draw_stats().bots=world_draw_stats().bots*.9+(GetTime()-bots_t0)*1000*.1;
                hurt_player(br_bots.events().player_damage);
                const Vector3 ear_forward=Vector3Subtract(camera.target,camera.position);
                for(const Vector3 past:br_bots.events().near_misses) gun_audio.near_miss(past,camera.position,ear_forward);
                for(const Vector3 strike:br_bots.events().ricochets) gun_audio.ricochet(strike,camera.position,ear_forward);
                if(br_bots.events().player_damage>0) {hurt_from=br_bots.events().player_damage_from;hurt_directional=true;lan_last_attacker=-1;}
                // The host's bots shooting the other players: each hit goes to whoever it hit.
                if(lan_royale && lan.in_match())
                    for(const auto& hit:br_bots.events().other_hits)
                        if(game::lan::LanPlay::is_remote_body(hit.player))
                            lan.send(net::Msg::Hit,net::Hit{-std::max(1,hit.by),hit.amount,hit.from,0},game::lan::LanPlay::slot_of(hit.player));
            } else if(mafio.active() && !vehicle_paused) {
                // La Mafio: crews at their hideouts, hit squads on the road, wars and respawns.
                game::mafia::MafioFrame frame;
                frame.dt=dt;frame.now=match_time;frame.player_feet=feet;
                frame.player_velocity=dt>0 ? Vector3Scale(Vector3Subtract(feet,last_player_feet),1.0F/dt) : Vector3{};
                frame.player_alive=player_health.alive();
                frame.player_fired=weapons.events().shots>0;
                frame.player_weapon=weapons.selected();
                frame.player_in_vehicle=driving;
                frame.aim_origin=gun_muzzle;frame.aim_direction=gun_direction;
                if(!player_health.alive()) mafio.forgive();
                const double bots_t0=GetTime();
                if(ranches.active()) {
                    game::ranch::RanchFrame ranch_frame;
                    ranch_frame.dt=dt;ranch_frame.now=match_time;ranch_frame.clock=island_life.clock();ranch_frame.player_feet=feet;
                    ranch_frame.player_alive=player_health.alive();ranch_frame.player_fired=frame.player_fired;
                    ranch_frame.aim_origin=gun_muzzle;ranch_frame.aim_direction=gun_direction;
                    if(const auto* driven=vehicles.driver()) ranch_frame.player_vehicle=driven->id;
                    ranches.update(ranch_frame,br_bots,vehicles,verda_region);
                }
                if(walking_man.active()) {
                    game::life::WalkingManFrame paul;
                    paul.dt=dt;paul.now=match_time;paul.clock=island_life.clock();paul.player_feet=feet;
                    paul.player_alive=player_health.alive();paul.player_in_vehicle=driving;
                    paul.player_fired=frame.player_fired;paul.player_aiming=controls.aim && weapons.available() && !driving;
                    paul.aim_origin=gun_muzzle;paul.aim_direction=gun_direction;
                    paul.line_of_sight=bot_world.environment.line_of_sight;
                    walking_man.update(paul,br_bots,bot_drivers,vehicles,verda_region);
                }
                mafio.update(frame,br_bots,bot_world,bot_drivers,vehicles,verda_region);
                // The career: what the world holds for a job, and what the player did.
                if(!career_path.empty()) {
                    game::career::CareerFrame work;
                    work.now=match_time;work.player=feet;work.alive=player_health.alive();
                    if(const auto* car=vehicles.driver()) work.driving=car->id;
                    for(const auto& g:game::mafia::gangs) work.gang_names.emplace_back(g.name);
                    for(std::size_t c=0;c<mafio.crews().size();++c) {
                        const auto& crew=mafio.crews()[c];
                        int alive=0;
                        for(const int id:crew.members) if(const auto* bot=br_bots.find(id);bot && bot->alive()) ++alive;
                        work.crews.push_back({static_cast<int>(c),crew.gang,crew.door,crew.town,alive});
                    }
                    for(const auto& rig:freight.rigs()) {
                        const int v=vehicles.find(rig.vehicle);
                        if(v>=0) work.rigs.push_back({rig.vehicle,vehicles.vehicles()[static_cast<std::size_t>(v)].position,rig.cattle ? "cattle rig" : "supply rig"});
                    }
                    for(const auto& war:mafio.wars()) {
                        if(war.until<match_time || war.target<0) continue;
                        const auto& crew=mafio.crews()[static_cast<std::size_t>(war.target)];
                        work.wars.push_back({war.attackers,war.defenders,crew.door,crew.town});
                    }
                    for(const auto& town:verda_region.settlements()) work.towns.push_back({town.name,town.center});
                    if(!player_health.alive() && !career_dead) {career.died();career_dead=true;}
                    if(player_health.alive()) career_dead=false;
                    career.update(work);
                    if(const int pay=career.take_pay();pay>0) {loot_session.economy().money+=pay;loot_session.changed();}
                    mafio.set_player_gang(career.state().side==game::career::Side::Gang ? career.state().gang : -1);
                    services.assign_player_car(career.on_duty() ? career.state().patrol_car : std::string());
                    services.set_player_siren(career.on_duty() && career.job().kind!=game::career::JobKind::None);
                    career.tick_message(dt);
                    // Saved whenever it changes (at most once a second).
                    career_save_wait-=dt;
                    if(career_save_wait<=0) {
                        career_save_wait=1;
                        if(const auto text=career.serialise();text!=career_saved && SaveFileText(career_path.c_str(),const_cast<char*>(text.c_str()))) career_saved=text;
                    }
                }
                br_bots.sync_bodies(npcs,&character_registry);
                walking_man.dress(npcs);
                world_draw_stats().bots=world_draw_stats().bots*.9+(GetTime()-bots_t0)*1000*.1;
                hurt_player(br_bots.events().player_damage);
                const Vector3 ear_forward=Vector3Subtract(camera.target,camera.position);
                for(const Vector3 past:br_bots.events().near_misses) gun_audio.near_miss(past,camera.position,ear_forward);
                for(const Vector3 strike:br_bots.events().ricochets) gun_audio.ricochet(strike,camera.position,ear_forward);
                if(br_bots.events().player_damage>0) {hurt_from=br_bots.events().player_damage_from;hurt_directional=true;}
            }
            const bool finished=mode_session.terminal();
            // Riding and falling are out of the gas's reach (the line starts out over the sea).
            const bool in_drop=dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed;
            const Vector3 zone_feet=in_drop ? Vector3{mode_session.center().x,0,mode_session.center().y} : feet;
            if(lan_royale) mode_session.set_lan(lan_play.alive(),!bots_mirrored);
            mode_session.update(dt,zone_feet,player_health,br_bots,vehicle_paused);
            if(game_mode==game::GameMode::BattleRoyale) {
                br_bots.sync_bodies(npcs,&character_registry);
                // A fallen bot leaves its kit: its gun (loaded), ammo, and sometimes meds or a vest.
                for(const auto& bot:br_bots.bots()) {
                    if(bot.alive() || std::find(bots_looted.begin(),bots_looted.end(),bot.id())!=bots_looted.end()) continue;
                    bots_looted.push_back(bot.id());
                    const Vector3 at=bot.position();
                    const auto spot=[&](int k){const float a=static_cast<float>(k)*2.1F+static_cast<float>(bot.id());return Vector3{at.x+std::cos(a)*.7F,at.y,at.z+std::sin(a)*.7F};};
                    const auto weapon=bot.weapon();
                    const std::uint32_t roll=static_cast<std::uint32_t>(bot.id())*2654435761U;
                    int k=0;
                    for(const auto& item:item_registry.items()) {
                        // Its gun, and the rounds it loads (every shotgun takes the same shells).
                        if(!item.weapon || (*item.weapon!=weapon && !(item.category==game::inventory::ItemCategory::Ammo && *item.weapon==game::combat::ammo_of(weapon)))) continue;
                        if(item.category==game::inventory::ItemCategory::Weapon)
                            loot_session.edit_world().drop(item_registry,item.id,1,spot(k++),game::combat::definition(weapon).magazine);
                        else if(item.category==game::inventory::ItemCategory::Ammo)
                            loot_session.edit_world().drop(item_registry,item.id,std::max(4,game::combat::definition(weapon).magazine*2),spot(k++));
                    }
                    if(roll%2==0) loot_session.edit_world().drop(item_registry,"bandage",1+static_cast<int>(roll%3),spot(k++));
                    if(roll%5==1) loot_session.edit_world().drop(item_registry,roll%3==0 ? "vest_heavy" : "vest_light",1,spot(k++));
                    if(roll%4==3) loot_session.edit_world().drop(item_registry,"frag_grenade",1,spot(k++));
                }
            }
            if(!finished && mode_session.terminal()) {
                shop_open=inventory_open=false;play_ui.opened();input_system.cancel_controls();
                vehicle_audio.stop();
            }
            if(lan_match && lan.in_match()) {
                // Out of the match: tell the others (and who did it).
                if(!player_health.alive() && !lan_death_sent) {lan_death_sent=true;lan.send(net::Msg::Died,net::Died{lan_last_attacker});}
                // Where this player is, 20 times a second.
                lan_state_wait-=dt;
                lan_fired=lan_fired || weapons.events().shots>0;
                if(lan_state_wait<=0) {
                    lan_state_wait=1.0F/game::lan::LanPlay::state_rate;
                    net::PlayerState state;
                    state.time=static_cast<float>(match_time);
                    state.feet=feet;state.yaw=player.yaw;state.pitch=player.pitch;
                    state.velocity=dt>0 ? Vector3Scale(Vector3Subtract(feet,last_player_feet),1.0F/dt) : Vector3{};
                    state.health=player_health.current();
                    state.weapon=static_cast<std::uint8_t>(weapons.selected());
                    if(player_health.alive()) state.flags|=net::StateAlive;
                    if(lan_fired) state.flags|=net::StateFiring;
                    if(controls.aim) state.flags|=net::StateAiming;
                    if(player.grounded) state.flags|=net::StateGrounded;
                    if(in_drop) {
                        state.flags|=net::StateDropping;
                        state.dive=static_cast<std::uint8_t>(skydiver.stage());
                        if(skydiver.stage()==game::aircraft::DiveStage::Canopy) state.flags|=net::StateCanopy;
                    }
                    if(const auto* car=vehicles.driver()) {
                        const int index=vehicles.find(car->id);
                        if(index>=0) {
                            state.flags|=net::StateDriving;state.vehicle=static_cast<std::int16_t>(index);
                            state.car=car->position;state.car_yaw=car->yaw;state.car_speed=car->speed;
                        }
                    }
                    lan.send(net::Msg::State,state);
                    lan_fired=false;
                }
                // The host: every bot and moving car, 10 times a second, for the others to mirror.
                if(lan_royale && lan.hosting()) {
                    lan_snapshot_wait-=dt;
                    if(lan_snapshot_wait<=0) {
                        lan_snapshot_wait=1.0F/game::lan::LanPlay::snapshot_rate;
                        lan.send(net::Msg::BotSnapshot,lan_play.snapshot(br_bots,vehicles,static_cast<float>(match_time)));
                    }
                }
            }
            last_player_feet=feet;
            // Everyone else's gunfire, heard where it happened (bots, police): the bus says how far
            // it carries, which also tells pistol from rifle from sniper.
            for(const auto& heard:sound_bus.events()) {
                if(heard.serial<=gunfire_heard || heard.source==0) continue;
                // Someone running close by: a step every stride (the bus hears them every frame).
                if(heard.kind==game::sound::SoundKind::Footstep) {
                    auto& last=footstep_heard[heard.source];
                    if(match_time-last>=.31) {last=match_time;gun_audio.footstep(heard.position,camera.position,Vector3Subtract(camera.target,camera.position));}
                    continue;
                }
                if(heard.kind!=game::sound::SoundKind::Gunshot) continue;
                const float r=heard.radius;
                const auto weapon=r>=400 ? game::combat::WeaponId::Sniper : r>=290 ? game::combat::WeaponId::Rifle :
                    r>=250 ? game::combat::WeaponId::Shotgun : r>=190 ? game::combat::WeaponId::Smg : game::combat::WeaponId::Pistol;
                gun_audio.world_shot(weapon,heard.position,camera.position,Vector3Subtract(camera.target,camera.position));
            }
            gunfire_heard=sound_bus.latest();
            gun_audio.update(vehicle_paused ? 0.0F : dt);
            haptics.update(dt);
            // The nearest siren on the island, placed like a shot.
            {
                float loudest=0,pan=0;
                if(!vehicle_paused) for(const auto& unit:services.units()) {
                    if(!unit.siren) continue;
                    const int v=vehicles.find(unit.vehicle);
                    if(v<0) continue;
                    const auto mix=audio::mix_shot(vehicles.vehicles()[static_cast<std::size_t>(v)].position,camera.position,
                        Vector3Subtract(camera.target,camera.position),520,1.8F);
                    if(mix.audible && mix.volume>loudest) {loudest=mix.volume;pan=mix.pan;}
                }
                gun_audio.siren(loudest*.8F,pan);
            }
            // The drop: rotor thump near the helicopters (loud aboard), wind rising with the fall.
            {
                float rotor=0,wind=0,pitch=1;
                if(game_mode==game::GameMode::BattleRoyale && dropping && !vehicle_paused && !pause_menu.is_open()) {
                    using game::aircraft::DiveStage;
                    if(drop_flying) {
                        float nearest=1e9F;
                        for(const auto& path:drop_fleet)
                            nearest=std::min(nearest,Vector3Distance(camera.position,game::aircraft::helicopter_position(path,drop_clock)));
                        rotor=skydiver.stage()==DiveStage::Aboard ? .75F : std::clamp(.8F/(1+nearest/45),0.0F,.8F);
                    }
                    if(skydiver.stage()==DiveStage::Freefall) {
                        const float speed=Vector3Length(skydiver.velocity());
                        wind=std::clamp(speed/60,0.0F,1.0F)*.85F;pitch=.8F+speed/140;
                    } else if(skydiver.stage()==DiveStage::Canopy) {wind=.16F;pitch=.75F;}
                }
                gun_audio.drop_loops(rotor,wind,pitch);
            }
            // Warnings you hear: a pulse each second outside the zone, your heart when badly hurt.
            const float sound_dt=vehicle_paused ? 0.0F : dt;
            hurt_sound_wait-=sound_dt;zone_sound_wait-=sound_dt;heart_sound_wait-=sound_dt;
            if(game_mode==game::GameMode::BattleRoyale && player_health.alive() && !mode_session.terminal() &&
               !(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed) &&
               mode_session.distance(feet)>mode_session.radius() && zone_sound_wait<=0) {gun_audio.zone_warning();zone_sound_wait=1;}
            const float health_share=player_health.current()/std::max(1.0F,player_health.maximum());
            if(player_health.alive() && health_share<.35F && heart_sound_wait<=0 && !mode_session.terminal()) {
                gun_audio.heartbeat(1.2F-health_share*2);heart_sound_wait=.75F+health_share*1.2F;
            }
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

        // High in the drop everything is far away: a farther near plane keeps the depth buffer
        // precise enough that roads do not flicker through the ground below.
        rlSetClipPlanes(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed ? .6 : RL_CULL_DISTANCE_NEAR,RL_CULL_DISTANCE_FAR);
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
        else static_world.draw(verda_region,camera.position,&view,draw_reach,dropping ? std::clamp((camera_height-60)*.006F,0.0F,3.0F) : 0.0F);
        double sub_t1=GetTime();

        draw_model_backed_world_assets(verda_region, camera.position, character_registry, view, draw_reach);
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
        // Drivers at the wheel of the side-by-sides and the big rigs (Vakeroj in their hats).
        {
            std::vector<std::pair<std::string,std::pair<std::string,bool>>> seated;
            for(const auto& d:ranches.drivers()) seated.push_back({d.vehicle,{d.character,d.hat}});
            for(const auto& d:freight.drivers()) seated.push_back({d.vehicle,{d.character,d.hat}});
            ranch_seats.resize(seated.size());
            for(std::size_t i=0;i<seated.size();++i) {
                const auto* asset=vehicles.asset(verda_region,seated[i].first);
                const auto* type=asset ? vehicles.definition(*asset) : nullptr;
                if(!asset || !type || seated[i].second.first.empty() || Vector3DistanceSqr(asset->position,camera.position)>110*110) continue;
                const float scale=asset->size.x/type->width;
                const auto seat=game::vehicles::VehicleSystem::world_point(type->seat,asset->position,asset->rotation_y,scale);
                characters::BodyPose sitting;sitting.seated=true;
                ranch_seats[i].advance(characters::AnimationAction::Idle,dt,0);
                characters::HeldGun crown;
                character_renderer.draw(seated[i].second.first,{seat.x,seat.y+.1F-.92F,seat.z},asset->rotation_y,1,&ranch_seats[i],sitting,&crown);
                if(crown.hat && seated[i].second.second) draw_cowboy_hat(crown);
            }
        }
        vehicle_renderer.draw(vehicles,verda_region,camera.position);
        // The big rigs' trailers, hitched to their tractors.
        for(const auto& trailer:freight.trailers()) {
            if(Vector3DistanceSqr(trailer.centre,camera.position)>320*320) continue;
            Model* model=get_world_asset_model(trailer.model,character_registry);
            if(!model) continue;
            rlPushMatrix();
            rlTranslatef(trailer.centre.x,trailer.centre.y,trailer.centre.z);
            rlRotatef(trailer.yaw,0,1,0);
            rlRotatef(-trailer.pitch,1,0,0);
            DrawModel(*model,{0,0,0},1,WHITE);
            rlPopMatrix();
        }
        // Cattle: in the ranches' pastures and corrals, going up the chutes, and in the stockyard.
        if(ranches.active()) {
            Model* hereford=get_world_asset_model("assets/verda/ranch/cow_hereford.glb",character_registry);
            Model* angus=get_world_asset_model("assets/verda/ranch/cow_angus.glb",character_registry);
            const auto draw_cow=[&](const game::ranch::Cow& cow) {
                if(cow.state==game::ranch::CowState::Gone || Vector3DistanceSqr(cow.position,camera.position)>240*240) return;
                Model* model=cow.angus ? angus : hereford;
                if(!model || !view.sphere({cow.position.x,cow.position.y+.8F,cow.position.z},1.6F)) return;
                const bool dead=cow.state==game::ranch::CowState::Dead;
                const float bob=dead ? 0.0F : .035F*std::abs(std::sin(cow.stride*2.2F));
                rlPushMatrix();
                rlTranslatef(cow.position.x,cow.position.y+bob,cow.position.z);
                rlRotatef(cow.yaw,0,1,0);
                if(dead) {rlTranslatef(.45F,.35F,0);rlRotatef(88,0,0,1);}
                else rlRotatef(2.5F*std::sin(cow.stride*2.2F),0,0,1); // the walk's sway
                DrawModel(*model,{0,0,0},1,WHITE);
                rlPopMatrix();
            };
            for(const auto& ranch:ranches.ranches()) for(const auto& cow:ranch.herd) draw_cow(cow);
            for(const auto& cow:ranches.stockyard().pen) draw_cow(cow);
        }
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
        // A drive-by's flash at the driver's window.
        if(drive_by && weapons.muzzle_flash()>0) {
            DrawSphere(gun_muzzle,.12F,Color{255,228,135,255});
            DrawCylinderEx(gun_muzzle,Vector3Add(gun_muzzle,Vector3Scale(gun_direction,.45F)),.09F,0,6,Color{255,169,65,255});
        }
        if(Model* grenade_model=grenades.live().empty() ? nullptr : get_world_asset_model("assets/verda/combat/frag_grenade.glb",character_registry))
            for(const auto& g:grenades.live()) {
                if(g.rocket) continue;
                // Tumbling through the air, settling once it has stopped.
                const float spin=g.fuse*720.0F*std::min(1.0F,Vector3Length(g.velocity)/4.0F);
                DrawModelEx(*grenade_model,Vector3Subtract(g.position,{0,.05F,0}),Vector3Normalize({1,.3F,.2F}),spin,{1.6F,1.6F,1.6F},WHITE);
            }
        // Rockets in flight: a dark body nose-first, the motor's flame and a trail of smoke.
        for(const auto& g:grenades.live()) {
            if(!g.rocket) continue;
            const Vector3 heading=Vector3Normalize(g.velocity);
            const Vector3 tail=Vector3Subtract(g.position,Vector3Scale(heading,.7F));
            DrawCylinderEx(tail,g.position,.05F,.05F,8,Color{70,76,58,255});
            DrawCylinderEx(g.position,Vector3Add(g.position,Vector3Scale(heading,.18F)),.05F,0,8,Color{92,98,70,255});
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphere(tail,.14F,Color{255,190,90,255});
            DrawCylinderEx(tail,Vector3Subtract(tail,Vector3Scale(heading,.6F)),.09F,0,6,Color{255,140,50,255});
            EndBlendMode();
            const float flown=game::combat::Grenades::rocket_life-g.fuse;
            for(int k=1;k<=16;++k) {
                const float back=static_cast<float>(k)*.8F;
                if(back>flown*Vector3Length(g.velocity)) break;
                DrawSphere(Vector3Subtract(tail,Vector3Scale(heading,back)),.14F+.045F*static_cast<float>(k),Fade(Color{196,192,184,255},.55F-.03F*static_cast<float>(k)));
            }
        }
        for(const auto& e:grenades.explosions()) {
            const float t=e.age/game::combat::Grenades::explosion_life, big=e.rocket ? 1.5F : 1.0F;
            if(t<.25F) {
                // The fireball: a solid white-hot core inside an additive orange burst.
                const float k=t/.25F;
                DrawSphere(e.position,(1.0F+k*3.2F)*big,Color{236,110,24,static_cast<unsigned char>(235*(1-k*.6F))});
                if(k<.6F) DrawSphere(e.position,(.6F+k*2.0F)*big,Color{255,214,120,255});
                BeginBlendMode(BLEND_ADDITIVE);
                DrawSphere(e.position,(.5F+k*1.4F)*big,Color{static_cast<unsigned char>(200*(1-k)),static_cast<unsigned char>(150*(1-k)),static_cast<unsigned char>(60*(1-k)),255});
                EndBlendMode();
            }
            const float rise=std::min(1.0F,t*1.6F);
            DrawSphere({e.position.x,e.position.y+.6F+t*2.6F,e.position.z},(1.0F+rise*2.4F)*big,Fade(Color{72,70,66,255},.6F*(1-t)));
        }
        }
        // A pump or bolt works through the cycle after each shot of a slow gun.
        const auto& held_weapon=weapons.weapon();
        const float gun_work=!held_weapon.automatic && held_weapon.interval>.5F ?
            std::sin(std::clamp(1-weapons.cooldown()/held_weapon.interval,0.0F,1.0F)*PI)*(weapons.cooldown()>0 ? 1.0F : 0.0F) : 0.0F;
        const float gun_reload=weapons.reload_remaining()/held_weapon.reload_seconds;
        if(weapons.available() && !driving && !creator_active && (!player.third_person || ads_view) && !scoped)
            game::combat::CombatRenderer::draw_gun(gun_grip,look_direction,view_up,weapons.selected(),gun_reload,weapons.muzzle_flash(),true,gun_work,sight);
        if(!creator_active)loot_ui.draw_world(item_registry,loot_session.world(),loot_feet,loot_session.inventory().state().light);

        if(perf.enabled()) perf.mark(FrameStage::World,GetTime());
        // People hold what they carry: police their pistols, Battle Royale bots their loadout.
        // Hats: the Vakeroj's cattleman hats (game::ranch).
        const auto draw_hat=[&](const characters::NpcInstance& actor,const characters::HeldGun& held) {
            if(actor.bot>=0 && ranches.vakero(actor.bot) && actor.state!=characters::NpcState::Dead) draw_cowboy_hat(held);
        };
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
            [&](const characters::NpcInstance& actor,const characters::HeldGun& held) {
                if(held.held) {
                    const int weapon=actor.weapon>=0 ? actor.weapon : 0;
                    game::combat::CombatRenderer::draw_gun(held.grip,held.aim,held.up,static_cast<game::combat::WeaponId>(weapon),0,0,false);
                }
                if(held.hat) draw_hat(actor,held);
            });
        // Their muzzles flash as they fire, so you see who is shooting at you.
        // No tracer lines (real rounds are not seen in flight); the muzzle flash shows the shooter.
        for(const auto& tracer:br_bots.tracers()) if(tracer.life>.05F) DrawSphere(tracer.start,.1F,Color{255,222,150,235});
        for(const auto& tracer:police_tracers) if(tracer.life>.05F) DrawSphere(tracer.start,.1F,Color{255,222,150,235});

        // ----------------------------------------------------
        // PLAYER BODY
        // ----------------------------------------------------

        const bool skydiving=dropping && skydiver.airborne();
        if(skydiving) {
            // Belly-down in freefall, hanging under the canopy once it opens (blending as it fills).
            const float open=skydiver.canopy();
            characters::BodyPose body;
            body.skydive=skydiver.stage()==game::aircraft::DiveStage::Freefall ? 1.0F : std::max(0.0F,1-open*2);
            body.hang=skydiver.stage()==game::aircraft::DiveStage::Canopy ? std::min(1.0F,open*1.5F) : 0.0F;
            body.body_pitch=skydiver.body_pitch()+(body.hang>0 ? skydiver.swing()*.6F : 0.0F);
            body.body_roll=skydiver.bank();
            const Vector3 character_feet{player.position.x,player.position.y-1.0F,player.position.z};
            character_renderer.draw_player(character_feet,skydiver.heading(),body);
            if(skydiver.stage()==game::aircraft::DiveStage::Canopy)
                game::aircraft::draw_parachute(get_world_asset_model("assets/verda/aircraft/parachute.glb",character_registry),
                    Vector3Add(player.position,{0,.55F,0}),skydiver.heading(),open,skydiver.swing(),skydiver.bank());
        }
        if (player.third_person && !driving && !scoped && !ads_view && !(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed)) {

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
#ifdef OUTLAND_DEV_TOOLS
            // OUTLAND_DEV_HAT=1: the player wears a Vakero hat (to check how hats sit).
            if(static const bool hat=[]{const char* v=std::getenv("OUTLAND_DEV_HAT");return v && *v=='1';}();hat && !scoped) draw_cowboy_hat(held);
#endif
            if(held.held && !scoped) {
                const auto aim=weapon_input.sprint ? held.aim : Vector3Normalize(Vector3Subtract(aim_point,held.grip));
                game::combat::CombatRenderer::draw_gun(held.grip,aim,held.up,weapons.selected(),gun_reload,weapons.muzzle_flash(),false,gun_work,sight);
            }
        }

        build_mode.draw_world(static_cast<float>(GetTime()));
        if(mafio.active()) game::mafia::draw_turf(mafio,camera.position,static_cast<float>(GetTime()));
        // The current job's waypoint: a column of light to steer by, and a ring on the ground.
        if(career.job().kind!=game::career::JobKind::None) {
            const Vector3 at=career.job().target;
            const float ground=world::terrain::TerrainHeight::sample(at.x,at.z);
            const Color beam=career.on_duty() ? Color{90,150,255,255} : Color{255,196,70,255};
            const float pulse=.75F+.25F*std::sin(static_cast<float>(GetTime())*3);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCylinder({at.x,ground,at.z},1.1F,1.1F,90,10,Fade(beam,.22F*pulse));
            DrawCylinder({at.x,ground,at.z},.35F,.35F,90,8,Fade(beam,.5F*pulse));
            EndBlendMode();
            DrawCircle3D({at.x,ground+.15F,at.z},game::career::Career::arrive_radius*.5F,{1,0,0},90,Fade(beam,.8F));
        }
        if(services.active()) game::services::draw_light_bars(services,vehicles,camera.position,static_cast<float>(GetTime()));
        // The drop: the helicopters on their routes, and the bots on their way down.
        if(game_mode==game::GameMode::BattleRoyale && dropping) {
            if(drop_flying)
                game::aircraft::draw_helicopters(drop_fleet,drop_clock,get_world_asset_model("assets/verda/aircraft/drop_helicopter.glb",character_registry),camera.position);
            const Model* chute=get_world_asset_model("assets/verda/aircraft/parachute.glb",character_registry);
            for(std::size_t i=0;i<bot_jumps.size() && i<bot_landed.size();++i) {
                if(bot_landed[i]) continue;
                const auto air=game::aircraft::bot_in_air(bot_jumps[i],drop_clock);
                if(Vector3Distance(air.position,camera.position)>900) continue;
                const unsigned char shade=static_cast<unsigned char>(60+(bot_jumps[i].bot*37)%120);
                game::aircraft::draw_bot_in_air(air,chute,Color{shade,static_cast<unsigned char>(70+(bot_jumps[i].bot*53)%90),static_cast<unsigned char>(50+(bot_jumps[i].bot*29)%80),255});
            }
        }
        // The circle's edge stands in the world as a wall of blue light (violet as the end nears),
        // and the next circle as a white ring on the ground.
        if(game_mode==game::GameMode::BattleRoyale && !mode_session.terminal()) {
            const auto zone=mode_session.zone();
            const float menace=static_cast<float>(zone.phase)/std::max<std::size_t>(1,mode_session.circle.phases.size());
            game::draw_zone_wall(zone.center,zone.radius,camera.position,static_cast<float>(GetTime()),menace,zone.shrinking);
            if(zone.next_radius<zone.radius-1) game::draw_next_circle(zone.next_center,zone.next_radius,camera.position,static_cast<float>(GetTime()));
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
        if(services.active()) game::services::draw_light_bars(services,vehicles,camera.position,static_cast<float>(GetTime()));
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

        // (No "OUTLAND" title in play: it sat on top of the health line.)
        DrawText(
            game::game_mode_name(
                game_mode
            ),
            12,
            50,
            14,
            DARKGRAY
        );

        DrawText(
            player.third_person
                ? "TPS"
                : "FPS",
            12,
            68,
            14,
            BLACK
        );

        if(!island_life.empty() && !creator_active) {
            const auto clock_label=island_life.clock().label();
            const int clock_width=MeasureText(clock_label.c_str(),18);
            DrawText(clock_label.c_str(),screen_width/2-clock_width/2,10,18,BLACK);
            const Vector3 feet{player.position.x,player.position.y-1.0F,player.position.z};
            if(const auto* resident=poker_open ? nullptr : island_life.nearest(feet,3.5F)) {
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
        else if(red_dot_up) game::combat::CombatRenderer::draw_sight_view(sight,screen_width,screen_height,(fp_aim_blend-.6F)/.4F);
        // On iron sights the sights are the aim: the crosshair gives way to them.
        const bool irons_up=controls.aim && !driving && weapons.available() && sight==game::combat::Sight::Iron && !creator_active && fp_aim_blend>.7F;
        const int gap=static_cast<int>((controls.aim ? 3 : 8)+movement_amount*5+recoil_pitch*180);
        const Color crosshair_color=weapons.hit_marker()>0 ?
            (weapons.last_headshot() ? ORANGE : GREEN) : RAYWHITE;
        if(irons_up) {
            // Focus on the sights: the edges of the view darken a little.
            const auto edge=static_cast<unsigned char>(std::clamp((fp_aim_blend-.7F)/.3F,0.0F,1.0F)*90);
            const int band=screen_height/4;
            DrawRectangleGradientV(0,0,screen_width,band,Color{0,0,0,edge},Color{0,0,0,0});
            DrawRectangleGradientV(0,screen_height-band,screen_width,band,Color{0,0,0,0},Color{0,0,0,edge});
            DrawRectangleGradientH(0,0,band,screen_height,Color{0,0,0,edge},Color{0,0,0,0});
            DrawRectangleGradientH(screen_width-band,0,band,screen_height,Color{0,0,0,0},Color{0,0,0,edge});
        }
        if(!scoped && !red_dot_up && !irons_up) {
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
        // A binding's short name for the HUD ("LMB", "R", ...).
        const auto key_of=[&](input::Action action) {
            static std::string name;
            const int code=settings.bindings[action][0]!=0 ? settings.bindings[action][0] : settings.bindings[action][1];
            name=code==input::Bindings::mouse(MOUSE_BUTTON_LEFT) ? "LMB" : code==input::Bindings::mouse(MOUSE_BUTTON_RIGHT) ? "RMB" :
                code==input::Bindings::wheel ? "WHEEL" : input::Bindings::name(code);
            return name;
        };
        // Driving with a pistol or SMG: what is in the hand for a drive-by (over the speedometer).
        if(drive_by && !creator_active && !inventory_open) {
            const char* line=TextFormat("DRIVE-BY  %s  %d / %d   %s",weapons.weapon().name,weapons.ammo().loaded,weapons.ammo().reserve,
                touch_ui ? "" : "LMB FIRE - LOOK TO AIM");
            const int lw=MeasureText(line,18);
            DrawRectangle(12,screen_height-206,lw+24,30,Fade(BLACK,.6F));
            DrawText(line,24,screen_height-200,18,weapons.ammo().loaded>0 ? RAYWHITE : ORANGE);
        }
        if (!creator_active && !inventory_open && !build_mode.active() && !driving) {
            DrawRectangle(12,screen_height-103,320,91,Fade(BLACK,.65F));
            {
                // Long names (VR7 ROCKET LAUNCHER) shrink to stay clear of the sight label.
                const char* held=weapons.available()?weapons.weapon().name:"NO WEAPON - OPEN BAG";
                int size=19;
                while(size>12 && weapons.available() && MeasureText(held,size)>170) --size;
                DrawText(held,24,screen_height-94+(19-size)/2,size,RAYWHITE);
            }
            DrawText(TextFormat("NADES %d  %s",grenades.count(),touch_ui ? "" : key_of(input::Action::Grenade).c_str()),222,screen_height-68,16,grenades.count()>0 ? Color{190,220,150,255} : GRAY);
            if(const int armour=armour_level(); armour>0)
                DrawText(armour==2 ? "ARMOUR II  -45%" : "ARMOUR I  -30%",200,screen_height-112,14,game::inventory::LootUI::rarity_color(armour==2 ? game::inventory::Rarity::Epic : game::inventory::Rarity::Rare));
            if(weapons.available()) DrawText(TextFormat("%s  %s",game::combat::sight_info(sight).name,sight==game::combat::Sight::Sniper || touch_ui ? "" : key_of(input::Action::Sight).c_str()),
                200,screen_height-92,14,sight==game::combat::Sight::Iron ? GRAY : Color{235,120,110,255});
            if (weapons.unlimited()) DrawText("AMMO UNLIMITED - DEV LAB",24,screen_height-68,18,YELLOW);
            else DrawText(TextFormat("%d / %d",weapons.ammo().loaded,weapons.ammo().reserve),
                24,screen_height-68,22,RAYWHITE);
            if (weapons.reload_remaining()>0) DrawText(TextFormat("RELOADING %.1fs",weapons.reload_remaining()),
                24,screen_height-39,16,ORANGE);
            else if(touch_ui) DrawText(weapons.ammo().loaded==0 && !weapons.unlimited() ? "EMPTY - LOAD TO RELOAD" : "SWIPE RIGHT SIDE TO LOOK",
                24,screen_height-39,12,RAYWHITE);
            else {
                std::string line=weapons.ammo().loaded==0 && !weapons.unlimited() ? "EMPTY - "+key_of(input::Action::Reload)+" TO RELOAD" :
                    key_of(input::Action::Fire)+" FIRE  "+key_of(input::Action::Aim)+" AIM  "+key_of(input::Action::Reload)+" RELOAD  "+
                    key_of(input::Action::NextWeapon)+" SWAP";
                DrawText(line.c_str(),24,screen_height-39,12,RAYWHITE);
            }
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
        if(lan_match) {
            lan_play.draw_names(camera,screen_width,screen_height);
            if(game_mode!=game::GameMode::BattleRoyale) {
                const char* people=TextFormat("LAN - %d PLAYERS",1+static_cast<int>(lan_play.remotes().size()));
                DrawText(people,screen_width/2-MeasureText(people,18)/2,16,18,Color{120,220,255,235});
                int line=0;
                for(const auto& entry:br_bots.feed()) DrawText(entry.c_str(),screen_width/2-MeasureText(entry.c_str(),16)/2,40+20*line++,16,Color{235,215,180,255});
            }
        }
        if (!creator_active) {
            DrawText(TextFormat("HEALTH %.0f / %.0f",player_health.current(),player_health.maximum()),
                16,16,20,player_health.alive() ? RAYWHITE : RED);
            // The boost from food and drink: a thin amber bar under the health line while it lasts.
            if(loot_session.boost()>0 && player_health.alive()) {
                const float share=loot_session.boost()/game::inventory::LootSession::boost_max;
                DrawRectangle(16,40,160,5,Fade(BLACK,.5F));
                DrawRectangle(16,40,static_cast<int>(160*share),5,Color{240,180,60,230});
            }

            if (br_bots.active() && game_mode==game::GameMode::BattleRoyale) {
                const int alive=br_bots.alive_bots()+(player_health.alive() ? 1 : 0)+(lan_match ? lan_play.alive() : 0);
                const char* status=TextFormat("ALIVE %d   KILLS %d   BOTS %s%s",alive,br_bots.player_kills(),game::ai::bot_level_name(br_bots.level()),
                                              lan_match ? TextFormat("   LAN %d",1+static_cast<int>(lan_play.remotes().size())) : "");
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
        if (touch_ui && !inventory_open && !shop_open && !poker_open && !mode_session.terminal() && !asset_drawer_open && !pause_menu.is_open()) touch_hud.draw(
            controls,
            input_system.drawn_layout(screen_width,screen_height),
            screen_width,
            screen_height,
            !creator_active && !build_mode.active(),
            weapons.available() && weapons.weapon().automatic && !driving ? (weapons.single_fire() ? "SINGLE" : "AUTO") : nullptr,
            weapons.available() && !driving && sight!=game::combat::Sight::Sniper ?
                (sight==game::combat::Sight::RedDot ? "DOT" : sight==game::combat::Sight::Scope2x ? "2X" : sight==game::combat::Sight::Scope4x ? "4X" : "IRON") : nullptr,
            driving,
            creator_active,
            dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed
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
        if(!creator_active && !inventory_open && !shop_open && !poker_open && !mode_session.terminal()){
            const int nearby=loot_session.world().nearest(loot_feet,2.8F,reachable_pickup);
            if(nearby>=0){const auto& pickup=loot_session.world().state().pickups[static_cast<std::size_t>(nearby)];
                const auto* item=item_registry.find(pickup.item);
                if(item) {
                    const auto rarity=game::inventory::rarity_of(item->id);
                    const char* line=TextFormat("E / USE: %s x%d  %s",item->name.c_str(),pickup.quantity,game::inventory::rarity_name(rarity));
                    DrawRectangle(screen_width/2-190,screen_height-146,MeasureText(line,18)+20,30,Fade(BLACK,.5F));
                    DrawText(line,screen_width/2-180,screen_height-140,18,game::inventory::LootUI::rarity_color(rarity));
                }
            }
        }
        // La Mafio: gang tags over Krimuloj near by, whose turf this is, the news, the poker table.
        if(mafio.active() && !poker_open && !inventory_open && !shop_open) {
            const Font& font=sign_renderer.font();
            std::vector<Vector2> tagged; // one tag per spot on screen: a crew standing together shares one
            for(const auto& body:npcs.actors()) {
                if(body.bot<0 || body.hidden || body.state==characters::NpcState::Dead) continue;
                const int gang=mafio.gang_of(body.bot);
                if(gang<0 || Vector3Distance(body.position,camera.position)>22) continue;
                const Vector3 over{body.position.x,body.position.y+2.15F,body.position.z};
                if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) continue;
                const Vector2 at=GetWorldToScreen(over,camera);
                if(std::any_of(tagged.begin(),tagged.end(),[&](Vector2 t){return std::abs(t.x-at.x)<150 && std::abs(t.y-at.y)<24;})) continue;
                tagged.push_back(at);
                const bool angry=mafio.provoked(gang,match_time);
                const std::string tag=std::string(angry ? "! " : "")+std::string(game::mafia::gangs[static_cast<std::size_t>(gang)].name);
                const float tw=MeasureTextEx(font,tag.c_str(),15,1).x;
                DrawRectangle(static_cast<int>(at.x-tw/2-5),static_cast<int>(at.y-2),static_cast<int>(tw+10),19,Fade(BLACK,.45F));
                DrawTextEx(font,tag.c_str(),{at.x-tw/2,at.y},15,1,angry ? Color{255,90,80,255} : game::mafia::gangs[static_cast<std::size_t>(gang)].color);
            }
            // Paul Acosta: his name and what he is up to, close by.
            if(walking_man.active()) for(const auto& body:npcs.actors()) {
                if(body.bot!=walking_man.bot() || body.hidden || body.state==characters::NpcState::Dead) continue;
                if(Vector3Distance(body.position,camera.position)>26) break;
                const Vector3 over{body.position.x,body.position.y+2.2F,body.position.z};
                if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) break;
                const Vector2 at=GetWorldToScreen(over,camera);
                const bool angry=walking_man.angry(match_time);
                const std::string title=std::string(angry ? "! " : "")+"PAUL ACOSTA  \"the walking man\"";
                const std::string sub=walking_man.activity();
                const float tw=MeasureTextEx(font,title.c_str(),15,1).x,sw=MeasureTextEx(font,sub.c_str(),13,1).x,w=std::max(tw,sw);
                DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),36,Fade(BLACK,.5F));
                DrawTextEx(font,title.c_str(),{at.x-tw/2,at.y-16},15,1,angry ? Color{255,90,80,255} : Color{240,214,150,255});
                DrawTextEx(font,sub.c_str(),{at.x-sw/2,at.y+1},13,1,Color{220,226,206,255});
                break;
            }
            // Vakeroj: name and ranch over each one close by (red when the family is after you).
            if(ranches.active()) for(const auto& body:npcs.actors()) {
                if(body.bot<0 || body.hidden || body.state==characters::NpcState::Dead) continue;
                const auto* v=ranches.vakero(body.bot);
                if(!v || Vector3Distance(body.position,camera.position)>20) continue;
                const Vector3 over{body.position.x,body.position.y+2.25F,body.position.z};
                if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) continue;
                const Vector2 at=GetWorldToScreen(over,camera);
                const bool angry=ranches.angry(v->ranch,match_time);
                const std::string tag=std::string(angry ? "! " : "")+v->name+"  ·  Vakero";
                const float tw=MeasureTextEx(font,tag.c_str(),15,1).x;
                DrawRectangle(static_cast<int>(at.x-tw/2-5),static_cast<int>(at.y-2),static_cast<int>(tw+10),19,Fade(BLACK,.45F));
                DrawTextEx(font,tag.c_str(),{at.x-tw/2,at.y},15,1,angry ? Color{255,90,80,255} : Color{226,190,120,255});
            }
            // News from the ranches and the big rigs.
            for(int row=0;row<2;++row) {
                const std::string& text=row==0 ? ranches.message() : freight.message();
                const float time=row==0 ? ranches.message_time() : freight.message_time();
                if(time<=0) continue;
                const float alpha=std::clamp(time,0.0F,1.0F);
                const float mw=MeasureTextEx(font,text.c_str(),18,1).x;
                const int y=screen_height-(row==0 ? 312 : 348);
                DrawRectangle(static_cast<int>(screen_width/2-mw/2-12),y-5,static_cast<int>(mw+24),28,Fade(Color{40,30,10,255},.65F*alpha));
                DrawTextEx(font,text.c_str(),{screen_width/2-mw/2,static_cast<float>(y)},18,1,Fade(Color{240,214,150,255},alpha));
            }
            if(walking_man.message_time()>0) {
                const float alpha=std::clamp(walking_man.message_time(),0.0F,1.0F);
                const float mw=MeasureTextEx(font,walking_man.message().c_str(),18,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-mw/2-12),screen_height-276,static_cast<int>(mw+24),28,Fade(Color{40,30,10,255},.65F*alpha));
                DrawTextEx(font,walking_man.message().c_str(),{screen_width/2-mw/2,static_cast<float>(screen_height-271)},18,1,Fade(Color{255,226,160,255},alpha));
            }
            const int turf=mafio.turf_at({player.position.x,player.position.y-1,player.position.z},30);
            if(turf>=0) {
                const auto& crew=mafio.crews()[static_cast<std::size_t>(turf)];
                const auto& gang=game::mafia::gangs[static_cast<std::size_t>(crew.gang)];
                const std::string title=std::string(gang.name)+"  ·  "+crew.town+" crew";
                const std::string sub=career.in_gang(crew.gang) ? std::string("Your crew - wait here for the word, bring the rig here") :
                    mafio.provoked(crew.gang,match_time) ? std::string("They are shooting at you - the police won't come") :
                    !career_path.empty() ? std::string(gang.meaning)+" - USE at the door to join them" :
                    std::string(gang.meaning)+" - leave them be and they leave you be";
                const float tw=MeasureTextEx(font,title.c_str(),20,1).x,sw=MeasureTextEx(font,sub.c_str(),15,1).x;
                // Left, under the money line: clear of the compass and the touch buttons.
                const float career_room=career.state().side==game::career::Side::Civilian ? 0.0F :
                    career.job().kind!=game::career::JobKind::None ? 96.0F : 78.0F; // under the career panel
                const float w=std::max(tw,sw)+30,x=10,y=screen_height*.235F+career_room;
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),static_cast<int>(w),50,Fade(BLACK,.5F));
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),5,50,gang.color);
                DrawTextEx(font,title.c_str(),{x+15,y+5},20,1,gang.color);
                DrawTextEx(font,sub.c_str(),{x+15,y+28},15,1,Color{220,226,206,255});
            }
            // At a police station: how to sign up (or why not).
            if(!career_path.empty() && !career.on_duty() && services.active() &&
               services.station_near({player.position.x,player.position.y-1,player.position.z},14) && !vehicles.driver()) {
                const char* line=career.state().record ? "POLICIO DE VERDA  ·  no Krimuloj on the force - you have a record" :
                                                         "POLICIO DE VERDA  ·  USE to join: a patrol car and radio calls";
                const float lw=MeasureTextEx(font,line,17,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-lw/2-12),screen_height-246,static_cast<int>(lw+24),28,Fade(BLACK,.55F));
                DrawTextEx(font,line,{screen_width/2-lw/2,static_cast<float>(screen_height-241)},17,1,career.state().record ? Color{255,140,120,255} : Color{150,190,255,255});
            }
            // The career: side and rank, the respect meter, the job in hand, and the news.
            if(career.state().side!=game::career::Side::Civilian) {
                const bool cop=career.on_duty();
                const Color tint=cop ? Color{110,160,255,255} : game::mafia::gangs[static_cast<std::size_t>(std::max(0,career.state().gang))].color;
                const std::string who=std::string(cop ? "POLICIO DE VERDA" : std::string(game::mafia::gangs[static_cast<std::size_t>(career.state().gang)].name))+"  ·  "+career.rank_name();
                const auto& job=career.job();
                std::string task,status;
                if(job.kind!=game::career::JobKind::None) {
                    task=std::string(job.title)+":  "+job.detail;
                    const int left=static_cast<int>(std::max(0.0,job.deadline-match_time));
                    const float metres=std::hypot(job.target.x-player.position.x,job.target.z-player.position.z);
                    status=(job.kind==game::career::JobKind::RobRig ? std::string(job.taken ? "rig taken - to the hideout" : "find the rig") :
                            std::to_string(job.done)+" of "+std::to_string(job.needed))+
                        TextFormat("   ·   %d:%02d left   ·   %.0f m",left/60,left%60,metres);
                } else status=cop ? "On patrol - waiting for a call" : "Waiting for the word";
                const float x=10,y=screen_height*.235F;
                const float w=std::max({340.0F,MeasureTextEx(font,task.c_str(),15,1).x+24,MeasureTextEx(font,who.c_str(),18,1).x+24});
                const float h=task.empty() ? 70.0F : 88.0F;
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),static_cast<int>(w),static_cast<int>(h),Fade(BLACK,.55F));
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),5,static_cast<int>(h),tint);
                DrawTextEx(font,who.c_str(),{x+14,y+5},18,1,tint);
                // The respect meter: ticks at each rank.
                const float bar=200,share=career.state().respect/game::career::Career::respect_max;
                DrawRectangle(static_cast<int>(x+14),static_cast<int>(y+29),static_cast<int>(bar),8,Fade(WHITE,.15F));
                DrawRectangle(static_cast<int>(x+14),static_cast<int>(y+29),static_cast<int>(bar*share),8,tint);
                for(const float tick:{15.0F,40.0F,70.0F,95.0F}) DrawRectangle(static_cast<int>(x+14+bar*tick/100),static_cast<int>(y+27),1,12,Fade(WHITE,.6F));
                DrawTextEx(font,TextFormat("RESPECT %d",static_cast<int>(career.state().respect)),{x+22+bar,y+25},15,1,RAYWHITE);
                if(!task.empty()) DrawTextEx(font,task.c_str(),{x+14,y+44},15,1,Color{255,232,180,255});
                DrawTextEx(font,status.c_str(),{x+14,y+(task.empty() ? 46.0F : 64.0F)},14,1,Color{205,212,196,255});
            }
            if(career.message_time()>0) {
                const float alpha=std::clamp(career.message_time(),0.0F,1.0F);
                const float mw=MeasureTextEx(font,career.message().c_str(),19,1).x;
                const Color tint=career.on_duty() ? Color{150,190,255,255} : Color{255,210,120,255};
                DrawRectangle(static_cast<int>(screen_width/2-mw/2-14),screen_height-206,static_cast<int>(mw+28),30,Fade(BLACK,.6F*alpha));
                DrawTextEx(font,career.message().c_str(),{screen_width/2-mw/2,static_cast<float>(screen_height-201)},19,1,Fade(tint,alpha));
            }
            if(mafio.message_time()>0) {
                const float alpha=std::clamp(mafio.message_time(),0.0F,1.0F);
                const float mw=MeasureTextEx(font,mafio.message().c_str(),18,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-mw/2-12),screen_height-240,static_cast<int>(mw+24),28,Fade(Color{60,10,10,255},.65F*alpha));
                DrawTextEx(font,mafio.message().c_str(),{screen_width/2-mw/2,static_cast<float>(screen_height-235)},18,1,Fade(Color{255,215,170,255},alpha));
            }
            if(at_table) {
                const char* line="E / USE: PLAY POKER WITH LA VERDA MANO (buy-in up to 300 kr)";
                DrawRectangle(screen_width/2-MeasureText(line,18)/2-10,screen_height-176,MeasureText(line,18)+20,30,Fade(BLACK,.5F));
                DrawText(line,screen_width/2-MeasureText(line,18)/2,screen_height-170,18,Color{120,230,150,255});
            }
        }
        if (interaction_remaining > 0)
            DrawText(interaction_message.c_str(), screen_width / 2 - MeasureText(interaction_message.c_str(),18)/2, screen_height-205, 18, RAYWHITE);

        // MENU (pause, settings, controls, save, quit); N still mutes.
        DrawRectangleRec(audio_button, Fade(BLACK, 0.60F));
        DrawText(touch_ui ? "MENU" : "MENU · ESC", screen_width - 165, 53, 14, RAYWHITE);
        if(environment_audio.ready() && environment_audio.muted()) DrawText("MUTED", screen_width - 62, 53, 14, Color{230,120,100,255});
        if(game_mode!=game::GameMode::DevLab) {
            const auto map_button=game::MapScreen::map_button(screen_width,screen_height);
            DrawRectangleRec(map_button,Fade(BLACK,.60F));
            DrawText("MAP · M",static_cast<int>(map_button.x)+14,53,14,RAYWHITE);
            if(game::rules_for(game_mode).civilian_life && !map_screen.is_open() && !inventory_open && !shop_open)
                build_mode.draw_hud(player_builds,screen_width,screen_height);
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
        if(const auto* driver=vehicles.driver()) {
            game::hud::Speedometer gauge;
            gauge.speed=driver->speed;gauge.boosting=driver->boosting;gauge.handbrake=controls.crouch;
            if(const auto* asset=vehicles.asset(verda_region,driver->id)) {
                if(const auto* definition=vehicles.definition(*asset)) gauge.top_speed=definition->top_speed;
                gauge.fuel=asset->vehicle.fuel;
            }
            // Bottom middle, between the steering stick and the pedals.
            game::hud::draw_speedometer(gauge,{screen_width*.42F,screen_height-92.0F},66);
            if(!touch_ui)
                DrawText("W/S drive | A/D steer | SPACE handbrake | SHIFT boost | C park | H horn | E exit",
                         static_cast<int>(screen_width*.42F)+84,screen_height-26,15,YELLOW);
        }
        else if(!creator_active && !inventory_open && vehicles.nearest(verda_region,{player.position.x,player.position.y-1,player.position.z})>=0)DrawText("USE / E - Enter vehicle",screen_width/2-120,screen_height-100,20,YELLOW);
        if(game_mode!=game::GameMode::DevLab && !mode_session.terminal() && !shop_open && !poker_open && !inventory_open) {
            const int objective_bottom=game::PlayUI::draw_objective(mode_session,screen_width);
            if(game_mode==game::GameMode::BattleRoyale) {
                // The zone: its phase, the countdown, and how far the next circle is.
                const auto zone=mode_session.zone();
                const int phases=static_cast<int>(mode_session.circle.phases.size());
                const int left=static_cast<int>(std::ceil(zone.remaining));
                const float to_next=std::hypot(loot_feet.x-zone.next_center.x,loot_feet.z-zone.next_center.y)-zone.next_radius;
                const char* line=zone.phase>=phases ? TextFormat("FINAL ZONE  |  %.0f HP/s outside",zone.damage) :
                    zone.shrinking ? TextFormat("PHASE %d/%d  |  ZONE CLOSING %d:%02d",zone.phase+1,phases,left/60,left%60) :
                                     TextFormat("PHASE %d/%d  |  ZONE CLOSES IN %d:%02d",zone.phase+1,phases,left/60,left%60);
                const std::string first=line;
                const char* where=to_next>0 ? TextFormat("NEXT ZONE %.0f m away",to_next) : "INSIDE THE NEXT ZONE";
                DrawRectangle(12,objective_bottom+1,std::max(MeasureText(first.c_str(),18),MeasureText(where,16)+24)+16,48,Fade(BLACK,.5F));
                DrawText(first.c_str(),20,objective_bottom+6,18,zone.shrinking ? Color{255,170,70,255} : RAYWHITE);
                DrawText(where,44,objective_bottom+28,16,to_next>0 ? Color{255,255,255,230} : Color{140,230,150,255});
                // An arrow pointing the way to the next zone, turned with the view.
                const float want=std::atan2(zone.next_center.x-loot_feet.x,zone.next_center.y-loot_feet.z);
                const Vector3 look=Vector3Subtract(camera.target,camera.position);
                const float turn=std::remainder(want-std::atan2(look.x,look.z),2*PI);
                const Vector2 at{30,static_cast<float>(objective_bottom+36)};
                const Vector2 dir{-std::sin(turn),-std::cos(turn)}, side{-dir.y,dir.x};
                const auto tip=Vector2Add(at,Vector2Scale(dir,9)),l=Vector2Add(Vector2Subtract(at,Vector2Scale(dir,6)),Vector2Scale(side,6)),r=Vector2Subtract(Vector2Subtract(at,Vector2Scale(dir,6)),Vector2Scale(side,6));
                const Color arrow=to_next>0 ? Color{255,255,255,240} : Color{140,230,150,255};
                DrawTriangle(tip,r,l,arrow);DrawTriangle(tip,l,r,arrow);
            }
            if(game_mode==game::GameMode::Explore) {
                DrawText(TextFormat("%lld kronoj | stores: BUY supplies / SELL scavenged goods",static_cast<long long>(loot_session.economy().money)),20,objective_bottom+6,16,GOLD);
                if(nearby_shop && !at_table && !poker_open) DrawText(TextFormat("E / USE: %s",nearby_shop->name.c_str()),screen_width/2-160,screen_height-165,20,GOLD);
                if(penthouse.inside(loot_feet)) {
                    const bool owned=game::economy::Penthouse::owned(loot_session.economy());
                    const char* line=!owned ? TextFormat("PENTHOUSE FOR SALE - %lld kronoj   E / USE: BUY",static_cast<long long>(game::economy::Penthouse::price)) :
                        penthouse.near_bed(loot_feet) ? "E / USE: SLEEP UNTIL MORNING" : nullptr;
                    if(line) {
                        const int tw=MeasureText(line,22);
                        DrawRectangle(screen_width/2-tw/2-14,screen_height-252,tw+28,34,Fade(BLACK,.6F));
                        DrawText(line,screen_width/2-tw/2,screen_height-246,22,GOLD);
                    }
                }
            }
        }
        if(shop_open && current_shop) play_ui.draw_shop(economy,*current_shop,loot_session.economy(),item_registry,loot_session.inventory(),shop_status,screen_width,screen_height);
        if(mode_session.terminal()) play_ui.draw_result(mode_session,screen_width,screen_height);
        // Outside the circle: red at the edges of the screen, pulsing, and the way back.
        if(game_mode==game::GameMode::BattleRoyale && !mode_session.terminal() && player_health.alive()) {
            const float outside=mode_session.distance(loot_feet)-mode_session.radius();
            if(outside>0 && !(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed)) {
                // Inside the gas: the whole view goes blue, deeper as the damage rises.
                DrawRectangle(0,0,screen_width,screen_height,Color{70,110,255,static_cast<unsigned char>(std::clamp(30+mode_session.zone_damage()*3,30.0F,90.0F))});
                const float pulse=.55F+.45F*std::sin(static_cast<float>(GetTime())*5);
                const auto edge=static_cast<unsigned char>(110*pulse);
                const int band=std::max(40,screen_height/7);
                DrawRectangleGradientV(0,0,screen_width,band,Color{90,140,255,edge},Color{90,140,255,0});
                DrawRectangleGradientV(0,screen_height-band,screen_width,band,Color{90,140,255,0},Color{90,140,255,edge});
                DrawRectangleGradientH(0,0,band,screen_height,Color{90,140,255,edge},Color{90,140,255,0});
                DrawRectangleGradientH(screen_width-band,0,band,screen_height,Color{90,140,255,0},Color{90,140,255,edge});
                const char* warning=TextFormat("OUTSIDE THE ZONE - %.0f m TO SAFETY - %.0f HP/s",outside,mode_session.zone_damage());
                const int ww=MeasureText(warning,22);
                DrawRectangle(screen_width/2-ww/2-12,screen_height/2+90,ww+24,34,Fade(BLACK,.6F));
                DrawText(warning,screen_width/2-ww/2,screen_height/2+96,22,Color{150,190,255,255});
            }
        }
        // The compass, in every circumstance: where the camera looks.
        if(!map_screen.is_open() && !poker_open)
            game::hud::draw_compass(game::hud::bearing(Vector3Subtract(camera.target,camera.position)),screen_width,32);
        // Battle Royale: a banner when a new circle is marked and when the zone starts to close.
        if(game_mode==game::GameMode::BattleRoyale && !mode_session.terminal() && !map_screen.is_open()) {
            const auto zone=mode_session.zone();
            if(zone.phase!=zone_seen_phase || zone.shrinking!=zone_seen_shrinking) {
                if(zone_seen_phase>=0) {
                    circle_banner=zone.shrinking ? "THE ZONE IS CLOSING" : zone.phase>=static_cast<int>(mode_session.circle.phases.size()) ? "FINAL ZONE" : "NEW SAFE ZONE MARKED";
                    circle_banner_time=3.5F;gun_audio.zone_warning();
                }
                zone_seen_phase=zone.phase;zone_seen_shrinking=zone.shrinking;
            }
            if(circle_banner_time>0) {
                circle_banner_time-=dt;
                const int size=34,tw=MeasureText(circle_banner.c_str(),size);
                const auto alpha=static_cast<unsigned char>(255*std::clamp(circle_banner_time,0.0F,1.0F));
                DrawRectangle(screen_width/2-tw/2-20,screen_height/4-10,tw+40,size+20,Color{20,30,60,static_cast<unsigned char>(alpha*.7F)});
                DrawText(circle_banner.c_str(),screen_width/2-tw/2,screen_height/4,size,Color{170,210,255,alpha});
            }
        }
        // The drop's own HUD: what to press, the route, an altimeter.
        if(game_mode==game::GameMode::BattleRoyale && dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed &&
           !mode_session.terminal() && !map_screen.is_open() && !pause_menu.is_open()) {
            using game::aircraft::DiveStage;
            const auto centred=[&](const char* text,int y,int size,Color colour) {
                const int tw=MeasureText(text,size);
                DrawRectangle(screen_width/2-tw/2-14,y-6,tw+28,size+12,Fade(BLACK,.55F));
                DrawText(text,screen_width/2-tw/2,y,size,colour);
            };
            const int base=screen_height-(touch_ui ? 250 : 150);
            if(skydiver.stage()==DiveStage::Aboard) {
                const auto& ride=drop_fleet[static_cast<std::size_t>(drop_ride)];
                const int jumpers=static_cast<int>(std::count_if(bot_jumps.begin(),bot_jumps.end(),[&](const auto& j){return j.helicopter==drop_ride && drop_clock<j.jump_time;}))+1;
                centred(TextFormat("HELICOPTER %d OF %d: %s  |  %d ABOARD",drop_ride+1,static_cast<int>(drop_fleet.size()),game::aircraft::heading_name(ride.heading),jumpers),112,18,Color{255,220,120,255});
                centred(touch_ui ? "TAP JUMP TO LEAVE THE HELICOPTER" : "SPACE: JUMP   |   M: MAP (the bright line is your route)",base,24,RAYWHITE);
                if(drop_clock<switch_window && drop_fleet.size()>1)
                    centred(TextFormat(touch_ui ? "TAP LOAD TO SWITCH HELICOPTER (%d s)" : "R: SWITCH HELICOPTER (%d s)",static_cast<int>(std::ceil(switch_window-drop_clock))),base+40,20,Color{170,220,255,255});
                const float left=game::aircraft::last_jump_time(ride)-drop_clock;
                if(left<15) centred(TextFormat("LAST LAND AHEAD - AUTO JUMP IN %d",static_cast<int>(std::ceil(std::max(0.0F,left)))),base+40,20,Color{255,150,90,255});
            } else {
                const Vector3 body=skydiver.position();
                const float ground=world::physics::WorldCollision::ground_height(body,body.y-1,verda_region);
                const float height=std::max(0.0F,skydiver.height_above(ground));
                if(skydiver.stage()==DiveStage::Freefall) {
                    centred(skydiver.can_deploy() ? (touch_ui ? "TAP JUMP TO OPEN THE PARACHUTE" : "SPACE: OPEN THE PARACHUTE") : "FREEFALL",base,24,RAYWHITE);
                    centred("FORWARD: GLIDE   |   LOOK DOWN + FORWARD: DIVE",base+40,17,Color{220,220,220,255});
                } else {
                    centred("STEER WHERE YOU LOOK   |   FORWARD: FASTER   |   BACK: BRAKE",base,18,RAYWHITE);
                }
                // Altimeter on the right: height above the ground, the auto-open mark, the speed.
                const int bar_x=screen_width-70,bar_top=screen_height/2-130,bar_h=260;
                DrawRectangle(bar_x-6,bar_top-30,62,bar_h+70,Fade(BLACK,.5F));
                DrawRectangle(bar_x+14,bar_top,10,bar_h,Fade(WHITE,.25F));
                const auto y_of=[&](float metres){return bar_top+bar_h-static_cast<int>(std::clamp(metres/700.0F,0.0F,1.0F)*bar_h);};
                DrawRectangle(bar_x+8,y_of(game::aircraft::Skydiver::auto_open)-1,22,3,Color{255,170,60,255});
                DrawRectangle(bar_x+6,y_of(height)-3,26,6,Color{120,220,255,255});
                DrawText(TextFormat("%.0f m",height),bar_x-2,bar_top-24,16,RAYWHITE);
                DrawText(TextFormat("%.0f km/h",Vector3Length(skydiver.velocity())*3.6F),bar_x-4,bar_top+bar_h+8,13,LIGHTGRAY);
            }
        }
        if(poker_open) poker_table.draw(poker,loot_session.economy().money-poker_buy_in,"LA VERDA MANO",screen_width,screen_height,static_cast<float>(GetTime()));
        if(map_screen.is_open()) {
            game::MapScreen::View view;
            view.player=player.position;view.yaw=player.yaw;view.font=&sign_renderer.font();
            if(game_mode==game::GameMode::BattleRoyale) {
                const auto zone=mode_session.zone();
                view.circle_radius=zone.radius;view.circle_center=zone.center;
                if(zone.next_radius<zone.radius-1) {view.next_radius=zone.next_radius;view.next_center=zone.next_center;}
                if(dropping && drop_flying) {
                    view.flight=true;
                    for(const auto& path:drop_fleet) {
                        const bool mine=path.index==drop_ride && skydiver.stage()==game::aircraft::DiveStage::Aboard;
                        view.flights.push_back({path.start,path.end,game::aircraft::helicopter_position(path,drop_clock),mine});
                    }
                }
            }
            for(const auto& stop:bus_line.stops()) view.stops.push_back(stop.shelter);
            if(bus_line.running()) for(const auto& bus:bus_line.buses()) if(bus.running) view.buses.push_back({bus.position,bus.yaw});
            for(const auto& crew:mafio.crews()) view.turf.push_back({crew.door,game::mafia::gangs[static_cast<std::size_t>(crew.gang)].color});
            if(game::rules_for(game_mode).civilian_life && penthouse.found()) {
                view.home=penthouse.to_world({0,0,0});
                view.home_owned=game::economy::Penthouse::owned(loot_session.economy());
            }
            map_screen.draw(verda_region,view,settings,screen_width,screen_height);
        }
        // Mouse look waits for a click in the browser (pointer lock needs one).
        if(mouse_wanted && !mouse_captured && !pause_menu.is_open() && game_mode!=game::GameMode::Home) {
            const char* line="CLICK TO PLAY  -  the mouse looks around, ESC for the menu";
            const int size=22, tw=MeasureText(line,size);
            DrawRectangle(screen_width/2-tw/2-16,screen_height/2-64,tw+32,40,Fade(BLACK,.7F));
            DrawText(line,screen_width/2-tw/2,screen_height/2-55,size,RAYWHITE);
        }
        pause_menu.draw(settings,screen_width,screen_height,&sign_renderer.font(),can_quit_game);
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
