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
#include "outland/game/mafia/Territory.hpp"
#include "outland/game/mafia/Endgame.hpp"
#include "outland/game/mafia/GangLogos.hpp"
#include "outland/game/career/Career.hpp"
#include "outland/game/career/Backup.hpp"
#include "outland/game/career/Armory.hpp"
#include "outland/game/law/HitAndRun.hpp"
#include "outland/game/law/JailTerms.hpp"
#include "outland/game/law/PrisonYard.hpp"
#include "outland/game/career/TrainingCamp.hpp"
#include "outland/game/career/CampRenderer.hpp"
#include "outland/game/life/WalkingMan.hpp"
#include "outland/game/ranch/Ranches.hpp"
#include "outland/game/freight/Freight.hpp"
#include "outland/game/freight/Trucking.hpp"
#include "outland/game/player/PlayerStats.hpp"
#include "outland/game/campaign/Campaign.hpp"
#include "outland/game/TouchLayoutEditor.hpp"
#include <ctime>
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/game/mafia/MafioRenderer.hpp"
#include "outland/game/services/EmergencyServices.hpp"
#include "outland/game/ZoneWall.hpp"
#include "outland/game/inventory/LootSave.hpp"
#include "outland/game/inventory/LootSession.hpp"
#include "outland/game/inventory/LootUI.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/game/vehicles/VehicleRenderer.hpp"
#include "outland/game/vehicles/VehicleAudio.hpp"
#include "outland/game/radio/RadioAudio.hpp"
#include "outland/game/career/Crew.hpp"
#include "outland/game/career/Callouts.hpp"
#include "outland/assets/ModelCache.hpp"
#include "outland/engine/render/WorldShading.hpp"
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
#include <optional>
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/aircraft/HelicopterDrop.hpp"
#include "outland/game/aircraft/DropRenderer.hpp"
#include "outland/game/economy/Penthouse.hpp"
#include "outland/game/economy/SkyMansion.hpp"
#include "outland/game/economy/Stash.hpp"
#include "outland/world/ParkingGarage.hpp"
#include "outland/world/Ballparks.hpp"
#include "outland/game/sports/BallGame.hpp"
#include "outland/game/sports/League.hpp"
#include "outland/game/economy/Homes.hpp"
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
#include "outland/engine/render/SignalRender.hpp"
#include "outland/game/culture/Faith.hpp"
#include "outland/game/culture/Gossip.hpp"
#include "outland/game/racing/StreetRace.hpp"
#include "outland/game/campaign/Phone.hpp"
#include "outland/world/Highways.hpp"
#include "outland/game/services/HighwayPatrol.hpp"
#include "outland/game/interiors/Furnishing.hpp"
#include "outland/game/wardrobe/StoreUI.hpp"
#include "outland/game/autos/Autos.hpp"
#include "outland/game/autos/AutoMenu.hpp"
#include "outland/world/MotorTrade.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/game/zombies/ZombieEcology.hpp"
#include "outland/game/outbreak/OutbreakRun.hpp"
#include "outland/game/LanScreen.hpp"
#include "outland/game/lan/LanPlay.hpp"
#include "outland/game/lan/SquadRevive.hpp"
#include "outland/net/LanSession.hpp"
#include <random>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <array>
#include <filesystem>
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
        render::shade_model(model);
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

    render::shading_initialize();

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
    game::TouchLayoutEditor touch_editor;   // SETTINGS > Touch layout > EDIT
#ifdef OUTLAND_DEV_TOOLS
    // OUTLAND_DEV_SLOT=N: CAMPAIGN goes straight into save slot N (screenshots).
    if(const char* slot=std::getenv("OUTLAND_DEV_SLOT")) home_screen.set_auto_slot(std::atoi(slot));
#endif
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
    game::career::Backup backup;     // an officer's radio call: two officers in a squad car
    game::career::Crew player_crew;  // the player's own: up to four gang recruits, or a partner on the force
    bool riding_now=false;           // in the passenger seat while the crew drives
    int bus_ride=-1;                 // the bus the player rides (game::transit::BusLine), -1 on foot
    characters::AnimationController bus_rider_pose;   // the people sitting on the buses
    // Ŝtata Malliberejo, the state prison (world::layout::prison_*): a point in its frame (x across,
    // z from the gate (-) to the back), on the ground; and the time an inmate gets out (-1: free).
    const auto prison_at=[](float x,float z) {
        const Vector3 c=world::layout::prison_center;
        const float a=world::layout::prison_yaw*DEG2RAD;
        const Vector3 p{c.x+x*std::cos(a)+z*std::sin(a),0,c.z-x*std::sin(a)+z*std::cos(a)};
        return Vector3{p.x,world::terrain::TerrainHeight::sample(p.x,p.z)+.15F,p.z};
    };
    const auto inside_prison=[](Vector3 p) {
        const Vector3 c=world::layout::prison_center;
        const float a=world::layout::prison_yaw*DEG2RAD,dx=p.x-c.x,dz=p.z-c.z;
        const float x=dx*std::cos(a)-dz*std::sin(a),z=dx*std::sin(a)+dz*std::cos(a);
        return std::abs(x)<40.5F && std::abs(z)<30.5F;
    };
    const Vector3 prison_intake=prison_at(0,-42);   // outside the gate: where officers hand prisoners over
    double prison_until=-1;
    game::law::PrisonYard prison_yard;   // its guards and inmates
    game::law::JailTerms jail_terms;     // ...and how long the island's people booked in there stay (one game week)
    // La Verda Basballigo (game::sports): the county clubs' league, their parks and buses, the
    // crowds in the stands (drawn, not simulated), fights, the championship's riot.
    game::sports::League league;
    std::array<world::ballpark::Site,5> parks{};
    std::string league_path,league_saved;
    std::array<double,5> park_fight_until{},park_cheer_home{},park_cheer_away{};   // (match_time)
    // Each park's scoreboard in centre field: the line score drawn into a texture (redrawn when it
    // changes), shown on the board's face while a game is on.
    struct ParkBoards {
        std::array<RenderTexture2D,5> face{};
        std::array<std::string,5> shows{};
        std::array<bool,5> on{};
        ~ParkBoards() { for(auto& f:face) if(f.id) UnloadRenderTexture(f); }
    } park_boards;
    // Who's going to each game (island life's residents): their seat in the home or visitors' stand.
    struct GameFan { int resident; bool home; int seat; };
    std::map<int,std::vector<GameFan>> game_fans;   // by fixture, this season
    constexpr int rioter_base=970000;
    std::vector<int> rioters;
    int riot_park=-1,riot_team=0;
    double league_bus_sync=0;
    struct StandSeat { bool on{false}; Vector3 at{}; float yaw{0}; } stand_seat;   // the player sitting in the stands
    characters::AnimationController crowd_pose,cheer_pose,brawl_pose,field_pose,run_pose,walk_pose;
    // Each ballplayer's own gait and facing (by park, side, role and index): strides that match how
    // fast he is really going and fall out of step with the next man's, and turns that take a moment.
    struct FieldBody { characters::AnimationController gait; float yaw{0}; double seen{-1e9}; };
    std::unordered_map<std::uint64_t,FieldBody> field_bodies;
    // The nearest free seat in a park's stands within reach of `feet` (false: none).
    const auto stand_seat_near=[&](Vector3 feet,Vector3& seat,float& yaw) {
        float best=2.6F;   // from the foot of the stand, or a row up
        bool found=false;
        for(const auto& site:parks) {
            if(site.team<0 || std::hypot(feet.x-site.origin.x,feet.z-site.origin.z)>90) continue;
            for(const bool home:{true,false})
                for(int row=0;row<world::ballpark::rows;++row)
                    for(int place=0;place<world::ballpark::seats_per_row;++place) {
                        const Vector3 s=site.seat(home,row,place);
                        const float d=std::hypot(s.x-feet.x,s.z-feet.z);
                        if(d<best && std::abs(s.y-feet.y)<1.6F) {best=d;seat=s;yaw=site.seat_yaw(home);found=true;}
                    }
        }
        return found;
    };
    // Who wears which body there: guards in the Policio's navy, gangsters, the island's law breakers.
    const auto prison_body=[&](bool guard,bool gang,std::uint64_t seed) -> std::string {
        if(guard) return seed%2 ? "character_18_police" : "character_21_police";
        const auto* d=character_registry.choose(gang ? characters::CharacterPool::Hostile : characters::CharacterPool::Civilian,seed);
        return d ? d->id : std::string("bot");
    };
    const auto prison_spawn=[&](const game::law::PrisonPerson& p) {
        npcs.spawn_bot(p.bot,p.character,prison_yard.world(p),prison_yard.world_yaw(p));
        npcs.set_bot_weapon(p.bot,p.guard ? static_cast<int>(game::combat::WeaponId::Rifle) : -1);
    };
    // Where the player sleeps inside: a cell on the block's back row, or (after a shanking) the SHU.
    const auto prison_cell=[&](bool shu) {
        using namespace world::layout;
        return shu ? prison_at(prison_shu_mid-1.6F,22.4F) : prison_at(prison_cell_x0+5*prison_cell_w-1.9F,22.6F);
    };
    const auto prison_bunk=[&](bool shu) {
        using namespace world::layout;
        return shu ? prison_at(prison_shu_mid-.7F,23.5F) : prison_at(prison_cell_x0+5*prison_cell_w-.6F,23.6F);
    };
    // Crime calls for an officer (game::career::Callouts): the places they happen, the call in hand.
    game::career::Callouts callouts;
    std::vector<game::career::CallScene> call_scenes;
    // Busted: the ride in (a fade), the booking photo and prison number, then a word from a guard
    // (and, for a gang member, the gang's word on a snitch in the yard). A shanking ends in the SHU.
    struct Booking { int stage{-1}; float t{0}; float sentence{0}; } booking;
    struct Mugshot { RenderTexture2D target{}; ~Mugshot() { if(target.id) UnloadRenderTexture(target); } } mugshot;
    std::vector<game::campaign::Line> inside_lines;
    std::size_t inside_line=0;
    bool in_shu=false;
    float sleep_fade=-1;          // sleeping off the sentence: the screen goes dark, then out the gate
    // An officer's first drop-off: the warden shows them round (a camera tour, a line at a time).
    struct PrisonTour { int line{-1}; float t{0}; Vector3 eye{}, look{}; } prison_tour;
    static const std::array<const char*,5> tour_lines{{
        "So you're the officer bringing me guests. Welcome to Ŝtata Malliberejo - the only prison on Verda.",
        "Drop-offs come in the front: drive your police car up to the gate and stop. My guards take them out of the back.",
        "They're booked in at intake - a photo, a number, a bunk. You're paid for each one: 120 kronoj, more as you rise.",
        "In there: the yard, the day room and twelve cells. Trouble-makers go to the SHU at the far end - solitary.",
        "Bring them in alive and the Chief hears about it. Now - let's see who you've got."}};
    // Where the tour's camera stands for each line (the prison's frame, height over the ground) and what it looks at.
    const auto tour_shot=[&](int line,Vector3& eye,Vector3& look) {
        const auto at=[&](float x,float z,float up){Vector3 p=prison_at(x,z);p.y+=up;return p;};
        switch(line) {
            case 0: eye=at(-75,-85,46);look=at(0,0,0);break;
            case 1: eye=at(-16,-58,7);look=at(0,-33,2.5F);break;
            case 2: eye=at(-2,-22,9);look=at(17,-27,1);break;
            case 3: eye=at(-34,-16,15);look=at(4,14,2.5F);break;
            default: eye=at(0,-70,12);look=at(0,-40,1);break;
        }
    };
    std::string career_path,career_saved;
    float career_save_wait=0;
    bool career_dead=false;
    struct JoinOffer { int crew{-1}; std::string patrol_car; double until{-1}; } join_offer;
    // The police questline: Chief Marko Forta at Verda City HQ (a body in NpcSystem under his own
    // id), the training camp's range outside the city, and the player's own car's lights.
    game::career::TrainingCamp police_camp;
    struct PoliceChief { Vector3 at{}; float yaw{0}; bool placed{false}; float health{100}; } chief;
    constexpr int chief_bot=910001;
    constexpr const char* chief_character="character_22_police";
    constexpr const char* police_uniform="character_17_police";   // the player's, on duty
    std::string chief_line;float chief_line_time=0;
    // Verda Transporto: the boss at the depot door (a body under his own id), the loads he gives out
    // (saved beside the career) and where they go.
    game::freight::Trucking trucking;
    game::freight::HaulWorld haul_world;
    std::string trucking_path,trucking_saved;
    // The player's record (game::player::PlayerStats): kept beside the career, shown in the bag.
    game::player::PlayerStats stats;
    std::string stats_path,stats_saved;
    std::int64_t stats_money=-1;
    Vector3 stats_feet{};bool stats_placed=false;
    struct DepotBoss { Vector3 at{}; float yaw{180}; bool placed{false}; } depot_boss;
    constexpr int depot_boss_bot=930001;
    constexpr const char* depot_boss_name=game::career::depot_boss;
    // A STRIKE at the depot's gate: the player within this of it (in the cab or on the picket
    // line), and the brothers who come out to picket with signs (hidden the rest of the time).
    constexpr float strike_picket_reach=40,strike_gate_reach=14;
    std::string strike_rig;   // the last big rig the player drove: the one that can block the gate
    constexpr int picket_bot=933000,picket_size=4;
    bool picket_out=false;
    std::array<Vector3,picket_size> picket_spots{};std::array<float,picket_size> picket_yaws{};bool picket_placed=false;
    // La Frateco de Kamionistoj, the outlaw truckers' brotherhood: its man at its home, La Ringo on
    // the ring road (Ivo Kovač, first), brothers there and at the depot's diesel lane, and a trucker
    // at a pub in every town (bodies under their own ids),
    // and the brothers who answer a member's CB call (game::career::Backup, set_truckers).
    struct Kamionisto { Vector3 at{}; float yaw{0}; std::string town; bool rep{false}; };
    std::vector<Kamionisto> kamionistoj;
    constexpr int kamionisto_bot=932000;
    constexpr const char* brotherhood_man="Ivo Kovač";
    game::career::Backup cb_backup;
    int trucker_talks=0;
    // A secure Verda's last word: La Verda Mano, the last of the gangs, lay down their guns at their
    // Verda City bar and become La Verda Mano Kultura Societo - the language, the songs, the flag.
    struct Society { bool open{false}; Vector3 at{}; float yaw{0}; } society;
    constexpr int society_bot=950000,society_size=4;
    int society_talks=0;
    // The campaign: this save slot's story (Uncle Ron's welcome home), Uncle Ron himself (a body
    // under his own id), his house (one of the Okcidenta homes, the player's once he hands it
    // over), his truck, and the autosave.
    game::campaign::CampaignState campaign;
    game::campaign::Story story;
    std::string campaign_path;
    // What Uncle Ron has to say next time the player talks to him (his reaction to the road they took).
    struct HouseTag { bool found{false}; Vector3 at{}, normal{}; };
    std::vector<HouseTag> house_tags;   // per Mafio crew: where its tag is sprayed
    // The player's tags on a rival gang's walls that nobody has seen yet: a patrol of that gang
    // (game::mafia::Patrol) that comes within sight finds it. The first one found makes the gang
    // hunt the player; after that, each one is war - a squad sent at the player's nearest crew.
    struct FreshTag { Vector3 at{}; int owner{-1}, tagger{-1}, turf{-1}; };
    std::vector<FreshTag> fresh_tags;
    std::array<int,game::mafia::gang_count> tags_found{};
    std::vector<game::campaign::Line> ron_pending;
    bool ron_pending_home=false;   // ...and then he drives the player home (the prison gate)
    struct UncleRon { Vector3 at{}; float yaw{0}; bool placed{false}; } uncle_ron;
    constexpr int uncle_ron_bot=940001;
    int ron_home=-1;                 // index into homes_for_sale
    // Uncle Dario, Ron's brother, heads La Verda Mano from its Verda City bar: the nephew who joins
    // gets a flat close by (a Main Street building near the bar; index into homes_for_sale).
    int dario_flat=-1;
    constexpr const char* dario_name="Uncle Dario";
    float autosave_wait=0,autosave_note=0,slots_refresh=0;
    constexpr float autosave_every=120;
    // The cargo terminal's quay, where the player comes off the ship and Uncle Ron waits by his truck.
    constexpr Vector3 arrival_spot{2230,0,-482},ron_at_port{2203,0,-462},ron_truck_at_port{2195,0,-454};
    std::string lights_car;          // the police car whose lights the player switched on ("" none)
    // Who runs Verda: every town and ranch, their holders and holds, the tags on the walls (saved
    // beside the career); the logos the tags are painted with; the underworld's news.
    game::mafia::Territory territory;
    game::mafia::GangLogos gang_logos;
    std::string territory_path,territory_saved;
    float territory_save_wait=0;
    std::string gang_news;float gang_news_time=0;
    std::string quiet_news;   // news the player isn't part of: not on the screen, only passed round by word of mouth
    bool island_ruled=false;
    // How it ends when the gangs win: the Green Hand's siege of the parliament, then the Russian
    // landing and the garrisons to clear (saved beside the territory).
    game::mafia::Endgame endgame;
    std::string endgame_path,endgame_saved;
    int boss_talks=0;                // the bosses vary what they say
    std::vector<game::career::DockSpot> smuggling_docks;
    // Marnafto's diesel lanes for the big rigs (found by model; game::freight::Diesel).
    std::vector<Vector3> diesel_lanes;
    float diesel_owed=0;std::int64_t diesel_paid=0;
    // La Verda Mano's colours: the gable mural on the Verda City hideout, and the banners over the
    // hideouts and town squares it holds (rebuilt when a town changes hands).
    struct Mural { Vector3 centre{}, normal{}; float width{0}; bool placed{false}; } green_hand_mural;
    std::string banner_key;
    int mural_attempts=0;float mural_wait=0;
    bool gt_stolen=false;
    // La Ŝosea Patrolo: its outposts on the ring road (how many are open grows with the law), the
    // art for the booths and the cruisers' doors, and which ranches have already called them.
    std::vector<game::services::Outpost> patrol_posts;
    int patrol_open=0;
    game::services::HighwayPatrolArt patrol_art;
    // Furniture in the pubs and some homes; Vestaĵejo Verda, the clothing store, and what you wear.
    std::optional<game::interiors::ClothingStore> clothing_store;
    std::optional<game::interiors::StashSite> stash;   // the holdall of a million kronoj (game::economy::Stash)
    game::wardrobe::Catalogue wardrobe_catalogue;
    game::wardrobe::StoreUI store_ui;
    // Whether anything is in reach of USE (the touch USE button shows only then).
    bool use_available=false;
    double entered_at=-10;   // when the player last got in at a driver's door (GetTime)
    int banner_turf=-1;float banner_time=0;   // the crew banner shows a few seconds on arriving
    float drive_hint=0;                       // the driving keys, shown for a while after getting in
    bool store_open=false;
    std::string store_status,store_prompt;
    // The motor trade (game::autos): Marnafto's pumps, the Meĥanikejo bays, the car lot's desk.
    game::autos::AutoMenu motor_menu;
    enum class MotorScreen{None,Pump,Workshop,Paint,Engine,Tires,Bumper,Dealer,Depot,Plating,Gun,Tanks,Exhaust,Spoiler,Tint,Rims,Glow};
    // The workshop's rows in order (then Repair).
    static constexpr std::array<MotorScreen,9> workshop_rows{MotorScreen::Paint,MotorScreen::Engine,MotorScreen::Tires,MotorScreen::Bumper,
        MotorScreen::Exhaust,MotorScreen::Spoiler,MotorScreen::Tint,MotorScreen::Rims,MotorScreen::Glow};
    MotorScreen motor_screen=MotorScreen::None;
    std::vector<world::motor::Site> motor_sites;
    // The churches (game::culture::Faith): a word about each the first time the player walks up.
    std::vector<game::culture::Church> churches;
    int church_seen=-1;
    // Word gets around (game::culture::Gossip): the island's news, and what it says about the player.
    game::culture::Gossip gossip;std::string gossip_heard;
    // Street racing on the ring at night (game::racing): Fulmo and the boys at the Marnafto station.
    game::racing::StreetRace street_race;
    constexpr int race_bot=986000;
    bool race_cars_out=false,race_bots_out=false;
    // Uncle Ron and Uncle Dario on the phone (the campaign, in free play).
    game::campaign::Phone phone;
    // Sundays and feasts: the bells, and the faithful outside the nearest church.
    constexpr int crowd_bot=987000;
    int crowd_church=-1,crowd_size=0,crowd_green=0;bool crowd_candles=false;
    std::vector<Vector3> crowd_spots;
    float bell_clock=0;int feast_said_day=-1;
    Sound bell_sound{},ring_sound{};bool chimes_ready=false;
    int motor_site=-1,motor_served=-1;   // the site in use; one whose menu was closed (not again till the car leaves it)
    std::string motor_car;               // the car being served
    std::vector<game::autos::OwnedCar> owned_cars;   // the cars the player bought (this session's ids)
    bool owned_ready=false;
    float owned_sync=0;
    characters::AnimationController mannequin_pose;
    mannequin_pose.advance(characters::AnimationAction::Idle,0);
    std::array<bool,game::mafia::family_count> ranch_alarm{};
    std::vector<int> retired_bots;   // crews broken up in an earlier session: not drawn            // an outsider at the wheel of the Verda Mano GT (once a drive)
    game::casino::Poker poker;
    game::casino::PokerTable poker_table;
    bool poker_open=false;
    int poker_buy_in=0; // the wallet changes only when the player leaves (by the net win or loss)
    // Explore: the Policio's patrol cars and Hospitalo de Verda's ambulances.
    game::services::EmergencyServices services;
    bool respawn_at_hospital=false; // the player died in Explore: the ambulance took them in
    Vector3 death_at{};             // ...from here, to the nearest clinic or the hospital
    // Explore: the Stela Turo penthouse, for sale; once bought, home (sleep there, wake there).
    game::economy::Penthouse penthouse;
    bool was_in_penthouse=false;
    // Parkumejo Centra, the multi-storey car park downtown: cars drive on its decks and ramps, and the
    // mansion on its roof is for sale (game::economy::SkyMansion), with its own parking bay.
    world::garage::Site garage_site;
    bool garage_here=false,was_in_mansion=false,was_in_bay=false;
    // Homes for sale round the island (besides the penthouse): vacant while listed this session.
    std::vector<game::economy::Listing> homes_for_sale;
    std::vector<game::economy::Vacated> vacated_homes;
    const game::economy::Listing* home_here=nullptr;   // the listing the player stands in
    const game::economy::Listing* was_in_home=nullptr;
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
    characters::AnimationController car_seat_pose;            // everyone else at a wheel
    // Outsiders: Paul Acosta, the Vakeroj, and an officer's backup (against a gang at war with the officer).
    mafio.set_outsiders([&](int a,int b,double now){
        if(endgame.ours(a) || endgame.ours(b)) return endgame.hostile(a,b);   // the parliament's guard, the occupiers
        const auto officer=[&](int id){return backup.officer(id) || cb_backup.officer(id) || (player_crew.partner()>=0 && id==player_crew.partner());};
        if(officer(a) || officer(b)) {
            const int gang=mafio.gang_of(officer(a) ? b : a);
            return gang>=0 && mafio.provoked(gang,now);
        }
        return walking_man.hostile_to(a,b,now) || ranches.hostile_to(a,b,now);});
    bot_drivers.reserve(std::string(game::life::WalkingMan::car_marker));
    bot_drivers.reserve("vehicle_spawn_sxs_");
    bot_drivers.reserve("vehicle_spawn_semi_");
    // A cattleman hat on a head (the crown from the body's pose): its band ~10 cm below the top.
    const auto draw_cowboy_hat=[&](const characters::HeldGun& head,const char* model="assets/verda/ranch/cowboy_hat.glb") {
        Model* hat=get_world_asset_model(model,character_registry);
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
    // The Chief's peaked cap: navy crown, gold band and badge, black peak (built from parts).
    const auto draw_chief_cap=[&](const characters::HeldGun& head) {
        if(!head.hat) return;
        const Vector3 up=Vector3Normalize(head.crown_up);
        Vector3 forward=Vector3Subtract(head.crown_forward,Vector3Scale(up,Vector3DotProduct(head.crown_forward,up)));
        if(Vector3Length(forward)<1e-3F) return;
        forward=Vector3Normalize(forward);
        const Vector3 right=Vector3CrossProduct(up,forward);
        const Vector3 at=Vector3Subtract(Vector3Subtract(head.crown,Vector3Scale(up,.075F)),Vector3Scale(forward,.02F));
        Matrix m=MatrixIdentity();
        m.m0=right.x;m.m1=right.y;m.m2=right.z;m.m4=up.x;m.m5=up.y;m.m6=up.z;m.m8=forward.x;m.m9=forward.y;m.m10=forward.z;
        m.m12=at.x;m.m13=at.y;m.m14=at.z;
        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(m));
        const Color navy{22,36,84,255},gold{212,170,60,255},black{14,14,16,255};
        DrawCylinder({0,0,0},.112F,.112F,.065F,14,navy);                 // the band
        DrawCylinder({0,.018F,0},.1135F,.1135F,.022F,14,gold);            // gold braid
        DrawCylinder({0,.065F,.008F},.112F,.142F,.035F,14,navy);          // the crown, flaring out
        DrawCylinder({0,.1F,.008F},.142F,.142F,.008F,14,navy);
        rlPushMatrix();rlTranslatef(0,.006F,.1F);rlRotatef(-14,1,0,0);
        DrawCube({0,0,0},.2F,.012F,.085F,black);                         // the peak
        rlPopMatrix();
        DrawCube({0,.065F,.135F},.04F,.045F,.012F,gold);                 // the badge
        rlPopMatrix();
    };
    // The occupiers' steel helmet: an olive shell over the crown with a rim (built from parts).
    const auto draw_helmet=[&](const characters::HeldGun& head) {
        if(!head.hat) return;
        const Vector3 up=Vector3Normalize(head.crown_up);
        Vector3 forward=Vector3Subtract(head.crown_forward,Vector3Scale(up,Vector3DotProduct(head.crown_forward,up)));
        if(Vector3Length(forward)<1e-3F) return;
        forward=Vector3Normalize(forward);
        const Vector3 right=Vector3CrossProduct(up,forward);
        const Vector3 at=Vector3Subtract(Vector3Subtract(head.crown,Vector3Scale(up,.085F)),Vector3Scale(forward,.015F));
        Matrix m=MatrixIdentity();
        m.m0=right.x;m.m1=right.y;m.m2=right.z;m.m4=up.x;m.m5=up.y;m.m6=up.z;m.m8=forward.x;m.m9=forward.y;m.m10=forward.z;
        m.m12=at.x;m.m13=at.y;m.m14=at.z;
        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(m));
        const Color olive{78,86,52,255},dark{52,58,36,255};
        DrawCylinder({0,0,0},.128F,.132F,.012F,16,dark);                // the rim
        DrawCylinder({0,.012F,0},.122F,.118F,.045F,16,olive);           // the sides
        DrawCylinder({0,.057F,0},.118F,.085F,.035F,16,olive);           // shoulder of the dome
        DrawCylinder({0,.092F,0},.085F,.02F,.03F,16,olive);             // the crown
        rlPopMatrix();
    };
    const auto krimulo_hostile=[&](int bot){return !mafio.active() || mafio.provoked(mafio.gang_of(bot),match_time) ||
        walking_man.hostile_to(bot,game::ai::BattleRoyaleBots::player_id,match_time) ||
        ranches.hostile_to(bot,game::ai::BattleRoyaleBots::player_id,match_time) ||
        endgame.hostile(bot,game::ai::BattleRoyaleBots::player_id);};

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
    // The outbreak (Zombie Survival's story, game::outbreak): its cast and scenes, and its save.
    game::outbreak::Run outbreak;
    std::string outbreak_path;
    float outbreak_autosave=0;
    bool outbreak_respawn=false;   // died in the outbreak: back at the hideout (or the story again)
    std::uint64_t zombie_seed=0x2b7e1516U;
    // Esperanto signage and the flag. Rebuilt when a mode starts and after Creator edits.
    game::culture::SignRenderer sign_renderer;
    sign_renderer.load(GetApplicationDirectory());
    // La Verda Mano's clothes (tools/generate_gang_outfits.py), worn on Character_05's body: the
    // foot soldiers' white shirt with the green hand, the officers' charcoal suit and green tie.
    // Every other gang in its own colours (gang_<n>_soldato/kapo): easy to pick out on the street.
    struct Outfits {
        Texture2D soldato{}, kapo{}, occupier{};
        std::array<Texture2D,5> gang_soldato{},gang_kapo{};
        std::array<Texture2D,5> team{};   // La Verda Basballigo's jerseys (players and fans alike)
        ~Outfits() {
            if(soldato.id) UnloadTexture(soldato);
            if(occupier.id) UnloadTexture(occupier);
            for(auto& t:team) if(t.id) UnloadTexture(t);
            if(kapo.id) UnloadTexture(kapo);
            for(auto* set:{&gang_soldato,&gang_kapo}) for(auto& t:*set) if(t.id) UnloadTexture(t);
        }
    } outfits;
    constexpr const char* outfit_body="character_05";
    {
        const auto load_outfit=[](const char* path) {
            const std::string packaged=std::string(GetApplicationDirectory())+path;
            Texture2D t=LoadTexture(FileExists(packaged.c_str()) ? packaged.c_str() : path);
            if(t.id) {GenTextureMipmaps(&t);SetTextureFilter(t,TEXTURE_FILTER_TRILINEAR);}
            return t;
        };
        outfits.soldato=load_outfit("assets/verda/characters/outfits/verda_mano_soldato.png");
        outfits.kapo=load_outfit("assets/verda/characters/outfits/verda_mano_kapo.png");
        outfits.occupier=load_outfit("assets/verda/characters/outfits/rusa_soldato.png");   // the occupiers' camouflage
        for(int k=0;k<5;++k) outfits.team[static_cast<std::size_t>(k)]=load_outfit(TextFormat("assets/verda/characters/outfits/team_%d_jersey.png",k));
        for(int g=1;g<5;++g) {
            outfits.gang_soldato[static_cast<std::size_t>(g)]=load_outfit(TextFormat("assets/verda/characters/outfits/gang_%d_soldato.png",g));
            outfits.gang_kapo[static_cast<std::size_t>(g)]=load_outfit(TextFormat("assets/verda/characters/outfits/gang_%d_kapo.png",g));
        }
    }
    // A gang member's clothes (by gang and rank).
    const auto gang_outfit=[&](int gang,bool officer)->unsigned int {
        if(gang==0) return officer ? outfits.kapo.id : outfits.soldato.id;
        if(gang<1 || gang>4) return 0U;
        return officer ? outfits.gang_kapo[static_cast<std::size_t>(gang)].id : outfits.gang_soldato[static_cast<std::size_t>(gang)].id;
    };
    const auto outfit_texture=[&](const std::string& outfit)->unsigned int {
        if(outfit=="verda_mano_soldato") return outfits.soldato.id;
        if(outfit=="verda_mano_kapo") return outfits.kapo.id;
        return 0U;
    };
    std::size_t signed_world=static_cast<std::size_t>(-1);
    const auto world_size=[&]{
        std::size_t n=0;
        for(const auto& settlement:verda_region.settlements()) n+=settlement.assets.size()+settlement.buildings.size()+settlement.gameplay_markers.size();
        return n;
    };
    const auto rebuild_signs=[&]{
        world::physics::MeshCollisionLibrary::preload(verda_region);
        auto signage=game::culture::build_signage(verda_region,island_life.island().places.empty() ?
            game::life::build_island(verda_region,&character_registry) : island_life.island());
        // Vestaĵejo Verda's board over its windows.
        if(clothing_store) {
            std::erase_if(signage.signs,[&](const game::culture::Sign& sign){return sign.place==clothing_store->place_id;});
            game::culture::Sign sign;
            sign.kind=game::culture::SignKind::Shopfront;
            sign.place=clothing_store->place_id;
            sign.position=clothing_store->sign;
            sign.yaw=clothing_store->yaw+180;   // the face looks out of the shop front
            sign.max_width=clothing_store->width*.7F;
            sign.front={{"VESTAĴEJO VERDA",.36F},{"VESTOJ · ĈAPELOJ · KOSTUMOJ",.17F}};
            sign.board=Color{0,96,52,255};sign.ink=Color{255,244,214,255};
            signage.signs.push_back(std::move(sign));
        }
        // The highway gantries at downtown's ends: a green board over each carriageway, facing the
        // traffic under it (traffic keeps right: southbound at -x, northbound at +x).
        for(const auto& town:verda_region.settlements())
            for(const auto& a:town.assets) {
                if(!a.model_path.ends_with("highway_gantry.glb")) continue;
                const bool north=a.position.z<0;
                const auto board=[&](float x,float yaw,const char* top,const char* bottom) {
                    game::culture::Sign sign;
                    sign.kind=game::culture::SignKind::Distance;
                    sign.place=a.id;
                    sign.position={x,a.position.y+7.75F,a.position.z+(yaw>90 ? -.62F : .62F)};
                    sign.yaw=yaw;
                    sign.max_width=7.2F;
                    sign.front={{top,.62F},{bottom,.42F}};
                    sign.board=Color{0,106,62,255};sign.ink=WHITE;
                    signage.signs.push_back(std::move(sign));
                };
                if(north) {
                    board(-5.4F,180,"BONVENON AL VERDA","CENTRO · PARLAMENTO");
                    board(5.4F,0,"ESPERA  1,5 km","NORDO ·  RINGVOJO");
                } else {
                    board(5.4F,0,"BONVENON AL VERDA","CENTRO · STELA TURO");
                    board(-5.4F,180,"SUDA HAVENO  1,6 km","SUDO ·  RINGVOJO");
                }
            }
        // VENDATA boards in front of the homes for sale.
        for(const auto& home:homes_for_sale) {
            if(game::economy::owns(loot_session.economy(),home) || home.price<=0) continue;   // (Uncle Ron's is not for sale)
            game::culture::Sign sign;
            sign.kind=game::culture::SignKind::Shopfront;
            sign.place=home.id;
            const float ground=home.house ? world::terrain::TerrainHeight::sample(home.board.x,home.board.z) : home.centre.y;
            sign.position={home.board.x,ground+1.75F,home.board.z};
            sign.yaw=home.yaw+180;   // facing the street
            sign.max_width=1.6F;
            sign.front={{"VENDATA",.26F},{TextFormat("%lld kr",static_cast<long long>(home.price)),.2F}};
            sign.board=Color{176,32,36,255};sign.ink=WHITE;
            signage.signs.push_back(std::move(sign));   // its post is drawn with the world (below)
        }
        sign_renderer.set(std::move(signage));
        signed_world=world_size();
    };
    wardrobe_catalogue.build(character_registry);
    // The pubs' and the lived-in homes' furniture, and Vestaĵejo Verda, as ordinary world assets
    // ("furnish_*"; they collide like any prop). Rebuilt with island life each session.
    const auto furnish_island=[&]{
        for(auto& town:verda_region.runtime_settlements())
            std::erase_if(town.assets,[](const world::WorldAsset& a){return a.id.starts_with("furnish_");});
        clothing_store.reset();
        const auto& places=island_life.places();
        auto& towns=verda_region.runtime_settlements();
        int capital=-1;
        for(std::size_t t=0;t<towns.size();++t) if(towns[t].id=="capital_verda") capital=static_cast<int>(t);
        const auto* site=capital>=0 ? game::interiors::store_site(places,capital,towns[static_cast<std::size_t>(capital)].center) : nullptr;
        // Every home somebody lives in is furnished; homes for sale stand empty.
        std::vector<bool> lived_in(places.size(),false);
        for(const auto& r:island_life.residents()) if(r.home>=0 && r.home<static_cast<int>(places.size())) lived_in[static_cast<std::size_t>(r.home)]=true;
        auto pieces=game::interiors::furnish(places,[](float x,float z){return world::terrain::TerrainHeight::sample(x,z);},site ? site->id : std::string(),&lived_in);
        // The hidden stash, in an empty house, until the player has taken it.
        {
            std::vector<std::string> listed;
            for(const auto& home:homes_for_sale) listed.push_back(home.building);
            stash=game::interiors::stash_site(places,lived_in,[](float x,float z){return world::terrain::TerrainHeight::sample(x,z);},listed);
            if(stash) {
                TraceLog(LOG_INFO,"OUTLAND stash in %s at %.1f %.1f",stash->place_id.c_str(),stash->at.x,stash->at.z);
                if(!game::economy::Stash::taken(loot_session.economy())) pieces.push_back(stash->bag);
            }
        }
        if(site) {
            clothing_store=game::interiors::clothing_store(*site,std::max(0,static_cast<int>(wardrobe_catalogue.looks().size())-1));
            pieces.insert(pieces.end(),clothing_store->pieces.begin(),clothing_store->pieces.end());
        }
        // The homes the player owns come furnished.
        for(const auto& home:homes_for_sale) if(game::economy::owns(loot_session.economy(),home)) {
            const auto furniture=game::interiors::furnish_home(game::economy::place_of(home),home.centre.y);
            pieces.insert(pieces.end(),furniture.begin(),furniture.end());
        }
        int pubs=0,homes=0;
        for(std::size_t k=0;k<places.size();++k) {
            const auto& place=places[k];
            if(site && place.id==site->id) continue;
            if(place.kind==game::life::PlaceKind::Pub && game::interiors::shell_of(place)!=game::interiors::Shell::None) ++pubs;
            else if(game::interiors::furnished_home(place) && lived_in[k]) {
                if(++homes<=2 || game::interiors::shell_of(place)==game::interiors::Shell::House)
                    TraceLog(LOG_INFO,"OUTLAND furnished home %s at %.0f %.0f",place.id.c_str(),place.interior.x,place.interior.z);
            }
        }
        std::size_t n=0;
        for(const auto& piece:pieces) {
            // Into the nearest town's assets (they draw and collide with it).
            std::size_t near=0;
            for(std::size_t t=1;t<towns.size();++t)
                if(Vector3Distance(towns[t].center,piece.at)<Vector3Distance(towns[near].center,piece.at)) near=t;
            world::WorldAsset asset;
            asset.id="furnish_"+std::to_string(++n);
            asset.type=world::AssetType::Sign;
            asset.model_path=piece.model;
            asset.position=piece.at;
            asset.rotation_y=piece.yaw;
            asset.size=game::interiors::footprint(piece.model);
            asset.collision=piece.solid;
            if(!towns.empty()) towns[near].assets.push_back(std::move(asset));
        }
        world::physics::MeshCollisionLibrary::preload(verda_region);
        world::physics::WorldCollision::invalidate_index();
        static_world.invalidate();
        TraceLog(LOG_INFO,"OUTLAND furnished: %d pubs and %d homes, %d pieces%s",pubs,homes,static_cast<int>(n),
                 clothing_store ? TextFormat("; Vestaĵejo Verda in %s, door %.1f %.1f facing %.0f, %d mannequins",clothing_store->place_id.c_str(),
                     clothing_store->front.x,clothing_store->front.z,clothing_store->yaw+180,static_cast<int>(clothing_store->mannequins.size())) : "");
    };
    game::vehicles::VehicleRegistry vehicle_registry;
    std::string vehicle_error;
    if(!vehicle_registry.load((std::string(character_root)+"/assets/verda/vehicles/vehicle_manifest.tsv"),vehicle_error))
        TraceLog(LOG_ERROR,"Vehicle catalog: %s",vehicle_error.c_str());
    game::vehicles::VehicleSystem vehicles(vehicle_registry);
    game::vehicles::VehicleRenderer vehicle_renderer;
    // Over the parking garage a car rides its decks and ramps: the highest floor of it within a
    // metre above the wheels (WorldCollision::ground_height); everywhere else, the terrain.
    garage_here=world::garage::find(verda_region,garage_site);
    vehicles.bind_surface([&](Vector3 p,float from) {
        if(!garage_here || !garage_site.over(p,1.5F)) return std::numeric_limits<float>::quiet_NaN();
        return world::physics::WorldCollision::ground_height({p.x,from,p.z},from,verda_region,1.0F);
    });
    // Off the roads and paved ground (the towns' and yards' level pads count as paved): rough.
    {
        auto offroad_graph=std::make_shared<std::unique_ptr<world::roads::RoadGraph>>();
        auto offroad_roads=std::make_shared<std::size_t>(0);
        vehicles.bind_offroad([&,offroad_graph,offroad_roads](Vector3 p) {
            if(game_mode!=game::GameMode::Explore) return 0.0F;
            std::size_t roads=0;
            for(const auto& town:verda_region.settlements()) roads+=town.roads.size();
            if(!*offroad_graph || roads!=*offroad_roads) {*offroad_graph=std::make_unique<world::roads::RoadGraph>(verda_region);*offroad_roads=roads;}
            const auto spot=(*offroad_graph)->nearest(p);
            const float verge=spot.edge>=0 ? spot.distance-spot.width*.5F : 1e9F;
            const float road=std::clamp((verge-1.5F)/6.0F,0.0F,1.0F);   // easing off the verge
            const float paved=std::clamp((world::terrain::TerrainHeight::pad_weight(p.x,p.z)-.5F)*2.0F,0.0F,1.0F);
            return road*(1-paved);
        });
    }
    vehicles.reconcile(verda_region);
    const std::string vehicle_state_path=map_path+".vehicles";
    bool vehicle_save_protected=false,vehicle_dirty=false;
    const auto save_outbreak=[&] {
        if(outbreak_path.empty() || !outbreak.active()) return;
        if(const auto* car=vehicles.driver();car && outbreak.has_hideout()) outbreak.state().position=car->position;
        const auto text=game::outbreak::serialise(outbreak.state());
        if(!SaveFileText(outbreak_path.c_str(),const_cast<char*>(text.c_str()))) TraceLog(LOG_WARNING,"OUTLAND outbreak: could not save %s",outbreak_path.c_str());
    };
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
    // The car radio: three live stations; the clock starts at the time of day, so each session
    // tunes in part-way through a song.
    game::radio::CarRadio car_radio;
    car_radio.set_station(settings.radio_station);
    double radio_clock=static_cast<double>(std::time(nullptr)%86400);
    const auto apply_settings=[&]() {
        input_system.set_look({settings.look_sensitivity,settings.mouse_sensitivity,settings.invert_y,settings.aim_toggle});
        input_system.set_bindings(settings.bindings);
        input_system.set_button_size(settings.button_size);
        input_system.layout()=input::customised(settings.touch_layout);   // the player's own button layout
        haptics.set_enabled(settings.vibration);
        environment_audio.set_footsteps(settings.footsteps);
        environment_audio.set_volume(settings.volume);
        if(environment_audio.muted()!=settings.muted) environment_audio.toggle_mute();
        render_scale.set_mode(static_cast<int>(settings.graphics));
    };
    const auto store_settings=[&]() {
        settings.volume=environment_audio.volume();settings.muted=environment_audio.muted();
        settings.radio_station=car_radio.station();
        if(!settings.save(settings_path)) TraceLog(LOG_WARNING,"Settings could not be saved: %s",settings_path.c_str());
        platform::flush_saves();
    };
    apply_settings();
    // Entering a zone shows its name for a few seconds.
    int shown_zone=-1;
    int shown_turf=-2;float turf_banner=0;   // the turf line under the compass, a few seconds on arriving
    float zone_banner=0;

    game::combat::WeaponSystem weapons;
    // A rig's roof gun (the depot's fit-out): a machine gun of its own, fed from the rig.
    game::combat::WeaponSystem roof_gun;
    bool roof_gun_ready=false,roof_gun_on=false;
    // The Policio's desk and armory, and the gangs' gun men (game::career::Armory): one list on screen.
    enum class GearScreen{None,Desk,Armory,GunMan,Talk,Confirm};
    // Talking to a gang's boss, Ivo or a station's front desk opens a short menu: what you can do
    // there, and leaving (a gang, the brotherhood, the force) only ever behind a second "yes".
    enum class TalkWith{None,OwnBoss,GangBoss,Ivo,IvoJoin,Station};
    enum class Pending{None,LeaveGang,LeaveBrotherhood,HandInBadge};
    TalkWith talk_with=TalkWith::None;
    Pending pending=Pending::None;
    int talk_crew=-1;                 // the boss's crew
    std::string talk_car;             // the station's patrol car (joining the Policio)
    bool talk_open=false;             // asked for this frame
    GearScreen gear_screen=GearScreen::None;
    game::autos::AutoMenu gear_menu;
    int gear_gang=-1;                 // whose gun man
    bool gear_open_desk=false,gear_open_gun_man=false;   // asked for this frame
    struct GunMan { Vector3 at{}; float yaw{0}; int gang{-1}; };
    std::vector<GunMan> gun_men;
    constexpr int gun_man_bot=960000;
    std::unordered_set<std::string> shotgun_cars;   // police cars whose shotgun the officer already took
    std::vector<char> drive_by_alive;
    // Cars and pedestrians: who was struck lately (by key), the player's hit pending or run from,
    // and the last news of somebody else's hit and run.
    std::unordered_map<int,double> struck_at;
    game::law::HitAndRun hit_and_run;
    double npc_hit_news=-1e9;
    double next_hijack=-1;            // when a gang next tries for the player's loaded rig   // the Mafio's members standing last frame (a drive-by's crew kills count)
    game::combat::CombatWorld combat_world(verda_region);
    game::Health player_health{game::player_max_health};
    // LAN sides (net::Sides, picked in the lobby): who is on the player's side, how many of each
    // still stand, and - out of it with the side fighting on - whose shoulder the camera follows.
    net::Sides lan_sides=net::Sides::Free;
    const auto teammate=[&](std::uint8_t slot){return lan_match && lan_sides!=net::Sides::Free && net::same_side(lan_sides,lan.slot(),slot);};
    const auto teammates_alive=[&]{int n=0;for(const auto& r:lan_play.remotes()) n+=r.known && (r.state.flags&net::StateAlive) && teammate(r.slot);return n;};
    const auto side_alive=[&]{return teammates_alive()+(player_health.alive() ? 1 : 0);};
    const auto enemies_alive=[&]{int n=0;for(const auto& r:lan_play.remotes()) n+=r.known && (r.state.flags&net::StateAlive) && !teammate(r.slot);return n;};
    int spectate_slot=-1;
    // Squad revive: knocked down while a teammate stands; a teammate picks you up (game::lan::SquadRevive).
    game::lan::SquadRevive squad_revive;
    bool was_downed=false;
    const auto squad_play=[&]{return lan_match && lan_sides!=net::Sides::Free && game_mode==game::GameMode::BattleRoyale;};
    const auto down=[](const game::lan::Remote& r){return (r.state.status&net::StatusDowned)!=0;};
    const auto teammates_standing=[&]{int n=0;for(const auto& r:lan_play.remotes()) n+=r.known && (r.state.flags&net::StateAlive) && !down(r) && teammate(r.slot);return n;};
    // ---- The campaign's story: where Uncle Ron, his truck and the player stand, and saving it.
    const auto floor_at=[&](Vector3 at) {
        const float top=std::max(at.y,world::terrain::TerrainHeight::sample(at.x,at.z))+1.5F;
        const float floor=world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region);
        return Vector3{at.x,std::isfinite(floor) ? floor : world::terrain::TerrainHeight::sample(at.x,at.z),at.z};
    };
    const auto facing=[](Vector3 from,Vector3 to){return std::atan2(to.x-from.x,to.z-from.z)*RAD2DEG;};
    // At Uncle Ron's: him on the front path, the player on the lawn facing him, the truck on the drive.
    struct HouseSpots { Vector3 ron{},nephew{},truck{};float ron_yaw{0},nephew_yaw{0},truck_yaw{0}; };
    const auto ron_house=[&](HouseSpots& out) {
        if(ron_home<0 || ron_home>=static_cast<int>(homes_for_sale.size())) return false;
        const auto& home=homes_for_sale[static_cast<std::size_t>(ron_home)];
        const float a=home.yaw*DEG2RAD,front=-home.size.z*.5F;
        const auto local=[&](float x,float z){return Vector3{home.centre.x+x*std::cos(a)+z*std::sin(a),home.centre.y,home.centre.z-x*std::sin(a)+z*std::cos(a)};};
        out.ron=floor_at(local(0,front-2.2F));
        out.nephew=floor_at(local(1.4F,front-5.2F));
        out.ron_yaw=facing(out.ron,out.nephew);out.nephew_yaw=facing(out.nephew,out.ron);
        out.truck_yaw=home.yaw+180;
        for(const float side:{1.0F,-1.0F}) {
            out.truck=local(side*(home.size.x*.5F+2.6F),front+2.2F);
            bool clear=!world::physics::WorldCollision::body_blocked(floor_at(out.truck),floor_at(out.truck).y,verda_region,1.0F);
            for(const auto& v:vehicles.vehicles()) if(v.id!=campaign.ron_truck && Vector3Distance(v.position,out.truck)<4) clear=false;
            if(clear) break;
        }
        return true;
    };
    const auto place_ron=[&](Vector3 at,float yaw) {
        uncle_ron={at,yaw,true};
        if(npcs.find_bot(uncle_ron_bot)>=0) {npcs.set_bot(uncle_ron_bot,at,yaw,100,characters::NpcState::Idle);return;}
        // A working man in everyday clothes.
        std::string body="character_05";
        for(std::uint64_t k=0;k<40;++k)
            if(const auto* d=character_registry.choose(characters::CharacterPool::Civilian,9100+k*13);
               d && d->id.find("female")==std::string::npos && d->id.find("police")==std::string::npos && d->id.find("doctor")==std::string::npos) {body=d->id;break;}
        npcs.spawn_bot(uncle_ron_bot,body,at,yaw);
    };
    const auto park_ron_truck=[&](Vector3 at,float yaw) {
        if(!campaign.ron_truck.empty() && vehicles.find(campaign.ron_truck)>=0) {
            if(vehicles.driver() && vehicles.driver()->id==campaign.ron_truck) return;
            vehicles.place(verda_region,campaign.ron_truck,at,yaw);vehicle_dirty=true;return;
        }
        std::vector<std::string> before;for(const auto& v:vehicles.vehicles()) before.push_back(v.id);
        if(vehicles.spawn(verda_region,at,yaw,"truck"))
            for(const auto& v:vehicles.vehicles()) if(std::find(before.begin(),before.end(),v.id)==before.end()) {campaign.ron_truck=v.id;vehicle_dirty=true;break;}
    };
    const auto own_ron_house=[&] {
        if(campaign.kicked_out) return;   // he showed the player the door
        if(ron_home<0 || ron_home>=static_cast<int>(homes_for_sale.size())) return;
        auto& owned=loot_session.economy().properties;
        const auto& id=homes_for_sale[static_cast<std::size_t>(ron_home)].id;
        if(std::find(owned.begin(),owned.end(),id)!=owned.end()) return;
        owned.push_back(id);loot_session.changed();
        furnish_island();rebuild_signs();   // the furniture moves in
    };
    const auto own_dario_flat=[&] {
        if(dario_flat<0 || dario_flat>=static_cast<int>(homes_for_sale.size())) return false;
        auto& owned=loot_session.economy().properties;
        const auto& id=homes_for_sale[static_cast<std::size_t>(dario_flat)].id;
        if(std::find(owned.begin(),owned.end(),id)!=owned.end()) return false;
        owned.push_back(id);loot_session.changed();
        furnish_island();rebuild_signs();
        return true;
    };
    const auto ron_home_now=[&](bool move_player) {
        HouseSpots spots;
        if(!ron_house(spots)) return;
        place_ron(spots.ron,spots.ron_yaw);
        park_ron_truck(spots.truck,spots.truck_yaw);
        if(move_player) {player.position=Vector3Add(spots.nephew,{0,1,0});player.yaw=spots.nephew_yaw*DEG2RAD;player.vertical_velocity=0;}
    };
    const auto place_story=[&](bool explicit_spawn,bool respawned) {
        using game::campaign::Stage;
        if(story.stage()==Stage::Arrival) {
            // Off the ship onto the quay at Porto Luma; Uncle Ron waiting by his truck.
            const Vector3 nephew=floor_at({arrival_spot.x,world::layout::cargo_quay_height,arrival_spot.z});
            const Vector3 ron=floor_at({ron_at_port.x,world::layout::cargo_quay_height,ron_at_port.z});
            place_ron(ron,facing(ron,nephew));
            park_ron_truck(floor_at({ron_truck_at_port.x,world::layout::cargo_quay_height,ron_truck_at_port.z}),90);
            if(!explicit_spawn) {player.position=Vector3Add(nephew,{0,1,0});player.yaw=facing(nephew,ron)*DEG2RAD;}
            interaction_message="Verda. Home again, after all those years in Eastern Europe";interaction_remaining=6;
            return;
        }
        if(story.stage()==Stage::Free) own_ron_house();
        HouseSpots spots;
        if(!ron_house(spots)) return;
        place_ron(spots.ron,spots.ron_yaw);
        if(story.stage()==Stage::HomeTalk) park_ron_truck(spots.truck,spots.truck_yaw);
        else if(campaign.ron_truck.empty() || vehicles.find(campaign.ron_truck)<0) park_ron_truck(spots.truck,spots.truck_yaw);
        if(explicit_spawn || respawned) return;
        if(story.stage()==Stage::Free && campaign.has_position) {
            player.position=Vector3Add(floor_at(campaign.position),{0,1,0});player.yaw=campaign.yaw*DEG2RAD;
        } else {
            player.position=Vector3Add(spots.nephew,{0,1,0});player.yaw=spots.nephew_yaw*DEG2RAD;
        }
    };
    // This slot's place in the story and where the player stands (the inventory and the rest with
    // `everything`; the career, territory and trucking save themselves as they change).
    const auto save_campaign=[&](bool everything) {
        if(campaign_path.empty()) return true;
        campaign.stage=story.stage();
        if(story.stage()==game::campaign::Stage::Free && player_health.alive()) {
            const auto* car=vehicles.driver();
            campaign.has_position=true;
            campaign.position=car ? car->position : Vector3{player.position.x,player.position.y-1,player.position.z};
            campaign.yaw=player.yaw*RAD2DEG;
        }
        campaign.saved_at=static_cast<std::int64_t>(std::time(nullptr));
        float best=1e30F;
        for(const auto& town:verda_region.settlements()) {
            if(town.id.starts_with("ranch_") || town.id.starts_with("hamlet_") || town.name.empty()) continue;
            const float d=std::hypot(town.center.x-campaign.position.x,town.center.z-campaign.position.z);
            if(d<best) {best=d;campaign.place=town.id=="capital_verda" ? std::string("Verda City") : town.name;}
        }
        if(!campaign.has_position) campaign.place=story.stage()==game::campaign::Stage::Arrival ? "Porto Luma" : "Verda City";
        const auto text=game::campaign::serialise(campaign);
        std::error_code made;
        std::filesystem::create_directories(std::filesystem::path(campaign_path).parent_path(),made);
        bool ok=SaveFileText(campaign_path.c_str(),const_cast<char*>(text.c_str()));
        if(everything) ok=(loot_session.save(weapons) || !loot_session.can_persist()) && ok;
        return ok;
    };
    // Armour from the bag: the best vest carried soaks part of every bullet, blast and bite.
    const auto armour_level=[&]{
        if(game_mode==game::GameMode::DevLab) return 0;
        const auto& bag=loot_session.inventory();
        if(game_mode==game::GameMode::Explore && career.on_duty()) return 2;   // an officer on duty wears body armour
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
    game::radio::RadioAudio radio_audio;
    vehicles.bind_occupant_damage([&](float amount){player_health.damage(amount);});
    // Verda Transporto's shed takes a rig without its trailer: drop it (J) outside first.
    bool depot_barred=false;
    vehicles.bind_barrier([&](std::string_view id,Vector3 from,Vector3 to,float yaw) {
        const auto* driven=vehicles.driver();
        if(!driven || driven->id!=id || !freight.hitched(driven->id)) return false;
        const auto* a=vehicles.asset(verda_region,id);
        const auto* def=a ? vehicles.definition(*a) : nullptr;
        if(!def) return false;
        const float r=yaw*DEG2RAD,half=def->length*.5F;
        const Vector3 ahead{std::sin(r)*half,0,std::cos(r)*half};
        for(const auto& site:motor_sites) {
            if(site.kind!=world::motor::Kind::Depot || std::hypot(site.origin.x-to.x,site.origin.z-to.z)>60) continue;
            for(const float end:{1.0F,-1.0F}) {
                const Vector3 was=Vector3Add(from,Vector3Scale(ahead,end)),now=Vector3Add(to,Vector3Scale(ahead,end));
                if(world::motor::in_depot(site,now) && !world::motor::in_depot(site,was)) {depot_barred=true;return true;}
            }
        }
        return false;
    });
    // The tank: about 25 km of driving in Explore (a bigger engine drinks more); Marnafto fills it.
    vehicles.bind_fuel([&](const std::string& id,float metres) {
        if(game_mode!=game::GameMode::Explore) return 0.0F;
        const auto* a=vehicles.asset(verda_region,id);
        // A big rig's tank goes four times as far (game::freight::Diesel).
        // Bigger tanks go further; a bigger diesel drinks a little more.
        if(a && a->model_path.find("semi_body")!=std::string::npos)
            return metres/(game::freight::Diesel::range_km*1000*a->vehicle.range)*(1+(std::max(1.0F,a->vehicle.power)-1)*.25F);
        return metres/25000.0F*(a ? std::max(1.0F,a->vehicle.power) : 1.0F);
    });
    const auto gang_name=[](int faction) -> std::string {
        return faction>=0 && faction<game::mafia::gang_count ? std::string(game::mafia::gangs[static_cast<std::size_t>(faction)].name) : std::string("?");
    };
    const auto faction_name=[&](int faction) -> std::string {
        if(faction<game::mafia::gang_count) return gang_name(faction);
        return "the "+territory.families()[static_cast<std::size_t>(faction-game::mafia::gang_count)].name+" family";
    };
    const auto faction_color=[&](int faction) -> Color {
        if(faction>=0 && faction<game::mafia::gang_count) return game::mafia::gangs[static_cast<std::size_t>(faction)].color;
        return Color{176,128,74,255};   // the ranching families: saddle brown
    };
    const auto player_gang=[&]{return career.state().side==game::career::Side::Gang ? career.state().gang : -1;};
    // The island's news reaches the screen only when it is the player's business (their gang, the
    // badge, the brotherhood); otherwise it only goes round as gossip.
    const auto tell=[&](std::string text,float seconds,bool involved) {
        if(involved) {gang_news=std::move(text);gang_news_time=seconds;}
        else quiet_news=std::move(text);
    };
    const auto underworld=[&]{return player_gang()>=0 || career.on_duty();};
    // Who a body fights for: a gang (its crew), a ranching family, or -1.
    const auto faction_of=[&](int bot) -> int {
        if(bot<0) return -1;
        if(mafio.active()) if(const int g=mafio.gang_of(bot);g>=0) return g;
        if(ranches.active()) if(const auto* v=ranches.vakero(bot)) return game::mafia::family_faction(v->ranch);
        return -1;
    };
    combat_world.bind_vehicles([&](Vector3 start,Vector3 end,bool occupants){return vehicles.trace(verda_region,start,end,occupants);},[&](const game::combat::BulletHit& hit,float amount){
        vehicles.damage(verda_region,hit,amount);vehicle_dirty=true;
        // Through the glass into a bot at the wheel.
        if((hit.kind==game::combat::HitKind::VehicleWindow || hit.kind==game::combat::HitKind::VehicleOccupant) &&
           hit.target>=0 && static_cast<std::size_t>(hit.target)<vehicles.vehicles().size()) {
            const int driver=bot_drivers.driver_of(vehicles.vehicles()[static_cast<std::size_t>(hit.target)].id);
            if(driver>=0 && mafio.active()) {mafio.provoke(driver,match_time);walking_man.provoke(driver,match_time);ranches.provoke(driver,match_time);}
            if(driver>=0 && br_bots.damage_bot(driver,amount*.8F,hit.position,match_time)) {
                pending_kills.push_back(hit.position);
                career.killed(faction_of(driver),vehicles.driver()!=nullptr);
                territory.killed(faction_of(driver),hit.position,player_gang());
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
        if(body.bot==chief_bot) {
            // Nobody shoots the Chief - unless the boss ordered it (THE BIG HIT).
            if(!chief.placed || career.job().kind!=game::career::JobKind::HitChief) return false;
            chief.health-=damage;
            if(chief.health>0) return false;
            npcs.set_bot(chief_bot,chief.at,chief.yaw,0,characters::NpcState::Dead);
            chief.placed=false;
            career.chief_killed();
            pending_kills.push_back(where);
            return true;
        }
        if(lan_match && game::lan::LanPlay::is_remote_body(body.bot)) {
            // Someone on the player's side: no friendly fire.
            if(teammate(game::lan::LanPlay::slot_of(body.bot))) return false;
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
        if(killed && body.bot>=0) {
            const int victim=faction_of(body.bot);
            career.killed(victim,vehicles.driver()!=nullptr);
            territory.killed(victim,where,player_gang());
            // The Chief has not given the word on Verda City yet.
            if(const int vcrew=mafio.active() ? mafio.crew_of(body.bot) : -1;vcrew>=0 && career.serving() &&
               mafio.crews()[static_cast<std::size_t>(vcrew)].casino &&
               std::any_of(mafio.crews().begin(),mafio.crews().end(),[](const game::mafia::Crew& c){return !c.disbanded && !c.casino;}))
                career.warned_off_capital(match_time);
            // A rival gangster down on your own gang's turf: you defended the town.
            if(const int mine=player_gang();mine>=0 && victim>=0 && victim<game::mafia::gang_count && victim!=mine)
                if(const int turf=territory.turf_at(where);turf>=0) {
                    const auto& t=territory.turfs()[static_cast<std::size_t>(turf)];
                    if(t.ranch<0 && !t.cleared && t.owner==mine) {territory.defended(turf,mine);career.defended(t.name);}
                }
        }
        return killed;
    };
    combat_world.bind_actors([&](Vector3 start,Vector3 end) {
        const auto hit=npcs.trace_segment(start,end);
        return game::combat::BulletHit{hit.actor>=0 ? game::combat::HitKind::Npc : game::combat::HitKind::None,
            hit.fraction,hit.position,hit.normal,hit.actor,hit.headshot,hit.legs ? 2 : 0};
    },[&](int actor,float damage,Vector3 attacker) {damage_actor(actor,damage,attacker);});
    // The training camp's pop-up targets take real bullets.
    combat_world.bind_drill([&](Vector3 a,Vector3 b){return police_camp.trace(a,b);},[&](int target,bool head){police_camp.hit(target,head);});
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
                    lan.set_lobby(static_cast<std::uint8_t>(next),lobby.level,lobby.sides);
                    break;
                }
                case Kind::CycleLevel: lan.set_lobby(lobby.mode,static_cast<std::uint8_t>((lobby.level+1)%3),lobby.sides);break;
                case Kind::CycleSides: lan.set_lobby(lobby.mode,lobby.level,static_cast<std::uint8_t>((lobby.sides+1)%net::sides_count));break;
                case Kind::Start: lan.start_match((static_cast<std::uint64_t>(std::random_device{}())<<20)+static_cast<std::uint64_t>(GetTime()*1000.0)+1);break;
                default: break;
                }
                lan_start=lan.take_start();
                if(lan_start) lan_screen_open=false;
            } else {
                selected=queued_mode!=game::GameMode::Home ? queued_mode : home_screen.update(screen_width,screen_height);
                if(home_screen.take_lan()) {lan_screen_open=true;lan.browse();}
                // The campaign's save slots: what each holds, and erasing one (all its files).
                if(const int erase=home_screen.take_erase();erase>0) {
                    const std::filesystem::path loot=game::inventory::LootSave::slot_path(map_path,erase);
                    std::error_code ec;
                    for(const auto& entry:std::filesystem::directory_iterator(loot.parent_path(),ec))
                        if(entry.path().filename().string().starts_with(loot.filename().string())) std::filesystem::remove(entry.path(),ec);
                    std::filesystem::remove(std::filesystem::path(loot).replace_extension(".builds"),ec);
                    slots_refresh=0;
                }
                if(home_screen.slots_open() && (slots_refresh-=GetFrameTime())<=0) {
                    slots_refresh=.5F;
                    std::array<game::campaign::SlotInfo,game::HomeScreen::slot_count> slots;
                    for(int i=0;i<game::HomeScreen::slot_count;++i) {
                        const std::string loot=game::inventory::LootSave::slot_path(map_path,i+1);
                        std::string text;
                        if(FileExists((loot+".campaign").c_str())) if(char* t=LoadFileText((loot+".campaign").c_str())) {text=t;UnloadFileText(t);}
                        slots[static_cast<std::size_t>(i)]=game::campaign::describe(i+1,text,FileExists(loot.c_str()) || FileExists((loot+".career").c_str()));
                    }
                    home_screen.set_slots(slots);
                }
            }
            queued_mode=game::GameMode::Home;
            if(lan_start) {
                lan_match=true;lan_seed=lan_start->seed;lan_sides=static_cast<net::Sides>(lan_start->sides);spectate_slot=-1;squad_revive.reset();was_downed=false;
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
                // The campaign plays in the save slot chosen on the home screen.
                if(game_mode==game::GameMode::Explore && !lan_match) game::inventory::LootSave::set_campaign_slot(home_screen.slot());

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
                garage_here=world::garage::find(verda_region,garage_site);
                vehicles.reconcile(verda_region);
                if(game_mode==game::GameMode::BattleRoyale) {
                    match_fleet=vehicles.checkpoint(verda_region);match_fleet_active=true;fleet_was_dirty=vehicle_dirty;
                    vehicles.reset_match_fleet(verda_region);
                }
                npcs.reset_session();
                lan_play.clear();lan_state_wait=lan_snapshot_wait=0;lan_last_attacker=-1;lan_death_sent=lan_host_lost=false;
                squad_revive.reset();was_downed=false;player_health.set_downable(false);
                loot_session.drop_prefix=lan_match ? "drop_s"+std::to_string(lan.slot())+"_" : std::string{};
                build_mode.set_active(false);bus_dwell_seen.clear();
                if(game::rules_for(game_mode).civilian_life) {
                    builds_path=game::build::PlayerBuilds::path_beside(game::inventory::LootSave::profile_path(map_path,game_mode));
                    std::string builds_error;
                    if(!player_builds.load(builds_path,builds_error)) {TraceLog(LOG_WARNING,"Player builds: %s",builds_error.c_str());player_builds.clear();}
                } else player_builds.clear();
                player_builds.apply(verda_region); // other modes take the player's town away again
                // The homes for sale stand empty (Explore); other modes get the buildings back as they were.
                game::economy::restore_purposes(verda_region,vacated_homes);
                vacated_homes.clear();homes_for_sale.clear();home_here=was_in_home=nullptr;
                motor_sites.clear();motor_menu.close();motor_screen=MotorScreen::None;motor_site=motor_served=-1;motor_car.clear();
                owned_cars.clear();owned_ready=false;
                if(game::rules_for(game_mode).civilian_life) {
                    homes_for_sale=game::economy::plan_homes(verda_region);
                    motor_sites=world::motor::sites(verda_region);
                    churches=game::culture::find_churches(verda_region);church_seen=-1;
                    gossip=game::culture::Gossip{};gossip_heard.clear();crowd_church=-1;crowd_size=0;feast_said_day=-1;

                    // The street racers meet by the Marnafto station on the ring road.
                    street_race=game::racing::StreetRace{};race_cars_out=race_bots_out=false;
                    {
                        std::vector<Vector3> ring;
                        for(const auto& line:world::highways::lines()) if(line.loop) ring=line.points;
                        const world::motor::Site* meet=nullptr;
                        float off=200;
                        for(const auto& site:motor_sites)
                            if(site.kind==world::motor::Kind::Gas)
                                if(const float d=std::abs(std::hypot(site.origin.x,site.origin.z)-1500);d<off) {off=d;meet=&site;}
                        if(meet && street_race.setup(ring,meet->origin)) TraceLog(LOG_INFO,"OUTLAND street race meet at %.0f %.0f, %.0f m to the finish",meet->origin.x,meet->origin.z,street_race.length());
                    }
                    for(const auto& c:churches) TraceLog(LOG_INFO,"OUTLAND church: %s %s at %.0f %.0f%s",game::culture::denomination_name(c.denomination),c.settlement.c_str(),c.centre.x,c.centre.z,c.green_hand ? " (La Verda Mano)" : "");
                    vacated_homes=game::economy::vacate(verda_region,homes_for_sale);
                    TraceLog(LOG_INFO,"OUTLAND homes for sale: %d",static_cast<int>(homes_for_sale.size()));
                    for(const auto& home:homes_for_sale)
                        TraceLog(LOG_DEBUG,"OUTLAND for sale: %s (%s) %lld kr at %.0f %.0f",home.name.c_str(),home.building.c_str(),static_cast<long long>(home.price),home.board.x,home.board.z);
                }
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
                    furnish_island();
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
                outbreak.clear(npcs);outbreak_path.clear();outbreak_autosave=0;
                if(game::rules_for(game_mode).zombie_ecology) {
                    // The outbreak's Verda City is ground zero: twice and more the nests of an ordinary day.
                    const bool story_mode=game_mode==game::GameMode::ZombieSurvival && !lan_match;
                    game::zombies::ZombieConfig zombie_config;
                    if(story_mode) {zombie_config.capital_nests=22;zombie_config.population=260;}
                    zombies.configure(zombie_config);
                    zombies.build(verda_region,&character_registry,++zombie_seed);zombies.reset(npcs);
                    if(story_mode) {
                        outbreak_path=game::inventory::LootSave::profile_path(map_path,game_mode)+".outbreak";
                        game::outbreak::OutbreakState saved;
                        if(FileExists(outbreak_path.c_str())) if(char* text=LoadFileText(outbreak_path.c_str())) {
                            if(!game::outbreak::parse(text,saved)) saved={};
                            UnloadFileText(text);
                        }
                        outbreak.start(saved,verda_region,npcs,vehicles,zombies,&character_registry,outbreak_respawn);
                        // The filling stations hold what they hold (game::outbreak fuel): no attendants, no money.
                        for(const auto& site:world::motor::sites(verda_region)) if(site.kind==world::motor::Kind::Gas) motor_sites.push_back(site);
                        vehicle_dirty=true;
                        if(!explicit_spawn) {
                            player.position=Vector3Add(outbreak.start_feet(),{0,1,0});player.yaw=outbreak.start_yaw();player.vertical_velocity=0;
                        }
#ifdef OUTLAND_DEV_TOOLS
                        // OUTLAND_DEV_OUTBREAK=farm: at the wheel with Ron, just short of the Garzas' farm.
                        if(const char* skip=std::getenv("OUTLAND_DEV_OUTBREAK");skip && std::string(skip)=="farm") {
                            const auto truck=outbreak.skip_to_farm(verda_region,npcs,vehicles);
                            if(const int v=vehicles.find(truck);v>=0) {
                                const Vector3 at=vehicles.vehicles()[static_cast<std::size_t>(v)].position;
                                player.position=Vector3Add(at,{2,1,0});
                                vehicles.enter(verda_region,v,at);
                            }
                        }
#endif
                        TraceLog(LOG_INFO,"OUTLAND outbreak: %s, %d people, %zu holdouts, tower %s",game::outbreak::stage_name(outbreak.story().stage()),
                                 static_cast<int>(outbreak.people().size()),outbreak.holdouts().size(),outbreak.tower().found ? "found" : "missing");
                        save_outbreak();
                    }
                    zombies.clear_area({player.position.x,player.position.y-1,player.position.z},70); // a safe start
                } else zombies.clear(npcs);
                outbreak_respawn=false;
                if(poker_open) {
                    loot_session.economy().money=std::max<std::int64_t>(0,loot_session.economy().money+poker.cash_out()-poker_buy_in);
                    poker_open=false;poker_buy_in=0;
                }
                walking_man.clear();backup.clear();cb_backup.clear();kamionistoj.clear();society={};player_crew.clear();prison_yard.clear();jail_terms.clear();
                trucking=game::freight::Trucking{};trucking_path.clear();depot_boss={};haul_world={};
                career=game::career::Career{};career_path.clear();mafio.set_player_gang(-1);services.assign_player_car({});
                police_camp=game::career::TrainingCamp{};chief={};lights_car.clear();
                territory.clear();territory_path.clear();smuggling_docks.clear();
                endgame.clear();endgame_path.clear();endgame_saved.clear();
                mafio.clear(br_bots,bot_drivers,vehicles);
                callouts.reset();booking={};inside_lines.clear();inside_line=0;in_shu=false;sleep_fade=-1;prison_tour={};
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
                    // The other gangs take an empty plain house (not one that is lived in or for sale).
                    std::vector<int> lived_in(island_life.places().size(),0);
                    for(const auto& r:island_life.residents())
                        for(const int place:{r.home,r.visit_home})
                            if(place>=0 && place<static_cast<int>(lived_in.size())) ++lived_in[static_cast<std::size_t>(place)];
                    std::vector<std::string> not_for_gangs;
                    for(const auto& home:homes_for_sale) not_for_gangs.push_back(home.building);
                    mafio.start(island_life.places(),verda_region,br_bots,session_seed,lived_in,not_for_gangs);
                    house_tags.clear();fresh_tags.clear();tags_found.fill(0);
#ifdef OUTLAND_DEV_TOOLS
                    for(const auto& crew:mafio.crews()) TraceLog(LOG_INFO,"OUTLAND crew %s at %s: door %.1f %.1f yaw %.0f",
                        std::string(game::mafia::gangs[static_cast<std::size_t>(crew.gang)].name).c_str(),crew.town.c_str(),crew.door.x,crew.door.z,crew.yaw);
#endif
                    // Where crimes happen for the Policio's calls: shops and pubs (robberies) and houses (drive-bys).
                    call_scenes.clear();callouts.reset();
                    {
                        int homes=0;
                        for(const auto& place:island_life.places()) {
                            const bool shop=place.kind==game::life::PlaceKind::Shop || place.kind==game::life::PlaceKind::Pub;
                            const bool house=place.kind==game::life::PlaceKind::Home && !place.mixed_home && place.indoor;
                            if((!shop && !house) || (house && homes++%3!=0)) continue;
                            if(place.settlement<0 || static_cast<std::size_t>(place.settlement)>=verda_region.settlements().size()) continue;
                            const auto& town=verda_region.settlements()[static_cast<std::size_t>(place.settlement)];
                            if(town.id.starts_with("ranch_")) continue;
                            Vector3 out=Vector3Subtract(place.door,place.interior);out.y=0;
                            if(Vector3Length(out)<.1F) continue;
                            game::career::CallScene scene;
                            scene.door=place.door;scene.out=Vector3Normalize(out);scene.shop=shop;
                            scene.town=town.name.empty() ? town.id : town.name;
                            scene.place=place.kind==game::life::PlaceKind::Pub ? "the pub" : "the shop";
                            for(const auto& s:economy.shops()) if(Vector3Distance(s.door,place.door)<6) scene.place="the "+s.name;
                            call_scenes.push_back(std::move(scene));
                        }
                    }
                    if(const auto* casino=mafio.casino();casino &&
                       walking_man.start(island_life.places(),verda_region,casino->door,casino->table,br_bots,vehicles,session_seed,island_life.clock())) {
                        // His own face, not a gang member's.
                        const Vector3 at=walking_man.position(br_bots);
                        npcs.spawn_bot(walking_man.bot(),game::life::WalkingMan::character,at,0);
                    }
                    // Every gang wears its own colours (on the body the outfits fit).
                    if(character_registry.find(outfit_body))
                        for(const auto& crew:mafio.crews())
                            for(const int id:crew.members) if(const auto* bot=br_bots.find(id))
                                npcs.spawn_bot(id,outfit_body,bot->position(),bot->yaw());
                    // Each gang's GUN MAN, by its main base's door: guns for the gang's own people.
                    gun_men.clear();
                    if(character_registry.find(outfit_body))
                        for(const auto& crew:mafio.crews()) {
                            if(!crew.base || crew.disbanded) continue;
                            if(std::any_of(gun_men.begin(),gun_men.end(),[&](const GunMan& g){return g.gang==crew.gang;})) continue;
                            Vector3 out=Vector3Subtract(crew.door,crew.interior);out.y=0;
                            out=Vector3Length(out)>.1F ? Vector3Normalize(out) : Vector3{0,0,1};
                            const Vector3 side{-out.z,0,out.x};
                            for(const float along:{-3.2F,3.2F,-4.6F,4.6F}) {
                                Vector3 at=Vector3Add(Vector3Add(crew.door,Vector3Scale(out,1.6F)),Vector3Scale(side,along));
                                const float top=at.y+2;
                                at.y=world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region);
                                if(!std::isfinite(at.y) || world::physics::WorldCollision::body_blocked(at,at.y,verda_region,.4F)) continue;
                                gun_men.push_back({at,std::atan2(out.x,out.z)*RAD2DEG,crew.gang});
                                npcs.spawn_bot(gun_man_bot+crew.gang,outfit_body,at,gun_men.back().yaw);
                                break;
                            }
                        }
                    // The Vakeroj on their ranches (each in their own clothes and hat), then the big rigs.
                    ranches.start(verda_region,br_bots,vehicles,&character_registry,session_seed,island_life.clock());
                    for(const auto& v:ranches.vakeros())
                        if(const auto* bot=br_bots.find(v.bot);bot && !v.character.empty()) npcs.spawn_bot(v.bot,v.character,bot->position(),bot->yaw());
                    if(freight.build(verda_region,vehicles,bus_line,&ranches,&character_registry,session_seed)) freight.start(verda_region,vehicles);
                    TraceLog(LOG_INFO,"OUTLAND ranches: %d ranches, %d Vakeroj, %d cattle rigs/supply rigs %d, %d trailer reversals",
                             static_cast<int>(ranches.ranches().size()),static_cast<int>(ranches.vakeros().size()),
                             static_cast<int>(freight.rigs().size()),freight.running() ? 1 : 0,freight.reversals());
                    // An officer's backup: two officers in uniform, off duty until the radio calls them.
                    {
                        Vector3 park{};
                        for(const auto& town:verda_region.settlements()) if(town.id=="capital_verda") park=town.center;
                        backup.start(br_bots,park,session_seed);
                        for(std::size_t k=0;k<backup.ids().size();++k)
                            npcs.spawn_bot(backup.ids()[k],k==0 ? "character_18_police" : "character_21_police",park,0);
                        // The squad car from an earlier session, if one was left about.
                        for(const auto& v:vehicles.vehicles())
                            if(v.id.starts_with("creator_vehicle_"))
                                if(const auto* asset=vehicles.asset(verda_region,v.id);asset && asset->vehicle.definition=="police") {backup.set_car(v.id);break;}
                        if(!backup.car().empty()) bot_drivers.reserve(backup.car());
                        // The partner a desk can give an officer.
                        player_crew.start(br_bots,park,session_seed);
                        // The brotherhood's two on the CB: truckers in their everyday clothes.
                        cb_backup=game::career::Backup{};cb_backup.set_truckers();
                        cb_backup.start(br_bots,park,session_seed+31);
                        for(std::size_t k=0;k<cb_backup.ids().size();++k) {
                            std::string body="character_05";
                            for(std::uint64_t n=0;n<40;++n)
                                if(const auto* d=character_registry.choose(characters::CharacterPool::Civilian,session_seed+311+k*53+n);
                                   d && d->id.find("female")==std::string::npos && d->id.find("police")==std::string::npos && d->id.find("doctor")==std::string::npos) {body=d->id;break;}
                            npcs.spawn_bot(cb_backup.ids()[k],body,park,0);
                        }
                        npcs.spawn_bot(player_crew.partner_bot(),"character_21_police",park,0);
                    }
                    // How it ends: the parliament's guard on Registaro de Verda's steps, and a garrison
                    // for each county seat's square (both out of the game until their part comes).
                    endgame.clear();
                    if(game::rules_for(game_mode).civilian_life) {
                        const auto open_ground=[&](Vector3 c) -> std::optional<Vector3> {
                            for(float r=0;r<=48;r+=6)
                                for(int k=0;k<(r==0 ? 1 : 12);++k) {
                                    const float a=static_cast<float>(k)*PI/6;
                                    const float x=c.x+std::cos(a)*r,z=c.z+std::sin(a)*r;
                                    const float top=std::max(world::terrain::TerrainHeight::sample(x,z),c.y)+3;
                                    const float y=world::physics::WorldCollision::ground_height({x,top,z},top,verda_region);
                                    if(!std::isfinite(y) || y<world::layout::sea_level+1) continue;
                                    if(!world::physics::WorldCollision::body_blocked({x,y,z},y,verda_region,.9F)) return Vector3{x,y,z};
                                }
                            return std::nullopt;
                        };
                        const game::life::Place* hall=nullptr;
                        for(const auto& place:island_life.places()) if(place.civic_hall) {hall=&place;break;}
                        std::vector<Vector3> posts;std::vector<std::string> towns;
                        for(const auto& zone:world::zones::zones)
                            for(const auto& town:verda_region.settlements()) if(town.id==zone.seat)
                                if(const auto at=open_ground({town.center.x,world::terrain::TerrainHeight::sample(town.center.x,town.center.z),town.center.z})) {
                                    posts.push_back(*at);towns.push_back(town.name.empty() ? town.id : town.name);
                                }
                        if(hall && !posts.empty()) {
                            Vector3 out=Vector3Subtract(hall->door,hall->interior);out.y=0;
                            out=Vector3Length(out)>.1F ? Vector3Normalize(out) : Vector3{0,0,1};
                            const auto steps=open_ground(Vector3Add(hall->door,Vector3Scale(out,7)));
                            endgame.start(br_bots,steps.value_or(hall->door),hall->interior,std::atan2(out.x,out.z)*RAD2DEG,posts,towns,session_seed);
                            // The guard in the Policio's uniform, the troops in theirs (on the body the outfits fit).
                            for(const auto& bot:br_bots.bots()) {
                                if(endgame.guard(bot.id())) npcs.spawn_bot(bot.id(),bot.id()%2 ? "character_18_police" : "character_21_police",bot.position(),0);
                                else if(endgame.soldier(bot.id()) && character_registry.find(outfit_body)) npcs.spawn_bot(bot.id(),outfit_body,bot.position(),0);
                            }
                            TraceLog(LOG_INFO,"OUTLAND endgame: the parliament's guard at %.0f %.0f, %d garrisons",hall->door.x,hall->door.z,static_cast<int>(posts.size()));
                            for(std::size_t g=0;g<posts.size();++g) TraceLog(LOG_INFO,"OUTLAND endgame garrison %s at %.0f %.1f %.0f",towns[g].c_str(),posts[g].x,posts[g].y,posts[g].z);
                        }
                    }
                    // The state prison's guards and inmates.
                    {
                        const Vector3 c=world::layout::prison_center;
                        prison_yard.start({c.x,world::terrain::TerrainHeight::sample(c.x,c.z),c.z},world::layout::prison_yaw,session_seed,prison_body);
                        for(const auto& p:prison_yard.people()) prison_spawn(p);
                        jail_terms.clear();jail_terms.seed(session_seed^0x9e3779b97f4a7c15ULL);
                    }
                    br_bots.sync_bodies(npcs,&character_registry);
                    walking_man.dress(npcs);
                    services.start(vehicles,verda_region,&character_registry);
                    // The Ŝosea Patrolo's plan: five places on the ring road, the first by Verda City.
                    patrol_posts.clear();patrol_open=0;ranch_alarm.fill(false);
                    for(auto& town:verda_region.runtime_settlements())
                        std::erase_if(town.assets,[](const world::WorldAsset& a){return a.id.starts_with("patrol_outpost_");});
                    if(game::rules_for(game_mode).civilian_life) {
                        Vector3 capital{};
                        for(const auto& town:verda_region.settlements()) if(town.id=="capital_verda") capital=town.center;
                        // Open ground: nothing built or placed within reach (the ranches' fences included).
                        const auto free=[&](Vector3 at) {
                            // Well clear of the farms (fields, pens and the yard in front of the gate) and the stockyard.
                            for(const auto& ranch:world::ranches::ranches) {
                                const world::ranches::Frame frame(ranch.gate,ranch.up_track);
                                const auto& f=world::ranches::footprint;
                                if(frame.inside(at,{f.u0-40,f.u1+40,f.v0-45,f.v1+40})) return false;
                            }
                            {
                                const world::ranches::Frame frame(world::ranches::stockyard_gate,world::ranches::stockyard_up);
                                const auto& f=world::ranches::stockyard_footprint;
                                if(frame.inside(at,{f.u0-40,f.u1+40,f.v0-45,f.v1+40})) return false;
                            }
                            for(const auto& town:verda_region.settlements()) {
                                for(const auto& asset:town.assets)
                                    if(std::hypot(asset.position.x-at.x,asset.position.z-at.z)<std::max(asset.size.x,asset.size.z)*.5F+7) return false;
                                for(const auto& building:town.buildings)
                                    if(std::hypot(building.position.x-at.x,building.position.z-at.z)<std::max(building.size.x,building.size.z)*.5F+7) return false;
                            }
                            return true;
                        };
                        patrol_posts=game::services::plan_outposts(capital,free);
                        for(auto& post:patrol_posts) {
                            post.at.y=world::terrain::TerrainHeight::sample(post.at.x,post.at.z);
                            post.parking.y=world::terrain::TerrainHeight::sample(post.parking.x,post.parking.z);
                        }
                        services.set_beat(game::services::patrol_loop(250));
                        patrol_art.prepare(sign_renderer.font());
                    }
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
                    // OUTLAND_DEV_CAREER=gang:N[:respect], police or trucker: start this session in a gang, on the force or in the brotherhood.
                    if(const char* dev=std::getenv("OUTLAND_DEV_CAREER"); dev && *dev) {
                        std::string why;const std::string want=dev;
                        career=game::career::Career{};
                        if(want=="trucker") career.join_truckers(why);
                        else if(want.rfind("gang:",0)==0) {
                            const int gang=std::clamp(std::atoi(want.c_str()+5),0,static_cast<int>(game::mafia::gangs.size())-1);
                            career.join_gang(gang,std::string(game::mafia::gangs[static_cast<std::size_t>(gang)].name),why);
                            loot_session.edit_inventory().add(item_registry,"spray_paint",4);
                            if(const auto colon=want.find(':',5);colon!=std::string::npos) {
                                auto st=career.state();st.respect=std::clamp(static_cast<float>(std::atof(want.c_str()+colon+1)),0.0F,100.0F);career.restore(st);
                            }
                        } else if(want=="police") {
                            const Vector3 here{player.position.x,player.position.y-1,player.position.z};
                            const game::services::Unit* best=nullptr;
                            for(const auto& u:services.units()) if(u.kind==game::services::UnitKind::Police &&
                                (!best || Vector3Distance(u.home,here)<Vector3Distance(best->home,here))) best=&u;
                            if(best) career.join_police(best->vehicle,why);
                        } else if(want.rfind("police:",0)==0) {
                            // police:training / police:graduated / police:serving - further along the questline.
                            for(const auto& u:services.units()) if(u.kind==game::services::UnitKind::Police) {career.join_police(u.vehicle,why);break;}
                            bool car=false;
                            career.chief_talk(car);
                            if(want!="police:training") career.passed_training();
                            if(want=="police:serving") {career.chief_talk(car);career.issue_car("creator_vehicle_reissue");} // the Interceptor at HQ, below
                        }
                    }
#endif
                    mafio.set_player_gang(career.state().side==game::career::Side::Gang ? career.state().gang : -1);
                    // The Chief, at Verda City HQ: beside the capital's first patrol car, facing it.
                    chief={};chief_line.clear();chief_line_time=0;lights_car.clear();
                    for(const auto& u:services.units()) {
                        if(u.kind!=game::services::UnitKind::Police || u.vehicle.find("capital_verda")==std::string::npos) continue;
                        const float r=u.home_yaw*DEG2RAD;
                        const Vector3 side{std::cos(r),0,-std::sin(r)},ahead{std::sin(r),0,std::cos(r)};
                        for(const Vector3 offset:{Vector3Scale(side,-3.2F),Vector3Scale(side,3.2F),Vector3Scale(ahead,-4.5F),Vector3Scale(ahead,4.5F)}) {
                            const Vector3 at=Vector3Add(u.home,offset);
                            const float top=u.home.y+2;
                            const Vector3 feet{at.x,world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region),at.z};
                            if(!std::isfinite(feet.y) || std::abs(feet.y-u.home.y)>1.2F || world::physics::WorldCollision::body_blocked(feet,feet.y,verda_region,.4F)) continue;
                            chief={feet,std::atan2(u.home.x-feet.x,u.home.z-feet.z)*RAD2DEG,true};
                            break;
                        }
                        if(chief.placed) break;
                    }
                    if(career.state().chief_dead) chief.placed=false;   // hit in an earlier session: the office is empty
                    chief.health=100;
                    if(chief.placed) TraceLog(LOG_INFO,"OUTLAND police chief at %.1f %.1f facing %.0f",chief.at.x,chief.at.z,chief.yaw);
                    if(chief.placed) {
                        const std::string character=character_registry.find(chief_character) ? chief_character : "bot";
                        npcs.spawn_bot(chief_bot,character,chief.at,chief.yaw);
                    }
                    career.seed(session_seed);   // which jobs come up: different every session
                    // Verda Transporto: the boss at the depot door, the places loads go, the player's trucking.
                    trucking=game::freight::Trucking{};trucking.seed(session_seed);haul_world={};depot_boss={};
                    trucking_path=career_path+".trucking";
                    if(FileExists(trucking_path.c_str())) if(char* text=LoadFileText(trucking_path.c_str())) {trucking.parse(text);UnloadFileText(text);}
                    trucking_saved=trucking.serialise();
                    stats=game::player::PlayerStats{};stats_path=career_path+".stats";stats_money=-1;stats_placed=false;
                    if(FileExists(stats_path.c_str())) if(char* text=LoadFileText(stats_path.c_str())) {stats.parse(text);UnloadFileText(text);}
                    stats_saved=stats.serialise();
                    // La Verda Basballigo: the parks, and the league where it stood (or a new one today).
                    parks=world::ballpark::find(verda_region);
                    league=game::sports::League{};
                    league_path=career_path+".league";
                    {
                        bool loaded=false;
                        if(FileExists(league_path.c_str())) if(char* text=LoadFileText(league_path.c_str())) {loaded=league.parse(text);UnloadFileText(text);}
                        if(!loaded) league.start(island_life.clock().day(),session_seed);
                    }
                    for(const int id:rioters) npcs.despawn_resident(id);
                    rioters.clear();riot_park=-1;stand_seat={};park_fight_until={};game_fans.clear();
#ifdef OUTLAND_DEV_TOOLS
                    // OUTLAND_DEV_LEAGUE=game|gates|final|riot[:park]: a game at that park about to start (or its gates about to open; or
                    // the final, at that park), or the riot on.
                    if(const char* dev=std::getenv("OUTLAND_DEV_LEAGUE");dev && *dev) {
                        std::string what=dev;
                        int park=-1;
                        if(const auto colon=what.find(':');colon!=std::string::npos) {park=std::atoi(what.c_str()+colon+1);what.resize(colon);}
                        const int today=island_life.clock().day();
                        std::uint64_t seed=session_seed;
                        league.start(today,seed);
                        // The final is at the leader's park: try seasons till it falls at that one.
                        if(what!="game" && what!="gates" && park>=0)
                            for(int i=0;i<500 && league.fixtures().back().home!=park;++i) league.start(today,seed+=2);   // (the seed is made odd)
                        int fixture=what=="game" || what=="gates" ? 0 : static_cast<int>(league.fixtures().size())-1;
                        if((what=="game" || what=="gates") && park>=0) for(int i=0;i<static_cast<int>(league.fixtures().size());++i) if(league.fixtures()[static_cast<std::size_t>(i)].home==park) {fixture=i;break;}
                        league.start(today-league.fixtures()[static_cast<std::size_t>(fixture)].day,seed);
                        const double pitch=league.first_pitch_at(fixture);
                        const double at=what=="riot" ? pitch+league.linescore(fixture).length+2 : what=="gates" ? pitch-game::sports::League::pregame-20 : pitch-(what=="game" ? 8 : 3);
                        island_life.set_clock(game::life::WorldClock{at});
                        const auto& f=league.fixtures()[static_cast<std::size_t>(fixture)];
                        TraceLog(LOG_INFO,"OUTLAND league dev: %s at park %d (%s v %s)",what.c_str(),f.home,game::sports::teams[static_cast<std::size_t>(f.home)].nick,game::sports::teams[static_cast<std::size_t>(f.away)].nick);
                    }
#endif
                    league.update(island_life.clock().minutes);
                    league_saved=league.serialise();
                    {
                        bool depot=false;
                        for(const auto& town:verda_region.settlements()) for(const auto& asset:town.assets) {
                            depot|=asset.model_path.ends_with("civic/truck_depot.glb");
                            haul_world.terminal|=asset.model_path.ends_with("civic/cargo_ship.glb");
                        }
                        if(depot) {
                            const world::roads::RoadGraph graph(verda_region);
                            const auto gate=graph.nearest(world::layout::freight_depot_gate);
                            haul_world.depot=gate.point;
                            // The picket line for a strike: two brothers either side of the gate, on
                            // the verge facing the road (out of a rig's way), hidden till a strike.
                            picket_placed=false;
                            if(game::rules_for(game_mode).civilian_life && gate.edge>=0) {
                                const Vector3 side{gate.direction.z,0,-gate.direction.x};
                                const float off=gate.width*.5F+2.2F;
                                for(int k=0;k<picket_size;++k) {
                                    const float s_=k<2 ? -1.0F : 1.0F,along=(k%2==0 ? -1.6F : 1.6F);
                                    Vector3 at=Vector3Add(Vector3Add(gate.point,Vector3Scale(side,s_*off)),Vector3Scale(gate.direction,along));
                                    const float top=world::terrain::TerrainHeight::sample(at.x,at.z)+2;
                                    at.y=world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region);
                                    picket_spots[static_cast<std::size_t>(k)]=at;
                                    std::string body="character_05";
                                    for(std::uint64_t n=0;n<40;++n)
                                        if(const auto* d=character_registry.choose(characters::CharacterPool::Civilian,session_seed+1801+static_cast<std::uint64_t>(k)*41+n);
                                           d && d->id.find("police")==std::string::npos && d->id.find("doctor")==std::string::npos) {body=d->id;break;}
                                    // Facing the road (the side of the gate it stands on).
                                    const float yaw=std::atan2(-side.x*s_,-side.z*s_)*RAD2DEG;
                                    npcs.spawn_bot(picket_bot+k,body,at,yaw);
                                    npcs.set_bot_hidden(picket_bot+k,true);
                                    picket_yaws[static_cast<std::size_t>(k)]=yaw;
                                }
                                picket_placed=true;picket_out=false;
                            }
                            haul_world.cargo_quay=graph.nearest(world::layout::cargo_loading).point;
                            if(ranches.cattle_quay().found)
                                haul_world.cattle_quay=graph.nearest(ranches.cattle_quay().frame.at(world::ranches::cattle_quay_chute.x,-3)).point;
                            else haul_world.terminal=false;
                            for(const auto& town:verda_region.settlements()) {
                                if(town.id.starts_with("ranch_") || town.id.starts_with("hamlet_") || town.id.find("bovbazaro")!=std::string::npos) continue;
                                // Verda City's drop: Main Street west of the boulevard (its centre is the highway);
                                // the big towns': Verda Transporto's freight yard off the highway (dev::freight_yards).
                                Vector3 at=town.id=="capital_verda" ? Vector3{-87,0,-42} : town.center;
                                for(const auto& t:verda_region.settlements()) for(const auto& m:t.gameplay_markers)
                                    if(m.id=="freight_yard_"+town.id) at=m.position;
                                haul_world.towns.push_back({town.name,graph.nearest(at).point,-1});
                            }
                            for(std::size_t i=0;i<ranches.ranches().size();++i) {
                                const auto& r=ranches.ranches()[i];
                                haul_world.ranches.push_back({std::string(r.spec->name),graph.nearest(world::ranches::load_frame(r.frame).at(world::ranches::chute.x,-3)).point,static_cast<int>(i)});
                            }
                            const Vector3 b=world::layout::freight_boss;
                            depot_boss={{b.x,world::terrain::TerrainHeight::sample(b.x,b.z)+.1F,b.z},180,true};
                            std::string body="character_05";   // a man in everyday clothes, whoever the registry offers
                            for(std::uint64_t k=0;k<40;++k)
                                if(const auto* d=character_registry.choose(characters::CharacterPool::Civilian,session_seed+77+k);
                                   d && d->id.find("female")==std::string::npos && d->id.find("police")==std::string::npos && d->id.find("doctor")==std::string::npos) {body=d->id;break;}
                            npcs.spawn_bot(depot_boss_bot,body,depot_boss.at,depot_boss.yaw);
                            TraceLog(LOG_INFO,"OUTLAND Verda Transporto boss %s at %.1f %.1f",body.c_str(),depot_boss.at.x,depot_boss.at.z);
                        }
                    }
                    // La Frateco de Kamionistoj: at home at La Ringo on the ring road (Ivo Kovač at the bar's
                    // door, brothers about it and among the rigs), a brother at the depot's diesel lane, and a
                    // trucker outside a pub in every town (the brotherhood is everywhere a load goes).
                    kamionistoj.clear();society={};
                    if(game::rules_for(game_mode).civilian_life) {
                        const auto stand=[&](Vector3 c) -> std::optional<Vector3> {
                            for(float r=0;r<=14;r+=1.5F)
                                for(int k=0;k<(r==0 ? 1 : 10);++k) {
                                    const float a=static_cast<float>(k)*PI/5;
                                    const float x=c.x+std::cos(a)*r,z=c.z+std::sin(a)*r;
                                    const float top=std::max(world::terrain::TerrainHeight::sample(x,z),c.y)+2;
                                    const float y=world::physics::WorldCollision::ground_height({x,top,z},top,verda_region);
                                    if(std::isfinite(y) && !world::physics::WorldCollision::body_blocked({x,y,z},y,verda_region,.6F)) return Vector3{x,y,z};
                                }
                            return std::nullopt;
                        };
                        // Home: La Ringo Okcidento on the ring road. Ivo outside the bar's door, brothers about
                        // the door and down among the rigs in the truck park.
                        const auto& home=world::highways::brotherhood_home();
                        const auto ground=[&](Vector3 p){p.y=world::terrain::TerrainHeight::sample(p.x,p.z);return p;};
                        const Vector3 door=home.front_door(0),step=home.front_door(1);
                        const float facing=std::atan2(step.x-door.x,step.z-door.z)*RAD2DEG;
                        if(const auto at=stand(ground(home.front_door(2.4F,-2.2F))))
                            kamionistoj.push_back({*at,facing,"La Ringo",true});
                        for(const auto& [out,side]:std::initializer_list<std::pair<float,float>>{{3.2F,3.6F},{5.0F,4.8F}})
                            if(const auto at=stand(ground(home.front_door(out,side)))) kamionistoj.push_back({*at,facing+150,"La Ringo"});
                        // (In front of empty bays, out in the aisle, looking over the rigs.)
                        for(const int bay:{2,9,16}) {
                            const Vector3 b=home.bay(bay);
                            const float a=home.bay_yaw()*DEG2RAD;
                            if(const auto at=stand(ground({b.x+std::sin(a)*5.5F,0,b.z+std::cos(a)*5.5F})))
                                kamionistoj.push_back({*at,home.bay_yaw()+180,"La Ringo"});
                        }
                        // A brother still at the company's diesel lane in Verda Transporto's yard.
                        const Vector3 lane=world::layout::depot_diesel;
                        if(const auto at=stand({lane.x,world::terrain::TerrainHeight::sample(lane.x,lane.z),lane.z+9}))
                            kamionistoj.push_back({*at,0,"Verda Transporto's yard"});
                        std::vector<int> towns_done;
                        // A pub where the town has one, else its shop.
                        std::vector<const game::life::Place*> spots;
                        for(const auto kind:{game::life::PlaceKind::Pub,game::life::PlaceKind::Shop})
                            for(const auto& place:island_life.places()) if(place.kind==kind) spots.push_back(&place);
                        for(const auto* spot:spots) {
                            const auto& place=*spot;
                            if(place.settlement<0 || static_cast<std::size_t>(place.settlement)>=verda_region.settlements().size()) continue;
                            if(std::find(towns_done.begin(),towns_done.end(),place.settlement)!=towns_done.end()) continue;
                            const auto& town=verda_region.settlements()[static_cast<std::size_t>(place.settlement)];
                            if(town.id.starts_with("ranch_")) continue;
                            Vector3 out=Vector3Subtract(place.door,place.interior);out.y=0;
                            out=Vector3Length(out)>.1F ? Vector3Normalize(out) : Vector3{0,0,1};
                            const Vector3 side{-out.z,0,out.x};
                            if(const auto at=stand(Vector3Add(Vector3Add(place.door,Vector3Scale(out,2.2F)),Vector3Scale(side,2.0F)))) {
                                kamionistoj.push_back({*at,std::atan2(out.x,out.z)*RAD2DEG,town.name.empty() ? town.id : town.name});
                                towns_done.push_back(place.settlement);
                            }
                        }
                        for(std::size_t i=0;i<kamionistoj.size();++i) {
                            std::string body="character_05";
                            for(std::uint64_t n=0;n<40;++n)
                                if(const auto* d=character_registry.choose(characters::CharacterPool::Civilian,session_seed+1201+i*37+n);
                                   d && d->id.find("female")==std::string::npos && d->id.find("police")==std::string::npos && d->id.find("doctor")==std::string::npos) {body=d->id;break;}
                            npcs.spawn_bot(kamionisto_bot+static_cast<int>(i),body,kamionistoj[i].at,kamionistoj[i].yaw);
                        }
                        TraceLog(LOG_INFO,"OUTLAND truckers' brotherhood: %d Kamionistoj (Ivo Kovach at %.0f %.0f)",static_cast<int>(kamionistoj.size()),
                                 kamionistoj.empty() ? 0.0F : kamionistoj.front().at.x,kamionistoj.empty() ? 0.0F : kamionistoj.front().at.z);
                    }
                    // The training camp: open, level ground outside Verda City, downrange pointing away from it.
                    police_camp=game::career::TrainingCamp{};
                    {
                        Vector3 centre{};
                        for(const auto& town:verda_region.settlements()) if(town.id=="capital_verda") centre=town.center;
                        // Clear of every road (and a margin) as well as of buildings, trees and props.
                        const auto on_road=[&](float px,float pz) {
                            for(const auto& town:verda_region.settlements())
                                for(const auto& road:town.roads) {
                                    const Vector2 a{road.start.x,road.start.z},b{road.end.x,road.end.z},p{px,pz};
                                    const Vector2 ab=Vector2Subtract(b,a);
                                    const float len=Vector2LengthSqr(ab);
                                    const float t=len>0 ? std::clamp(Vector2DotProduct(Vector2Subtract(p,a),ab)/len,0.0F,1.0F) : 0;
                                    if(Vector2Distance(p,Vector2Add(a,Vector2Scale(ab,t)))<road.width*.5F+4) return true;
                                }
                            return false;
                        };
                        bool placed=false;
                        // First choice: right behind (or beside) Verda City's police HQ, downrange
                        // pointing away from it - recruits walk out of the back door onto the range.
                        {
                            Vector3 hq{},front{0,0,-1};bool found=false;float best=1e9F;
                            for(const auto& u:services.units()) {
                                if(u.kind!=game::services::UnitKind::Police || u.vehicle.find("capital_verda")==std::string::npos) continue;
                                for(const auto& place:island_life.places()) {
                                    if(place.kind!=game::life::PlaceKind::Police) continue;
                                    const float d=Vector3Distance(place.door,u.home);
                                    if(d<best && d<60) {best=d;hq=place.interior;front=Vector3Subtract(place.door,place.interior);found=true;}
                                }
                                break;
                            }
                            front.y=0;
                            if(found && Vector3Length(front)>.1F) {
                                front=Vector3Normalize(front);
                                const float behind=std::atan2(-front.x,-front.z);
                                const Vector3 back{std::sin(behind),0,std::cos(behind)},side{std::cos(behind),0,-std::sin(behind)};
                                // Behind it first, then stepping out to either side (clear of the highway
                                // and the streets), the range always pointing away from the station.
                                std::vector<Vector2> offsets;
                                for(float along=20;along<=150;along+=6)
                                    for(float across=0;across<=96;across+=8)
                                        for(const float sign:{-1.0F,1.0F}) {
                                            if(across==0 && sign>0) continue;
                                            offsets.push_back({sign*across,along});
                                        }
                                std::stable_sort(offsets.begin(),offsets.end(),[](Vector2 a,Vector2 b){
                                    return a.y+std::abs(a.x)*.8F<b.y+std::abs(b.x)*.8F;});
                                for(std::size_t k=0;k<offsets.size() && !placed;++k) {
                                        const float x=hq.x+back.x*offsets[k].y+side.x*offsets[k].x,z=hq.z+back.z*offsets[k].y+side.z*offsets[k].x;
                                        const float yaw=behind*RAD2DEG;
                                        const Vector3 right=side,ahead=back;
                                        float low=1e9F,high=-1e9F;bool ok=true;
                                        for(float lx=-22;lx<=22 && ok;lx+=4)
                                            for(float lz=-12;lz<=40 && ok;lz+=4) {
                                                const float px=x+right.x*lx+ahead.x*lz,pz=z+right.z*lx+ahead.z*lz;
                                                const float g=world::terrain::TerrainHeight::sample(px,pz);
                                                if(!std::isfinite(g) || on_road(px,pz) ||
                                                   world::physics::WorldCollision::body_blocked({px,g,pz},g,verda_region,.8F)) ok=false;
                                                low=std::min(low,g);high=std::max(high,g);
                                            }
                                        if(!ok || high-low>2.0F) continue;
                                        police_camp.place({x,world::terrain::TerrainHeight::sample(x,z),z},yaw,
                                            [](float px,float pz){return world::terrain::TerrainHeight::sample(px,pz);});
                                        placed=true;
                                        TraceLog(LOG_INFO,"OUTLAND police training camp by HQ at %.0f %.0f facing %.0f (%.0f m behind, %.0f m aside)",x,z,yaw,offsets[k].y,offsets[k].x);
                                    }
                            }
                        }
                        for(float radius=320;radius<=980 && !placed;radius+=45)
                            for(int k=0;k<28 && !placed;++k) {
                                const float a=static_cast<float>(k)*2*PI/28+radius*.013F;
                                const float x=centre.x+std::sin(a)*radius,z=centre.z+std::cos(a)*radius;
                                const float yaw=std::atan2(x-centre.x,z-centre.z)*RAD2DEG;
                                const float r=yaw*DEG2RAD;
                                const Vector3 right{std::cos(r),0,-std::sin(r)},ahead{std::sin(r),0,std::cos(r)};
                                float low=1e9F,high=-1e9F;bool ok=true;
                                for(float lx=-22;lx<=22 && ok;lx+=4)
                                    for(float lz=-12;lz<=40 && ok;lz+=4) {
                                        const float px=x+right.x*lx+ahead.x*lz,pz=z+right.z*lx+ahead.z*lz;
                                        const float g=world::terrain::TerrainHeight::sample(px,pz);
                                        if(!std::isfinite(g) || g<world::layout::sea_level+2.5F || on_road(px,pz) ||
                                           world::physics::WorldCollision::body_blocked({px,g,pz},g,verda_region,.8F)) ok=false;
                                        low=std::min(low,g);high=std::max(high,g);
                                    }
                                if(!ok || high-low>3.0F) continue;
                                police_camp.place({x,world::terrain::TerrainHeight::sample(x,z),z},yaw,
                                    [](float px,float pz){return world::terrain::TerrainHeight::sample(px,pz);});
                                placed=true;
                                TraceLog(LOG_INFO,"OUTLAND police training camp at %.0f %.0f facing %.0f",x,z,yaw);
                            }
                        if(career.on_duty() && career.police_stage()>=game::career::PoliceStage::Graduated) police_camp.restore(true);
                    }
                    // Who runs Verda: each crew's town (its county's gang to begin with) and each ranch.
                    territory.clear();island_ruled=false;gang_news.clear();gang_news_time=0;smuggling_docks.clear();
                    gang_logos.prepare();
                    for(std::size_t c=0;c<mafio.crews().size();++c) {
                        const auto& crew=mafio.crews()[c];
                        if(crew.settlement<0 || static_cast<std::size_t>(crew.settlement)>=verda_region.settlements().size()) continue;
                        const auto& town=verda_region.settlements()[static_cast<std::size_t>(crew.settlement)];
                        if(territory.turf_at(town.center)>=0) continue;   // one turf a town
                        territory.add_town(town.id,town.name,town.center,town.id=="capital_verda" ? 320.0F : 190.0F,crew.gang,static_cast<int>(c));
                    }
                    for(std::size_t r=0;r<ranches.ranches().size() && r<static_cast<std::size_t>(game::mafia::family_count);++r) {
                        const auto& ranch=ranches.ranches()[r];
                        if(!ranch.spec) continue;
                        territory.add_ranch(std::string(ranch.spec->id),std::string(ranch.spec->name),ranch.frame.at(0,110),150,static_cast<int>(r),
                            std::string(ranch.spec->family),game::mafia::gang_of_zone(ranch.spec->county),session_seed);
                    }
                    // Contraband comes ashore at the smugglers' jetties, and at Porto Luma's fishers'
                    // jetty after dark: the load waits at the foot of each, where the track or the
                    // quay meets it (the jetty's land end is its model's -Z).
                    for(const auto& town:verda_region.settlements())
                        for(const auto& a:town.assets) {
                            if(!a.model_path.ends_with("smugglers_jetty.glb") && !a.model_path.ends_with("fishers_jetty.glb")) continue;
                            const float r=a.rotation_y*DEG2RAD;
                            const Vector3 foot{a.position.x-std::sin(r)*25,0,a.position.z-std::cos(r)*25};
                            smuggling_docks.push_back({town.name+" jetty",{foot.x,world::terrain::TerrainHeight::sample(foot.x,foot.z),foot.z}});
                        }
                    diesel_lanes.clear();
                    for(const auto& town:verda_region.settlements())
                        for(const auto& a:town.assets) if(a.model_path.ends_with("diesel_island.glb")) diesel_lanes.push_back(a.position);
                    if(smuggling_docks.empty())
                        for(const auto& place:island_life.places())
                            if(place.kind==game::life::PlaceKind::Dock && static_cast<std::size_t>(place.settlement)<verda_region.settlements().size())
                                smuggling_docks.push_back({verda_region.settlements()[static_cast<std::size_t>(place.settlement)].name+" docks",place.door});
                    territory_path=game::inventory::LootSave::profile_path(map_path,game_mode)+".territory";
                    bool territory_loaded=false;
                    if(FileExists(territory_path.c_str())) if(char* text=LoadFileText(territory_path.c_str())) {
                        territory_loaded=territory.parse(text);
                        if(!territory_loaded) TraceLog(LOG_WARNING,"Territory: unreadable %s",territory_path.c_str());
                        UnloadFileText(text);
                    }
                    // Crews wear the colours of whoever holds their town now; the ones the Policio
                    // broke up are gone.
                    retired_bots.clear();
                    for(const auto& t:territory.turfs()) if(t.crew>=0 && t.ranch<0) {
                        mafio.set_crew_gang(t.crew,t.owner);
                        if(t.cleared) {
                            mafio.disband(t.crew);
                            for(const int id:mafio.crews()[static_cast<std::size_t>(t.crew)].members) {br_bots.retire(id);retired_bots.push_back(id);}
                        }
                    }
                    if(!territory_loaded) {
                        // Every hideout starts with its gang's tag beside the door.
                        for(const auto& crew:mafio.crews()) {
                            Vector3 out=Vector3Subtract(crew.door,crew.interior);out.y=0;
                            if(Vector3Length(out)<.1F) continue;
                            out=Vector3Normalize(out);
                            const Vector3 side{-out.z,0,out.x};
                            for(const float along:{1.9F,-1.9F,3.2F}) {
                                const Vector3 from=Vector3Add(Vector3Add(crew.door,Vector3Scale(out,1.2F)),Vector3Add(Vector3Scale(side,along),{0,1.7F,0}));
                                const auto wall=combat_world.trace_segment(from,Vector3Subtract(from,Vector3Scale(out,4.5F)),false,false,false);
                                if((wall.kind==game::combat::HitKind::Building || wall.kind==game::combat::HitKind::Structure) && std::abs(wall.normal.y)<.4F) {
                                    // The paint faces the street, whichever way the wall's face was wound.
                                    const Vector3 face=Vector3DotProduct(wall.normal,Vector3Subtract(from,wall.position))<0 ? Vector3Negate(wall.normal) : wall.normal;
                                    territory.tag(wall.position,face,crew.gang);break;
                                }
                            }
                        }
                    }
                    // The Green Hand's mural goes up once the city's walls can be traced (retried
                    // every couple of seconds until then).
                    green_hand_mural={};mural_attempts=0;mural_wait=0;banner_key.clear();sign_renderer.set_banners({});
                    gang_logos.prepare_mural(sign_renderer.verda_mano_texture(),sign_renderer.font());
                    TraceLog(LOG_INFO,"OUTLAND territory: %d turfs, %d tags, %d docks%s",static_cast<int>(territory.turfs().size()),
                             static_cast<int>(territory.tags().size()),static_cast<int>(smuggling_docks.size()),territory_loaded ? " (saved)" : "");
                    territory_saved=territory_loaded ? territory.serialise() : std::string();   // a fresh island is saved at once
                    // Where the endgame stood: the siege, the landing, the towns already freed.
                    endgame_path=game::inventory::LootSave::profile_path(map_path,game_mode)+".endgame";
                    endgame_saved.clear();
                    if(endgame.ready() && FileExists(endgame_path.c_str())) if(char* text=LoadFileText(endgame_path.c_str())) {
                        if(endgame.parse(text,br_bots)) endgame_saved=endgame.serialise();
                        else TraceLog(LOG_WARNING,"Endgame: unreadable %s",endgame_path.c_str());
                        UnloadFileText(text);
                    }
                    // The Verda Mano GT waits outside the Verda City hideout (one on the island).
                    if(const auto* home=mafio.casino()) {
                        bool parked=false;
                        for(const auto& v:vehicles.vehicles())
                            if(const auto* a=vehicles.asset(verda_region,v.id);a && a->vehicle.definition==game::mafia::green_hand_car) {
                                parked=true;
                                TraceLog(LOG_INFO,"OUTLAND Verda Mano GT at %.1f %.1f",v.position.x,v.position.z);
                            }
                        Vector3 out=Vector3Subtract(home->door,home->interior);out.y=0;
                        if(!parked && Vector3Length(out)>.1F) {
                            out=Vector3Normalize(out);
                            // Along the kerb: the car's right (its local -x) toward the hideout.
                            const float yaw=std::atan2(-out.z,out.x)*RAD2DEG;
                            const Vector3 along{-out.z,0,out.x};
                            bool done=false;
                            for(const float k:{0.0F,6.0F,-6.0F,12.0F,-12.0F,18.0F,-18.0F}) {
                                if(vehicles.spawn(verda_region,Vector3Add(Vector3Add(home->door,Vector3Scale(out,-1.0F)),Vector3Scale(along,k)),yaw,game::mafia::green_hand_car)) {
                                    done=vehicle_dirty=true;
                                    TraceLog(LOG_INFO,"OUTLAND Verda Mano GT parked outside %s",home->town.c_str());
                                    break;
                                }
                            }
                            if(!done) TraceLog(LOG_WARNING,"OUTLAND Verda Mano GT: no room outside %s",home->town.c_str());
                        }
                    }
                    island_ruled=player_gang()>=0 && territory.rules_island(player_gang());
                    // A serving officer whose Interceptor went missing gets another at HQ.
                    if(career.serving() && chief.placed && career.state().patrol_car.rfind("creator_vehicle_",0)==0 && vehicles.find(career.state().patrol_car)<0) {
                        std::vector<std::string> before;for(const auto& v:vehicles.vehicles()) before.push_back(v.id);
                        if(vehicles.spawn(verda_region,chief.at,chief.yaw,"interceptor"))
                            for(const auto& v:vehicles.vehicles()) if(std::find(before.begin(),before.end(),v.id)==before.end()) {
                                career.issue_car(v.id);vehicle_dirty=true;
                                TraceLog(LOG_INFO,"OUTLAND Interceptor %s at %.1f %.1f",v.id.c_str(),v.position.x,v.position.z);break;
                            }
                    }
                    services.assign_player_car(career.on_duty() ? career.state().patrol_car : std::string());
                    // Back from the dead: out of the hospital's front door.
                    penthouse.locate(verda_region);was_in_penthouse=false;
                    // Dead in Explore: an ambulance took them to the nearest clinic or the hospital,
                    // and they come round there, patched up, by its front door.
                    if(respawn_at_hospital)
                        if(const auto* site=services.nearest_medical(death_at)) {
                            const auto door=site->door;
                            player.position={door.x,world::physics::WorldCollision::ground_height({door.x,door.y+1.2F,door.z},door.y+1.2F,verda_region)+1,door.z};
                            interaction_message="You wake up at "+site->name+(site->hospital ? "" : " - the ambulance brought you in");interaction_remaining=5;
                        }
                }
                // The campaign: where this slot is in the story. A fresh slot starts on the quay at
                // Porto Luma, just off the ship, with Uncle Ron waiting by his truck; a slot already
                // under way carries on where it was saved (or at a clinic's door, after dying).
                story=game::campaign::Story{};campaign={};uncle_ron={};ron_home=-1;autosave_wait=autosave_every;autosave_note=0;
                campaign_path.clear();
                if(game_mode==game::GameMode::Explore && !lan_match) {
                    const std::string loot=game::inventory::LootSave::profile_path(map_path,game_mode);
                    campaign_path=loot+".campaign";
                    bool known=false;
                    if(FileExists(campaign_path.c_str())) if(char* text=LoadFileText(campaign_path.c_str())) {known=game::campaign::parse(text,campaign);UnloadFileText(text);}
                    if(!known) {campaign={};if(FileExists(loot.c_str()) || FileExists(career_path.c_str())) campaign.stage=game::campaign::Stage::Free;}   // a save from before the story
#ifdef OUTLAND_DEV_TOOLS
                    // OUTLAND_DEV_STORY=port|home|skip: start the story on the quay, at Uncle Ron's, or skip it.
                    if(const char* want=std::getenv("OUTLAND_DEV_STORY")) {
                        const std::string w=want;
                        campaign.stage=w=="port" ? game::campaign::Stage::Arrival : w=="home" ? game::campaign::Stage::HomeTalk : game::campaign::Stage::Free;
                    }
#endif
                    story.start(campaign.stage);
                    phone=game::campaign::Phone{};phone.set_done(campaign.calls);
#ifdef OUTLAND_DEV_TOOLS
                    if(const char* soon=std::getenv("OUTLAND_DEV_PHONE"); soon && *soon=='1') phone.ring_in(5);   // dev: a call within seconds
#endif
                    // Uncle Ron's house: the first house in Okcidenta Verda on the list (not for sale - it's his).
                    for(std::size_t i=0;i<homes_for_sale.size();++i) if(homes_for_sale[i].house) {ron_home=static_cast<int>(i);break;}
                    if(ron_home>=0) {auto& home=homes_for_sale[static_cast<std::size_t>(ron_home)];home.name="Uncle Ron's house";home.price=0;}
                    // Uncle Dario's flat for family: the nearest Main Street building to the Green Hand's bar
                    // that is nobody's shop, pub or listing.
                    dario_flat=-1;
                    if(const auto* bar=mafio.casino();bar && bar->gang==0) {
                        const world::WorldAsset* best=nullptr;float nearest=150;
                        for(const auto& town:verda_region.settlements()) for(const auto& asset:town.assets) {
                            if(asset.model_path.find("/city/main_street/")==std::string::npos || asset.purpose!=world::BuildingPurpose::Auto || asset.id==bar->building) continue;
                            const float d=std::hypot(asset.position.x-bar->door.x,asset.position.z-bar->door.z);
                            if(d<12 || d>=nearest) continue;
                            bool listed=false;
                            for(const auto& home:homes_for_sale) listed|=home.building==asset.id;
                            bool used=false;
                            for(const auto& place:island_life.places())
                                used|=place.building_id==asset.id && place.kind!=game::life::PlaceKind::Home;
                            if(!listed && !used) {best=&asset;nearest=d;}
                        }
                        if(best) {
                            game::economy::Listing flat;
                            flat.building=best->id;flat.id="home_"+best->id;flat.name="Green Hand flat (Uncle Dario's)";
                            flat.centre=best->position;flat.yaw=best->rotation_y;flat.size=best->size;flat.house=false;flat.price=0;
                            flat.board=flat.centre;
                            homes_for_sale.push_back(flat);
                            dario_flat=static_cast<int>(homes_for_sale.size())-1;
                            const auto freed=game::economy::vacate(verda_region,{flat});
                            vacated_homes.insert(vacated_homes.end(),freed.begin(),freed.end());
                            // A member of the family already: the keys are his.
                            if(career.in_gang(0)) own_dario_flat();
                            else if(game::economy::owns(loot_session.economy(),flat)) furnish_island();
                        }
                    }
                    place_story(explicit_spawn,respawn_at_hospital);
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
            save_campaign(false);campaign_path.clear();
            save_outbreak();outbreak.clear(npcs);outbreak_path.clear();
            restore_match_fleet();
            vehicles.leave_session();vehicle_audio.stop();radio_audio.stop();vehicle_renderer.reset_camera();
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
                else if(escape && store_open) {store_open=false;input_system.cancel_controls();}
                else if(escape && (motor_menu.is_open()||gear_menu.is_open())) {}   // (its own ESC: back, or leave)
                else if(escape && build_mode.active()) {}
                else if(escape || lost_mouse || menu_tapped) {pause_menu.open();input_system.cancel_controls();}
            }
        }
        // Leaving a side, from the boss's / Ivo's / the desk's confirm screen or the pause menu's LEAVE.
        const auto leave_side=[&](Pending what) {
            const auto feet2=Vector3{player.position.x,player.position.y-1,player.position.z};
            if(what==Pending::LeaveGang) {
                for(const int id:player_crew.dismiss(br_bots)) mafio.lend(id,false,br_bots,feet2);
                career.quit();mafio.set_player_gang(-1);island_ruled=false;join_offer={};
                chief_line=career.state().record ? "You walked away - your record stays" : "You walked away with your honour - your record is clean";
            } else if(what==Pending::LeaveBrotherhood) {
                if(cb_backup.active()) cb_backup.dismiss(br_bots,match_time);
                career.quit();
                chief_line=std::string(brotherhood_man)+": \"Going it alone? Out there? ...Your call, brother. The door's open if you come back.\"";
            } else if(what==Pending::HandInBadge) {
                career.quit();services.assign_player_car({});lights_car.clear();join_offer={};
                if(backup.active()) backup.dismiss(br_bots,match_time);
                player_crew.dismiss_partner(br_bots);
                chief_line="You handed in your badge";
            }
            chief_line_time=5;
        };
        const auto side_to_leave=[&] {
            if(!game::rules_for(game_mode).civilian_life || career.in_prison()) return Pending::None;
            switch(career.state().side) {
            case game::career::Side::Gang: return Pending::LeaveGang;
            case game::career::Side::Police: return Pending::HandInBadge;
            case game::career::Side::Trucker: return Pending::LeaveBrotherhood;
            default: return Pending::None;
            }
        };
        if(pause_menu.is_open()) {
            settings.volume=environment_audio.volume();settings.muted=environment_audio.muted();
            // The touch layout editor, opened from SETTINGS, takes the screen until DONE or CANCEL.
            if(touch_editor.is_open()) {
                if(touch_editor.update(settings,screen_width,screen_height,IsWindowFocused())==game::TouchLayoutEditor::Result::Done) {apply_settings();store_settings();}
                if(!touch_editor.is_open()) input_system.cancel_controls();
            }
            {
                const auto side=side_to_leave();
                pause_menu.set_leave(side==Pending::LeaveGang ? "LEAVE THE GANG" : side==Pending::HandInBadge ? "HAND IN YOUR BADGE" :
                                     side==Pending::LeaveBrotherhood ? "LEAVE THE BROTHERHOOD" : "");
            }
            const auto result=touch_editor.is_open() ? game::PauseMenu::Result::None :
                pause_menu.update(settings,screen_width,screen_height,IsWindowFocused(),GetFrameTime(),can_quit_game);
            if(result==game::PauseMenu::Result::EditTouchLayout) touch_editor.open(settings);
            if(result==game::PauseMenu::Result::SettingsChanged) {apply_settings();store_settings();}
            if(result==game::PauseMenu::Result::Resume) input_system.cancel_controls();
            if(result==game::PauseMenu::Result::Leave) {
                if(gear_menu.is_open()) {gear_menu.close();gear_screen=GearScreen::None;}
                leave_side(side_to_leave());pending=Pending::None;input_system.cancel_controls();
            }
            if(result==game::PauseMenu::Result::Save) {
                bool saved=game_mode==game::GameMode::DevLab || loot_session.save(weapons) || !loot_session.can_persist();
                saved=save_campaign(false) && saved;
                save_outbreak();
                if(vehicle_dirty && !vehicle_save_protected && vehicles.save_state(verda_region,vehicle_state_path,vehicle_error)) vehicle_dirty=false;
                if(!builds_path.empty() && game::rules_for(game_mode).civilian_life) {
                    std::string builds_error;
                    if(!player_builds.save(builds_path,builds_error)) {saved=false;TraceLog(LOG_WARNING,"Player builds: %s",builds_error.c_str());}
                }
                store_settings();
                pause_menu.set_status(saved ? std::string("GAME SAVED") : std::string("SAVE FAILED - ")+loot_session.status());
            }
            if(result==game::PauseMenu::Result::Restart) {
                // Saves like QUIT TO MENU, then straight back into the same mode, fresh. Not in a LAN match.
                const auto mode=game_mode;
                if(lan_match) {pause_menu.open();pause_menu.set_status(std::string("LEAVE THE LAN MATCH TO RESTART"));}
                else if(!go_home()) {pause_menu.open();pause_menu.set_status(std::string("COULD NOT SAVE - ")+interaction_message);}
                else queued_mode=mode;
                continue;
            }
            if(result==game::PauseMenu::Result::QuitHome || result==game::PauseMenu::Result::QuitGame) {
                if(!go_home()) {pause_menu.open();pause_menu.set_status(std::string("COULD NOT SAVE - ")+interaction_message);continue;}
                if(result==game::PauseMenu::Result::QuitGame) break;
                // Finish this frame so the Home screen does not reuse the same click.
                BeginDrawing();
                home_screen.draw(screen_width, screen_height);
                EndDrawing();
                continue;
            }
        } else pause_menu.update(settings,screen_width,screen_height,false,GetFrameTime(),can_quit_game);

        if(mode_session.terminal()) {
            // Out, with the side fighting on: watch them (TAB or a tap for the next); H goes home.
            auto action=game::ResultAction::None;
            if(mode_session.squad_fighting()) {
                std::vector<std::uint8_t> watchable;
                for(const auto& r:lan_play.remotes()) if(r.known && (r.state.flags&net::StateAlive) && teammate(r.slot)) watchable.push_back(r.slot);
                const bool next=IsWindowFocused() && (IsKeyPressed(KEY_TAB) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
                const auto current=std::find(watchable.begin(),watchable.end(),static_cast<std::uint8_t>(std::max(0,spectate_slot)));
                if(!watchable.empty() && (current==watchable.end() || spectate_slot<0)) spectate_slot=watchable.front();
                else if(next && !watchable.empty()) spectate_slot=*(current+1==watchable.end() ? watchable.begin() : current+1);
                if(IsWindowFocused() && IsKeyPressed(KEY_H)) action=game::ResultAction::Home;
            } else action=play_ui.result_input(screen_width,screen_height,IsWindowFocused());
            if(action!=game::ResultAction::None) {
                if(!loot_session.save(weapons) && loot_session.can_persist()) {
                    interaction_message=loot_session.status();interaction_remaining=3;
                } else {
                    queued_mode=action==game::ResultAction::Restart ? game_mode : game::GameMode::Home;
                    respawn_at_hospital=action==game::ResultAction::Restart && game_mode==game::GameMode::Explore && !player_health.alive();
                    outbreak_respawn=action==game::ResultAction::Restart && game_mode==game::GameMode::ZombieSurvival && !player_health.alive();
                    if(outbreak.active()) {
                        if(!player_health.alive()) outbreak.state().has_position=false;   // back at the hideout, not where you fell
                        save_outbreak();
                    }
                    death_at=player.position;
                    if(lan_match) {
                        const bool again=action==game::ResultAction::Restart && lan.in_match();
                        if(again && game_mode!=game::GameMode::BattleRoyale) lan_restart=true; // back into the same world, with the others
                        else if(again) {queued_mode=game::GameMode::Home;lan.end_match();lan_screen_open=true;lan_match=false;} // the lobby, for the next one
                        else {lan.stop();lan_match=false;}
                        lan_play.clear(&npcs);
                    }
                    restore_match_fleet();
                    vehicles.leave_session();vehicle_audio.stop();radio_audio.stop();vehicle_renderer.reset_camera();
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
        if(!map_screen.is_open() && !pause_menu.is_open() && game_mode!=game::GameMode::DevLab && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open && !mode_session.terminal() &&
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
        } else if(shop_open || store_open || poker_open || mode_session.terminal()) {
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
                // At the wheel only a roof gun fires (the FIRE button shows for it).
                const auto* wheel=vehicles.driver();
                const auto* wheel_asset=wheel ? vehicles.asset(verda_region,wheel->id) : nullptr;
                const bool armed_at_wheel=wheel_asset && wheel_asset->vehicle.gun && !wheel_asset->vehicle.destroyed;
                input_system.set_drive_by(armed_at_wheel);touch_hud.set_drive_by(armed_at_wheel);
                // At the wheel of a police car (any patrol car, or the Interceptor): LIGHTS.
                const auto* car=vehicles.driver();
                const auto* asset=car ? vehicles.asset(verda_region,car->id) : nullptr;
                const bool police_car=asset && (asset->vehicle.definition=="police" || asset->vehicle.definition=="interceptor");
                input_system.set_police_car(police_car);touch_hud.set_police_car(police_car,police_car && lights_car==car->id);
                const bool tagger=!car && player_gang()>=0;
                input_system.set_tagger(tagger);touch_hud.set_tagger(tagger);
                const bool officer=!car && career.serving() && game_mode==game::GameMode::Explore;
                input_system.set_officer(officer);touch_hud.set_officer(officer);
                input_system.set_usable(use_available);touch_hud.set_usable(use_available);
                const bool can_throw=grenades.count()>0, can_heal=loot_session.can_heal(player_health);
                input_system.set_kit(can_throw,can_heal);touch_hud.set_kit(can_throw,can_heal);
                touch_hud.set_more(input_system.more_open());
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
                !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !mode_session.terminal() && !dev_ui;
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
        if(player_health.downed()) {
            // Down: crawl and look round, nothing else.
            auto& held=input_system.player();
            held.fire=held.aim=held.jump=held.sprint=held.reload=held.grenade=held.heal=held.tag=held.sight=false;
            held.next_weapon=held.fire_mode=held.interact=held.backup=held.inventory=held.toggle_view=false;
            held.crouch=true;
        }
        if(squad_play() && player_health.alive() && !player_health.downed() && !vehicles.driver() && !mode_session.terminal()) {
            // USE beside a downed teammate: kneel and pick them up (USE again stops).
            auto& held=input_system.player();
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const auto near=game::lan::SquadRevive::downed_near(feet,lan_play.remotes(),teammate);
            if(held.interact && (near!=game::lan::SquadRevive::nobody || squad_revive.helping()!=game::lan::SquadRevive::nobody)) {
                held.interact=false;
                if(squad_revive.helping()!=game::lan::SquadRevive::nobody) squad_revive.stop_helping();
                else squad_revive.help(near);
            }
        }
        const input::PlayerInput& controls = input_system.player();
        // On the phone: USE answers it, then each line (the last hangs up).
        if(controls.interact && phone.on_call() && !inventory_open && !map_screen.is_open() && !pause_menu.is_open()) {
            phone.next();input_system.player().interact=false;
        }
        {
            // BUILD (K): lay roads, homes and bus stops in Explore, on foot.
            const bool can_build=game::rules_for(game_mode).civilian_life && !vehicles.driver() && !map_screen.is_open() &&
                !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !mode_session.terminal() && player_health.alive();
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
        // A shop's counter is at its door: within a step or two of it, not from the pavement - and a
        // car within reach takes USE unless you are right at the door.
        if(nearby_shop) {
            const float to_door=std::hypot(nearby_shop->door.x-shop_feet.x,nearby_shop->door.z-shop_feet.z);
            if(to_door>2.5F || (to_door>1.2F && vehicles.nearest(verda_region,shop_feet)>=0)) nearby_shop=nullptr;
        }
        // Vestaĵejo Verda: try on what a mannequin wears, or browse everything at the counter or
        // the fitting rooms (the store sells clothes, not the supplies counter's goods).
        int store_mannequin=-1;
        bool at_store_counter=false;
        store_prompt.clear();
        // STORE ROBBERY: at the job's counter with a gun out, USE takes the till.
        if(career.job().kind==game::career::JobKind::RobStore && !career.job().taken && !vehicles.driver() && player_health.alive() && weapons.available()) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const Vector3 till=career.job().target;
            if(std::hypot(feet.x-till.x,feet.z-till.z)<game::career::Career::till_reach && std::abs(feet.y-till.y)<3) {
                store_prompt=touch_ui ? "USE - Empty the till" : "E - Empty the till";
                use_available=true;
                if(input_system.player().interact && career.robbed()) {
                    input_system.player().interact=false;
                    career.crime(true);
                    interaction_message="\"Take it, take it!\" - the till's emptied. Now get it home";interaction_remaining=4;
                }
            }
        }
        if(clothing_store && game_mode==game::GameMode::Explore && !vehicles.driver() && player_health.alive() && !store_open) {
            if(nearby_shop && nearby_shop->id==clothing_store->place_id) nearby_shop=nullptr;
            const auto close_to=[&](Vector3 at,float reach){
                return std::hypot(at.x-shop_feet.x,at.z-shop_feet.z)<reach && std::abs(at.y-shop_feet.y)<1.6F;};
            float best=1.5F;
            const auto& looks=wardrobe_catalogue.looks();
            for(std::size_t i=0;i<clothing_store->mannequins.size() && i+1<looks.size();++i) {
                const auto& m=clothing_store->mannequins[i];
                const float d=std::hypot(m.at.x-shop_feet.x,m.at.z-shop_feet.z);
                if(d<best && std::abs(m.at.y-shop_feet.y)<1.6F) {best=d;store_mannequin=static_cast<int>(i);}
            }
            at_store_counter=store_mannequin<0 && (close_to(clothing_store->counter,2.2F) || close_to(clothing_store->fitting,2.2F));
            const auto& state=loot_session.economy();
            if(store_mannequin>=0) {
                const auto& look=looks[static_cast<std::size_t>(store_mannequin)+1];
                store_prompt=std::string("E - Try on: ")+look.name+"  ("+(game::wardrobe::owns(state,wardrobe_catalogue,look.id) ? std::string("yours") :
                    look.green_hand && mafio.player_gang()!=0 ? std::string("La Verda Mano only") : std::to_string(look.price)+" kr")+")";
            } else if(at_store_counter) store_prompt="E - Vestaĵejo Verda: every look and hat";
        }
        // The Green Hand's poker table in Verda City: sit down with up to 300 kr.
        const auto* table_crew=game_mode==game::GameMode::Explore ? mafio.casino() : nullptr;
        const bool at_table=table_crew && !vehicles.driver() && !poker_open && player_health.alive() &&
            Vector3Distance(shop_feet,table_crew->table)<2.8F && !mafio.provoked(table_crew->gang,match_time);
        // An officer's arrest: a Krimulo within arm's reach (USE) is cuffed and taken away.
        const auto arrestable=[&](Vector3 feet) -> int {
            if(!career.on_duty() || !mafio.active() || vehicles.driver() || game_mode!=game::GameMode::Explore) return -1;
            int best=-1;float nearest=2.6F;
            for(const auto& m:mafio.members()) {
                const auto* bot=br_bots.find(m.bot);
                if(!bot || !bot->alive() || br_bots.mounted(m.bot) || std::abs(bot->position().y-feet.y)>1.6F) continue;
                if(m.held || callouts.escorting(m.bot) || mafio.heist_of(m.bot)) continue;   // the call's suspects give up first
                const float d=std::hypot(bot->position().x-feet.x,bot->position().z-feet.z);
                if(d<nearest) {nearest=d;best=m.bot;}
            }
            return best;
        };
        if(controls.interact && !creator_active && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !at_table && player_health.alive()) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            if(const int hands_up=career.on_duty() && !vehicles.driver() ? callouts.to_cuff(feet,mafio,br_bots) : -1;hands_up>=0) {
                // A suspect with his hands up: cuffed, he walks with the officer to the car.
                callouts.cuff(hands_up,mafio.gang_of(hands_up),mafio,br_bots);
                interaction_message="CUFFED - he walks with you. Get in your police car with him close by";
                interaction_remaining=4;gun_audio.kill_confirm();
                input_system.player().interact=false;
            } else if(const int bot=arrestable(feet);bot>=0) {
                const int gang=mafio.gang_of(bot),crew=mafio.crew_of(bot);
                const std::string name(game::mafia::gangs[static_cast<std::size_t>(std::max(0,gang))].name);
                // He comes quietly when he's hurt, or when none of his crew is there to see it.
                const auto* him=br_bots.find(bot);
                bool watched=false;
                for(const int other:crew>=0 ? mafio.crews()[static_cast<std::size_t>(crew)].members : std::vector<int>{}) {
                    const auto* o=br_bots.find(other);
                    watched|=other!=bot && o && o->alive() && !br_bots.hidden(other) && Vector3Distance(o->position(),him->position())<15;
                }
                if(watched && him->health()>60) {
                    interaction_message="He won't come quietly - his crew is watching. Get him alone, or wound him first";
                } else if(career.custody()+static_cast<int>(callouts.escorts().size())>=game::career::Career::custody_max) {
                    interaction_message="The car is full - take them to the state prison first";
                } else {
                    // Cuffed: he walks with the officer, and gets in the back of the police car.
                    if(watched) mafio.provoke(bot,match_time);   // cuffed in front of his crew: they take it badly
                    callouts.cuff(bot,gang,mafio,br_bots);
                    territory.killed(gang,feet,player_gang());
                    if(crew>=0 && mafio.crews()[static_cast<std::size_t>(crew)].casino && career.serving() &&
                       std::any_of(mafio.crews().begin(),mafio.crews().end(),[](const game::mafia::Crew& c){return !c.disbanded && !c.casino;}))
                        career.warned_off_capital(match_time);
                    interaction_message="CUFFED: one of "+name+" - walk him to your police car";
                    gun_audio.kill_confirm();
                }
                interaction_remaining=3;
                input_system.player().interact=false;
            }
        }
        // The truckers' brotherhood: the Kamionisto within reach (-1: none), and what he says.
        const auto trucker_near=[&](Vector3 feet) -> int {
            if(vehicles.driver() || game_mode!=game::GameMode::Explore || career_path.empty()) return -1;
            for(std::size_t i=0;i<kamionistoj.size();++i)
                if(std::hypot(kamionistoj[i].at.x-feet.x,kamionistoj[i].at.z-feet.z)<2.6F && std::abs(kamionistoj[i].at.y-feet.y)<1.6F) return static_cast<int>(i);
            return -1;
        };
        const auto gun_man_near=[&](Vector3 feet) -> int {
            if(vehicles.driver() || game_mode!=game::GameMode::Explore || career_path.empty()) return -1;
            for(const auto& g:gun_men)
                if(std::hypot(g.at.x-feet.x,g.at.z-feet.z)<2.4F && std::abs(g.at.y-feet.y)<1.6F) return g.gang;
            return -1;
        };
        const auto society_near=[&](Vector3 feet){
            return society.open && !vehicles.driver() && std::hypot(society.at.x-feet.x,society.at.z-feet.z)<3.4F && std::abs(society.at.y-feet.y)<1.6F;
        };
        const auto trucker_talk=[&](int k) -> std::string {
            const int line=trucker_talks++;
            if(kamionistoj[static_cast<std::size_t>(k)].rep) {
                // Ivo's word (joining, the work and leaving are in his menu).
                const std::string ivo=std::string(brotherhood_man)+": ";
                if(career.on_duty()) return ivo+"\"No badges at La Ringo, officer. The brotherhood looks after its own.\"";
                if(career.trucker()) {
                    static constexpr const char* lines[]{
                        "\"The company pays what it can get away with. Together, we make it pay what a load is worth.\"",
                        "\"Every town on the island has a brother in it. Call on the CB and they come.\"",
                        "\"The Ŝosea Patrolo grows by the month. Fine - we know every back road on Verda.\"",
                        "\"No brother gets sent anywhere he didn't ask to go. Want work? You know where I am.\""};
                    return ivo+lines[line%4];
                }
                return ivo+"\"Verda's trucking is a tough grind - that's why truckers stick together. Esperantistoj, working class, every town on the island, "
                           "one rate for all of us and a higher one every year.\"";
            }
            const std::string who="Kamionisto ("+kamionistoj[static_cast<std::size_t>(k)].town+"): ";
            if(career.trucker()) {
                static constexpr const char* lines[]{
                    "\"Brother! Keep the rubber side down.\"",
                    "\"The company squeezes us, we squeeze back. A strike at the gate and the rate goes up.\"",
                    "\"The patrol's thick on the ring - run the crates on the back roads.\"",
                    "\"Trouble? Get on the CB and we come running.\""};
                return who+lines[line%4];
            }
            static constexpr const char* lines[]{
                "\"Verda's trucking is a tough grind. That's why we stick together.\"",
                "\"Every town, every road - Esperantistoj, working people. Ivo Kovač signs up new brothers at La Ringo - our place on the ring road, the west side.\"",
                "\"One rate for all of us, and we hold out for a higher one. That's the brotherhood.\""};
            return who+lines[line%3];
        };
        // Joining: a gang by talking to its boss (the man by the door of the hideout); the
        // Policio at a station's front door - where a recruit signs up and an officer hands in the
        // badge. USE twice. (A cop never joins a gang: the badge goes back first.)
        const auto boss_near=[&](Vector3 feet) -> int {
            if(!mafio.active() || vehicles.driver() || game_mode!=game::GameMode::Explore || career_path.empty()) return -1;
            for(std::size_t c=0;c<mafio.crews().size();++c) {
                const auto& crew=mafio.crews()[c];
                if(crew.disbanded || crew.members.empty() || br_bots.hidden(crew.members.front())) continue;
                const auto* boss=br_bots.find(crew.members.front());
                // (Mid-conversation he can be a step or two further off.)
                const float reach=match_time<join_offer.until && join_offer.crew==static_cast<int>(c) ? 4.5F : 2.6F;
                if(boss && boss->alive() && std::hypot(boss->position().x-feet.x,boss->position().z-feet.z)<reach &&
                   std::abs(boss->position().y-feet.y)<1.6F) return static_cast<int>(c);
            }
            return -1;
        };
        // A foot soldier of the player's own gang within reach: he can join the player's crew.
        const auto recruit_near=[&](Vector3 feet) -> int {
            if(!mafio.active() || vehicles.driver() || game_mode!=game::GameMode::Explore || career_path.empty() ||
               career.state().side!=game::career::Side::Gang) return -1;
            int best=-1;float nearest=3.0F;
            for(const auto& m:mafio.members()) {
                if(player_crew.member(m.bot) || !mafio.recruitable(m.bot,br_bots)) continue;
                const auto* bot=br_bots.find(m.bot);
                const float d=std::hypot(bot->position().x-feet.x,bot->position().z-feet.z);
                if(d<nearest && std::abs(bot->position().y-feet.y)<2.2F) {nearest=d;best=m.bot;}
            }
            return best;
        };
        const auto station_desk=[&](Vector3 feet) -> const game::services::Unit* {
            if(!services.active() || vehicles.driver() || game_mode!=game::GameMode::Explore || career_path.empty()) return nullptr;
            for(const auto& place:island_life.places())
                // At the front door, or anywhere inside up to the counter.
                if(place.kind==game::life::PlaceKind::Police && std::abs(place.door.y-feet.y)<2.0F &&
                   (std::hypot(place.door.x-feet.x,place.door.z-feet.z)<4.5F || std::hypot(place.interior.x-feet.x,place.interior.z-feet.z)<5.0F))
                    return services.station_near(place.door,40);
            return nullptr;
        };
        if(controls.interact && !creator_active && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !at_table && !vehicles.driver() &&
           game_mode==game::GameMode::Explore && !career_path.empty() && player_health.alive()) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const int crew=boss_near(feet);
            const auto* station=crew<0 ? station_desk(feet) : nullptr;
            const bool confirming=match_time<join_offer.until && ((crew>=0 && join_offer.crew==crew) || (station && join_offer.patrol_car==station->vehicle));
            const int recruit=crew<0 && !station ? recruit_near(feet) : -1;
            if(recruit>=0) {
                if(player_crew.recruit(recruit)) {
                    mafio.lend(recruit,true,br_bots,feet);
                    interaction_message=TextFormat("He's with you - your crew: %d of %d",static_cast<int>(player_crew.members().size()),game::career::Crew::max_gang);
                } else interaction_message=TextFormat("Your crew is full (%d)",game::career::Crew::max_gang);
                interaction_remaining=3;input_system.player().interact=false;
            } else if(station && career.on_duty() && !confirming) {
                // The desk: a partner, the armory, the badge.
                gear_open_desk=true;
                input_system.player().interact=false;
            } else if(crew>=0 && career.on_duty()) {
                interaction_message="The boss won't talk to a cop - hand in your badge at a station first";
                interaction_remaining=3;input_system.player().interact=false;
            } else if(crew>=0 && career.in_gang(mafio.crews()[static_cast<std::size_t>(crew)].gang)) {
                // Your own boss: a word, or (behind a "yes") walking away.
                talk_with=TalkWith::OwnBoss;talk_crew=crew;talk_open=true;
                input_system.player().interact=false;
            } else if(crew>=0 && !mafio.provoked(mafio.crews()[static_cast<std::size_t>(crew)].gang,match_time)) {
                talk_with=TalkWith::GangBoss;talk_crew=crew;talk_open=true;
                input_system.player().interact=false;
            } else if(station) {
                talk_with=TalkWith::Station;talk_car=station->vehicle;talk_open=true;
                input_system.player().interact=false;
            }
        }
        if(controls.interact && at_table && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !mode_session.terminal()) {
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
        if(controls.interact && (store_mannequin>=0 || at_store_counter) && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !at_table && !mode_session.terminal()) {
            const auto& state=loot_session.economy();
            int look=store_mannequin>=0 ? store_mannequin+1 : 0;
            if(store_mannequin<0) if(const auto* on=game::wardrobe::wearing(state,wardrobe_catalogue))
                for(std::size_t i=0;i<wardrobe_catalogue.looks().size();++i) if(wardrobe_catalogue.looks()[i].id==on->id) look=static_cast<int>(i);
            int hat=-1;
            for(std::size_t i=0;i<wardrobe_catalogue.hats().size();++i) if(wardrobe_catalogue.hats()[i].id==state.hat) hat=static_cast<int>(i);
            store_ui.set_font(sign_renderer.font());
            store_ui.open(look,hat);
            store_open=true;store_status.clear();player.third_person=true;
            input_system.cancel_controls();
        }
        else if(controls.interact && nearby_shop && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !at_table && !mode_session.terminal()) {
            current_shop=nearby_shop;shop_open=true;shop_status="Select supplies, equipment or furniture. BUY / SELL transfers one item.";
            play_ui.opened();input_system.cancel_controls();
        }
        // The outbreak: on with whoever is talking, or a word with whoever is near.
        if(controls.interact && outbreak.active() && !creator_active && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) &&
           !vehicles.driver() && player_health.alive() && outbreak.use({player.position.x,player.position.y-1,player.position.z}))
            input_system.player().interact=false;
        // The Chief (talk), and the training camp's stand (start the next drill).
        if(controls.interact && !creator_active && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !at_table && !vehicles.driver() &&
           game_mode==game::GameMode::Explore && !career_path.empty() && player_health.alive()) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const auto ron_truck_near=[&]{
                const int v=campaign.ron_truck.empty() ? -1 : vehicles.find(campaign.ron_truck);
                return v>=0 && Vector3Distance(vehicles.vehicles()[static_cast<std::size_t>(v)].position,feet)<5.5F;
            };
            if(!campaign_path.empty() && story.talking() && (story.stage()!=game::campaign::Stage::Free || story.saying())) {
                story.talk();input_system.player().interact=false;   // the next line
            } else if(!campaign_path.empty() && story.stage()==game::campaign::Stage::ToTruck &&
                      (ron_truck_near() || (uncle_ron.placed && Vector3Distance(feet,uncle_ron.at)<3.5F))) {
                story.board();input_system.player().interact=false;  // off home with Uncle Ron
            } else if(!campaign_path.empty() && uncle_ron.placed && Vector3Distance(feet,uncle_ron.at)<3.2F && story.stage()!=game::campaign::Stage::Ride) {
                // He has something to say about the road the player took; otherwise a word.
                if(!ron_pending.empty() && story.say(ron_pending,ron_pending_home)) {ron_pending.clear();ron_pending_home=false;}
                else story.talk();
                input_system.player().interact=false;
            } else if(chief.placed && Vector3Distance(feet,chief.at)<2.4F) {
                bool issue=false;
                chief_line=career.chief_talk(issue);chief_line_time=10;
                if(issue) {
                    // The Interceptor, out front.
                    std::vector<std::string> before;for(const auto& v:vehicles.vehicles()) before.push_back(v.id);
                    if(vehicles.spawn(verda_region,chief.at,chief.yaw,"interceptor"))
                        for(const auto& v:vehicles.vehicles()) if(std::find(before.begin(),before.end(),v.id)==before.end()) {
                            career.issue_car(v.id);services.assign_player_car(v.id);vehicle_dirty=true;break;
                        }
                    gun_audio.kill_confirm();
                }
                input_system.player().interact=false;
            } else if(depot_boss.placed && Vector3Distance(feet,depot_boss.at)<2.6F) {
                chief_line=trucking.talk(career.on_duty(),match_time,haul_world);chief_line_time=9;
                input_system.player().interact=false;
            } else if(const int k=trucker_near(feet);k>=0) {
                if(kamionistoj[static_cast<std::size_t>(k)].rep && !career.on_duty()) {
                    talk_with=career.trucker() ? TalkWith::Ivo : TalkWith::IvoJoin;talk_open=true;
                } else {chief_line=trucker_talk(k);chief_line_time=9;}
                input_system.player().interact=false;
            } else if(const int g=gun_man_near(feet);g>=0) {
                if(career.in_gang(g)) {gear_gang=g;gear_open_gun_man=true;}
                else {chief_line="Gun man ("+gang_name(g)+"): \"I sell to family. You're not family.\"";chief_line_time=6;}
                input_system.player().interact=false;
            } else if(society_near(feet)) {
                static constexpr const char* lines[]{
                    "La Verda Mano Kultura Societo: \"No more guns. We keep the language, the songs and the flag - Verda is one people, and we'll show it the right way now.\"",
                    "La Verda Mano Kultura Societo: \"Esperanto classes on Thursdays, the old songs after. Bring your neighbours.\"",
                    "La Verda Mano Kultura Societo: \"We said only the Green Hand could unite the people. Turns out it takes a pen, not a pistol.\""};
                chief_line=lines[society_talks++%3];chief_line_time=9;
                input_system.player().interact=false;
            } else if(police_camp.placed() && police_camp.at_line(feet) &&
                      police_camp.phase()!=game::career::CampPhase::Countdown && police_camp.phase()!=game::career::CampPhase::Running) {
                if(!career.on_duty()) {interaction_message="The range is for Policio recruits - sign up at any police station";interaction_remaining=4;}
                else if(career.police_stage()==game::career::PoliceStage::Report) {interaction_message=std::string("Report to ")+game::career::chief_name+" at Verda City HQ first";interaction_remaining=4;}
                else police_camp.start(police_camp.next_drill());
                input_system.player().interact=false;
            }
        }
        // Front doors open by themselves as anyone walks up (the player, residents, bots) and swing
        // shut behind them; shut, a door stops the player (WorldCollision::resolve_player_body).
        {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const float step=dt*4.0F;
            for(auto& town:verda_region.runtime_settlements()) for(auto& building:town.buildings) {
                if(!building.enterable) continue;
                const Vector3 door=world::physics::WorldCollision::door_point(building);
                const float player_distance=std::hypot(door.x-feet.x,door.z-feet.z);
                if(player_distance>60) {building.door=0;continue;}
                bool open=player_distance<2.6F && !vehicles.driver();
                for(const auto& actor:npcs.actors()) {
                    if(open) break;
                    if(actor.state!=characters::NpcState::Dead && std::hypot(actor.position.x-door.x,actor.position.z-door.z)<1.8F) open=true;
                }
                building.door=open ? std::min(1.0F,building.door+step) : std::max(0.0F,building.door-step*.6F);
            }
        }
        // The island's life in Explore: word getting around, Sundays and feasts, the street races,
        // and the family on the phone.
        if(game_mode==game::GameMode::Explore && game::rules_for(game_mode).civilian_life && !island_life.empty() && !outbreak.active()) {
#ifdef OUTLAND_DEV_TOOLS
            // OUTLAND_DEV_CLOCK=day,hour: the island's clock, once (day 6 is a Sunday; feasts every 4th day).
            static bool clock_set=false;
            if(const char* when=std::getenv("OUTLAND_DEV_CLOCK"); when && *when && !clock_set) {
                clock_set=true;
                int d=0,h=12;
                if(std::sscanf(when,"%d,%d",&d,&h)==2) island_life.set_clock(game::life::WorldClock::at(d,h,0));
            }
#endif
            const auto clock=island_life.clock();
            const int hour=static_cast<int>(clock.hour()),day=clock.day(),minute=clock.minute_of_day();
            const bool sunday=clock.weekday()==game::life::Weekday::Sunday,saturday=clock.weekday()==game::life::Weekday::Saturday;
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const auto* mine=vehicles.driver();
            const Vector3 here=mine ? mine->position : feet;
            const bool held=creator_active||inventory_open||shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()||poker_open||pause_menu.is_open();
            // The nearest town to a point (its id): where a story happened.
            const auto town_of=[&](Vector3 p) {
                std::string id;float best=1e30F;
                for(const auto& s:verda_region.settlements()) {
                    if(s.id.starts_with("ranch_")) continue;
                    if(const float d=std::hypot(s.center.x-p.x,s.center.z-p.z);d<best) {best=d;id=s.id;}
                }
                return id;
            };
            // The bells and the phone's ring, made once (a struck bell's partials; a phone's two tones).
            if(!chimes_ready && IsAudioDeviceReady()) {
                chimes_ready=true;
                const auto make=[](int rate,float seconds,auto sample) {
                    const int frames=static_cast<int>(static_cast<float>(rate)*seconds);
                    auto* data=static_cast<short*>(MemAlloc(static_cast<unsigned>(frames)*sizeof(short)));
                    for(int i=0;i<frames;++i) data[i]=static_cast<short>(std::clamp(sample(static_cast<float>(i)/static_cast<float>(rate)),-1.0F,1.0F)*26000);
                    Wave w{static_cast<unsigned>(frames),static_cast<unsigned>(rate),16,1,data};
                    Sound s=LoadSoundFromWave(w);
                    UnloadWave(w);
                    return s;
                };
                bell_sound=make(22050,3.2F,[](float t) {
                    constexpr float f=220;
                    constexpr float partials[][3]{{.5F,.6F,.9F},{1,1,1.4F},{1.19F,.5F,1.8F},{1.5F,.45F,2.4F},{2,.35F,3},{2.74F,.2F,4.2F}};
                    float v=0;
                    for(const auto& p:partials) v+=p[1]*std::sin(2*PI*f*p[0]*t)*std::exp(-t*p[2]);
                    return v*.32F*std::min(1.0F,t*400);
                });
                ring_sound=make(22050,2.2F,[](float t) {
                    const float burst=std::fmod(t,1.1F)<.42F && std::fmod(t*25,1.0F)<.5F ? 1.0F : 0.0F;
                    return burst*.22F*(std::sin(2*PI*440*t)+std::sin(2*PI*480*t));
                });
            }
            // Word gets around: every headline is heard by the island.
            if(gang_news!=gossip_heard) {gossip_heard=gang_news;if(!gang_news.empty()) gossip.hear(gang_news,clock.minutes,town_of(here));}
            if(!quiet_news.empty()) {gossip.hear(quiet_news,clock.minutes,town_of(here));quiet_news.clear();}
            // A feast day: the morning's word, and the gangs keep the peace.
            if(day!=feast_said_day && minute>=7*60) {
                feast_said_day=day;
                if(const auto said=game::culture::feast_morning(day);!said.empty()) {gang_news=said;gang_news_time=9;gossip_heard=gang_news;}
            }
            if(game::culture::day_of_peace(day) && mafio.active()) mafio.set_next_war(std::max(mafio.next_war(),match_time+60));
            // The bells of the nearest church.
            int near_church=-1;float church_distance=1e9F;
            for(std::size_t i=0;i<churches.size();++i)
                if(const float d=std::hypot(churches[i].door.x-here.x,churches[i].door.z-here.z);d<church_distance) {church_distance=d;near_church=static_cast<int>(i);}
            if(near_church>=0 && church_distance<520 && game::culture::bells(sunday,day,minute) && bell_sound.stream.buffer && !environment_audio.muted()) {
                if((bell_clock-=dt)<=0) {
                    bell_clock=churches[static_cast<std::size_t>(near_church)].denomination==game::culture::Denomination::Orthodox ? 1.3F : 2.4F;
                    const float near=1-church_distance/520;
                    SetSoundVolume(bell_sound,near*near*environment_audio.volume());
                    SetSoundPitch(bell_sound,churches[static_cast<std::size_t>(near_church)].denomination==game::culture::Denomination::Orthodox ? .8F+.2F*static_cast<float>(GetRandomValue(0,3)) : 1.0F);
                    PlaySound(bell_sound);
                }
            } else bell_clock=0;
            // The faithful outside the nearest church with a gathering (Sunday, a feast, Pasko night).
            {
                int want=-1;game::culture::Gathering gather;
                if(near_church>=0 && church_distance<170) {
                    gather=game::culture::gathering(churches[static_cast<std::size_t>(near_church)],sunday,day,minute);
                    if(gather.people>0) want=near_church;
                }
                if(want!=crowd_church || (want>=0 && gather.people!=crowd_size)) {
                    for(int k=0;k<crowd_size;++k) npcs.despawn_bot(crowd_bot+k);
                    crowd_church=want;crowd_size=0;crowd_green=0;crowd_spots.clear();
                    if(want>=0) {
                        const auto& church=churches[static_cast<std::size_t>(want)];
                        Vector3 out=Vector3Subtract(church.door,church.centre);out.y=0;
                        out=Vector3Length(out)>.1F ? Vector3Normalize(out) : Vector3{0,0,-1};
                        const Vector3 side{-out.z,0,out.x};
                        const float face=std::atan2(-out.x,-out.z)*RAD2DEG;   // toward the church
                        for(int k=0;k<gather.people;++k) {
                            const int row=k/6,col=k%6;
                            const auto r=static_cast<std::uint32_t>(want*131+k*977);
                            const float across=(static_cast<float>(col)-2.5F)*1.5F+static_cast<float>(r%7)*.08F;
                            const Vector3 at=Vector3Add(church.door,Vector3Add(Vector3Scale(out,3.0F+static_cast<float>(row)*1.7F),Vector3Scale(side,across)));
                            const float top=at.y+3;
                            const Vector3 ground{at.x,world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region),at.z};
                            const bool green=k<gather.green_hand;
                            const auto* who=green ? nullptr : character_registry.choose(characters::CharacterPool::Civilian,r);
                            const std::string body=green || !who ? std::string(outfit_body) : who->id;
                            if(!character_registry.find(body)) continue;
                            npcs.spawn_bot(crowd_bot+k,body,ground,face+static_cast<float>(r%21)-10);
                            crowd_spots.push_back(ground);
                            crowd_size=k+1;
                        }
                        crowd_green=gather.green_hand;
                    }
                }
                crowd_candles=gather.candles;
            }
            // The street races: the boys' cars come out at night by the meet, and go home by day.
            if(street_race.ready()) {
                const float to_meet=std::hypot(street_race.meet().x-here.x,street_race.meet().z-here.z);
                if(!race_cars_out && game::racing::StreetRace::night(hour) && to_meet<700) {
                    struct Entry {const char* kind;const char* name;float pace;game::autos::Mods mods;};
                    const Entry entries[]{{"muscle","Mira 'Ŝtormo' Kovač",40,{1,2,0,0,0,0,0,2,2,1,3,1}},
                                          {"sedan","Tomas 'Pneŭo' Varga",37.5F,{8,1,0,0,0,0,0,1,1,2,2,3}},
                                          {"hatchback","Niko 'Pulvo' Petrov",35,{5,1,0,0,0,0,0,2,1,1,1,2}}};
                    std::vector<game::racing::Racer> field;
                    for(std::size_t i=0;i<std::size(entries);++i) {
                        world::roads::Route verge=street_race.track();
                        verge.lane.assign(verge.lane.size(),world::highways::paved_half+3.2F);
                        float yaw=0;
                        const Vector3 spot=world::roads::route_point(verge,22.0F+9.0F*static_cast<float>(i),&yaw);
                        if(!vehicles.spawn(verda_region,spot,yaw,entries[i].kind)) continue;
                        game::racing::Racer r;
                        r.vehicle=vehicles.last_spawned();r.name=entries[i].name;r.pace=entries[i].pace;r.parked=spot;r.parked_yaw=yaw;
                        if(auto* a=vehicles.asset(verda_region,r.vehicle)) game::autos::apply(entries[i].mods,a->vehicle);
                        vehicles.place(verda_region,r.vehicle,spot,yaw);
                        bot_drivers.reserve(r.vehicle);
                        field.push_back(r);
                    }
                    street_race.set_racers(field);
                    race_cars_out=!field.empty();
                } else if(race_cars_out && !game::racing::StreetRace::night(hour) && street_race.stage()==game::racing::Stage::Closed && to_meet>400) {
                    for(const auto& r:street_race.racers()) vehicles.remove(verda_region,r.vehicle);
                    street_race.set_racers({});
                    race_cars_out=false;
                }
                // A racer's car taken by the player is out of the field.
                if(mine) {
                    auto& field=street_race.racers();
                    std::erase_if(field,[&](const game::racing::Racer& r){return r.vehicle==mine->id;});
                }
#ifdef OUTLAND_DEV_TOOLS
                // OUTLAND_DEV_RACE=1: at the wheel near the meet at night, the bet is on (spawn on the grid to race at once).
                static const bool dev_race=[]{const char* v=std::getenv("OUTLAND_DEV_RACE");return v && *v=='1';}();
                static bool dev_raced=false;
                if(dev_race && !dev_raced && mine && race_cars_out && street_race.stage()==game::racing::Stage::Open) {
                    dev_raced=true;
                    bool paid=false;
                    street_race.talk(1000000,paid);street_race.talk(1000000,paid);
                    const Vector3 slot=street_race.grid(0);
                    TraceLog(LOG_INFO,"OUTLAND dev race: the bet is on; the grid is at %.1f %.1f yaw %.0f",slot.x,slot.z,street_race.start_yaw());
                }
#endif
                game::racing::Frame rf;
                rf.dt=held ? 0.0F : dt;rf.hour=hour;rf.driving=mine!=nullptr;rf.car=here;rf.speed=mine ? mine->speed : 0;
                rf.money=loot_session.economy().money;
                street_race.update(rf,vehicles,verda_region);
                const auto car_words=[&]{
                    const auto* a=mine ? vehicles.asset(verda_region,mine->id) : nullptr;
                    const auto* d=a ? vehicles.definition(*a) : nullptr;
                    if(!d) return std::string("a car");
                    const int paint=a->vehicle.painted ? a->vehicle.mods[0] : -1;
                    return game::culture::Gossip::car(paint>=0 && paint<static_cast<int>(game::autos::paints.size()) ? game::autos::paints[static_cast<std::size_t>(paint)].name : "",d->name);
                };
                switch(street_race.take_result()) {
                    case game::racing::Result::Won:
                        loot_session.economy().money+=street_race.pot();loot_session.changed();
                        ++campaign.races;++campaign.race_wins;stats.race_won();
                        gang_news="STREET RACE: first past the last flare - Fulmo's pot is yours, "+std::to_string(street_race.pot())+" kr";gang_news_time=8;gossip_heard=gang_news;
                        gossip.hear("somebody in "+car_words()+" beat Fulmo's boys on the ring road",clock.minutes,town_of(here),true);
                        gun_audio.kill_confirm();haptics.hit_given(true);
                        break;
                    case game::racing::Result::Lost:
                        ++campaign.races;
                        gang_news="STREET RACE: "+street_race.winner()+" takes the pot";gang_news_time=7;gossip_heard=gang_news;
                        gossip.hear(street_race.winner()+" won on the ring road again - left "+car_words()+" in the dust",clock.minutes,town_of(here),true);
                        break;
                    case game::racing::Result::Forfeit:
                        interaction_message="You left the race - the stake stays in the pot";interaction_remaining=4;
                        break;
                    default: break;
                }
                // Fulmo and the drivers by their cars while the meet is on.
                const bool meet_on=race_cars_out && to_meet<220 && (street_race.stage()==game::racing::Stage::Open || street_race.stage()==game::racing::Stage::Offered);
                if(meet_on && !race_bots_out) {
                    race_bots_out=true;
                    const auto stand=[&](int id,Vector3 at,float yaw,std::uint32_t seed) {
                        if(const auto* who=character_registry.choose(characters::CharacterPool::Civilian,seed)) {
                            const float top=at.y+3;
                            npcs.spawn_bot(id,who->id,{at.x,world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region),at.z},yaw);
                        }
                    };
                    stand(race_bot,street_race.organiser(),std::atan2(street_race.meet().x-street_race.organiser().x,street_race.meet().z-street_race.organiser().z)*RAD2DEG,4421);
                    for(std::size_t i=0;i<street_race.racers().size();++i) {
                        const auto& r=street_race.racers()[i];
                        stand(race_bot+1+static_cast<int>(i),Vector3Add(r.parked,{std::cos(r.parked_yaw*DEG2RAD)*2.2F,0,-std::sin(r.parked_yaw*DEG2RAD)*2.2F}),r.parked_yaw+90,5101+static_cast<std::uint32_t>(i)*17);
                    }
                } else if(!meet_on && race_bots_out) {
                    race_bots_out=false;
                    for(int k=0;k<4;++k) npcs.despawn_bot(race_bot+k);
                }
            }
            // Uncle Ron and Uncle Dario ring now and then (the campaign, in free play).
            if(!campaign_path.empty() && story.stage()==game::campaign::Stage::Free) {
                game::campaign::PhoneContext pc;
                pc.played=campaign.played;pc.hour=hour;pc.saturday=saturday;pc.sunday=sunday;
                pc.cop=career.on_duty();
                pc.gang=career.state().side==game::career::Side::Gang;
                pc.green_hand=player_gang()==0;
                pc.owns_car=!owned_cars.empty();
                pc.races=campaign.races;pc.race_wins=campaign.race_wins;
                for(const auto& r:gossip.rumours())
                    if(r.about_you && clock.minutes-r.at<24*60) pc.rumour="People are saying "+r.text+".";
                pc.feast_tomorrow=game::culture::feast_on(day+1);
                const auto stage=street_race.stage();
                pc.busy=stage==game::racing::Stage::Lining || stage==game::racing::Stage::Countdown || stage==game::racing::Stage::Racing ||
                        story.talking() || inventory_open || shop_open || store_open || motor_menu.is_open() || gear_menu.is_open() || poker_open || map_screen.is_open() ||
                        (chief_line_time>0 && !phone.on_call()) || !player_health.alive();
                if(street_race.ready()) pc.race_meet=street_race.meet();
                for(const auto& c:churches) if(c.settlement=="capital_verda") {
                    if(c.denomination==game::culture::Denomination::Orthodox) pc.orthodox_church=c.door;
                    else pc.chapel=c.door;
                }
                if(uncle_ron.placed) pc.ron_home=uncle_ron.at;
                if(const auto* home=mafio.active() ? mafio.casino() : nullptr) pc.hideout=home->door;
                phone.update(held ? 0.0F : dt,pc);
                if(phone.take_ring() && ring_sound.stream.buffer && !environment_audio.muted()) {SetSoundVolume(ring_sound,.8F*environment_audio.volume());PlaySound(ring_sound);haptics.hit_taken(6);}
                if(phone.ringing()) {chief_line=phone.call()->caller+" is calling... (USE to answer)";chief_line_time=1.2F;}
                else if(const auto said=phone.line();!said.empty()) {chief_line=said;chief_line_time=1.2F;}
                if(auto ended=phone.take_ended()) {
                    campaign.calls=phone.done();
                    if(ended->target) {
                        map_screen.set_pin(*ended->target);
                        interaction_message="Waypoint: "+ended->target_name+" (on your map)";interaction_remaining=5;
                    }
                }
            }
        }
        // Walking up to a church: who worships there, its saint, the next feast.
        if(game::rules_for(game_mode).civilian_life && !vehicles.driver() && !churches.empty()) {
            int near=-1;float best=8;
            for(std::size_t i=0;i<churches.size();++i) {
                const float d=std::hypot(player.position.x-churches[i].door.x,player.position.z-churches[i].door.z);
                if(d<best) {best=d;near=static_cast<int>(i);}
            }
            if(near>=0 && near!=church_seen) {
                church_seen=near;
                interaction_message=game::culture::church_notice(churches[static_cast<std::size_t>(near)],island_life.empty() ? 0 : island_life.clock().day());
                interaction_remaining=9;
            } else if(church_seen>=0) {
                const auto& c=churches[static_cast<std::size_t>(church_seen)];
                if(std::hypot(player.position.x-c.door.x,player.position.z-c.door.z)>30) church_seen=-1;
            }
        }
        {
            // Anything in reach of USE: shows the touch USE button next frame.
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            bool here=store_mannequin>=0 || at_store_counter || nearby_shop || at_table ||
                vehicles.nearest(verda_region,feet)>=0 || loot_session.world().nearest(feet,2.8F)>=0 ||
                (game::rules_for(game_mode).civilian_life && penthouse.inside(feet)) || home_here;
            if(outbreak.active() && !vehicles.driver()) if(const auto said=outbreak.prompt(feet);!said.empty()) {
                here=true;if(store_prompt.empty()) store_prompt=touch_ui && said.rfind("E - ",0)==0 ? "USE - "+said.substr(4) : said;
            }
            const bool at_stash=stash && game::rules_for(game_mode).civilian_life && !vehicles.driver() && !game::economy::Stash::taken(loot_session.economy()) &&
                std::hypot(feet.x-stash->at.x,feet.z-stash->at.z)<1.8F && std::abs(feet.y-stash->at.y)<1.5F;
            if(at_stash) {here=true;if(store_prompt.empty()) store_prompt=touch_ui ? "USE - Take the holdall" : "E - Take the holdall";}
            if(game::rules_for(game_mode).civilian_life && !vehicles.driver() && !here) {
                if(street_race.ready() && race_bots_out && std::hypot(feet.x-street_race.organiser().x,feet.z-street_race.organiser().z)<game::racing::StreetRace::talk_range) {
                    here=true;if(store_prompt.empty()) store_prompt=touch_ui ? "USE - Talk to Fulmo" : "E - Talk to Fulmo";
                } else if(!island_life.empty() && island_life.nearest(feet,2.6F)) {
                    here=true;if(store_prompt.empty()) store_prompt=touch_ui ? "USE - Talk" : "E - Talk";
                }
            }
            if(!here && game::rules_for(game_mode).civilian_life && !vehicles.driver()) {Vector3 s{};float y=0;here=stand_seat.on || stand_seat_near(feet,s,y);}
            if(!here && game_mode==game::GameMode::Explore && !career_path.empty()) {
                here=(chief.placed && Vector3Distance(feet,chief.at)<2.4F) || (police_camp.placed() && police_camp.at_line(feet)) || arrestable(feet)>=0 || (career.on_duty() && callouts.to_cuff(feet,mafio,br_bots)>=0) ||
                    (depot_boss.placed && Vector3Distance(feet,depot_boss.at)<2.6F) || trucker_near(feet)>=0 || society_near(feet) || gun_man_near(feet)>=0 ||
                    (!campaign_path.empty() && (story.talking() || (uncle_ron.placed && Vector3Distance(feet,uncle_ron.at)<3.2F))) ||
                    boss_near(feet)>=0 || station_desk(feet)!=nullptr || recruit_near(feet)>=0 ||
                    booking.stage==2 || inside_line<inside_lines.size() ||
                    (prison_until>=0 && (Vector3Distance(feet,prison_bunk(in_shu))<2.2F || prison_yard.snitch()>=0));
                // Who you'd be talking to.
                if(store_prompt.empty() && player_health.alive()) {
                    if(!campaign_path.empty() && story.talking() && story.stage()!=game::campaign::Stage::Free) store_prompt="E - Continue";
                    else if(!campaign_path.empty() && story.stage()==game::campaign::Stage::ToTruck && uncle_ron.placed && Vector3Distance(feet,uncle_ron.at)<3.5F)
                        store_prompt="E - Ride home with Uncle Ron";
                    else if(!campaign_path.empty() && uncle_ron.placed && Vector3Distance(feet,uncle_ron.at)<3.2F) store_prompt="E - Talk to Uncle Ron";
                    else if(career.on_duty() && !vehicles.driver() && callouts.to_cuff(feet,mafio,br_bots)>=0) store_prompt="E - Cuff him (hands up)";
                    else if(arrestable(feet)>=0) store_prompt="E - Arrest him";
                    else if(depot_boss.placed && Vector3Distance(feet,depot_boss.at)<2.6F)
                        store_prompt=!trucking.hired() ? "E - Ask the boss of Verda Transporto for work" :
                            trucking.job().stage==game::freight::HaulStage::Report ? "E - Report in for your pay" : "E - Talk to the boss";
                    else if(const int k=trucker_near(feet);k>=0)
                        store_prompt=kamionistoj[static_cast<std::size_t>(k)].rep ? std::string("E - Talk to ")+brotherhood_man+" (the truckers' brotherhood)" : "E - Talk to the trucker";
                    else if(society_near(feet)) store_prompt="E - Talk to La Verda Mano Kultura Societo";
                    else if(const int g=gun_man_near(feet);g>=0) store_prompt=career.in_gang(g) ? "E - The gun man: guns for the family" : "E - Talk to the gun man";
                    else if(recruit_near(feet)>=0)
                        store_prompt=static_cast<int>(player_crew.members().size())<game::career::Crew::max_gang ?
                            TextFormat("E - Recruit him (crew %d of %d)",static_cast<int>(player_crew.members().size())+1,game::career::Crew::max_gang) : "Your crew is full";
                    else if(const int crew=boss_near(feet);crew>=0) {
                        const std::string name(game::mafia::gangs[static_cast<std::size_t>(mafio.crews()[static_cast<std::size_t>(crew)].gang)].name);
                        store_prompt=career.in_gang(mafio.crews()[static_cast<std::size_t>(crew)].gang) ? "E - Talk to your boss" : "E - Talk to the boss of "+name;
                    } else if(station_desk(feet)) store_prompt=!career.on_duty() ? "E - Sign up with the Policio de Verda" :
                        "E - The desk: partner, armory, badge";
                }
            }
            if(squad_play() && player_health.alive() && !player_health.downed() && !vehicles.driver()) {
                const auto near=game::lan::SquadRevive::downed_near(feet,lan_play.remotes(),teammate);
                if(squad_revive.helping()!=game::lan::SquadRevive::nobody) {here=true;store_prompt=touch_ui ? "USE - Stop" : "E - Stop";}
                else if(near!=game::lan::SquadRevive::nobody) {
                    here=true;
                    if(const auto* who=lan_play.find(near)) store_prompt=std::string(touch_ui ? "USE - Pick up " : "E - Pick up ")+who->name;
                }
            }
            use_available=here && !creator_active && player_health.alive() && !player_health.downed();
        }
        // ====================================================
        // THE POLICIO'S DESK AND ARMORY, AND THE GANGS' GUN MEN (game::career::Armory)
        // ====================================================
        {
            using game::autos::MenuRow;
            namespace armory=game::career;
            static constexpr const char* police_ranks[]{"Kadeto","Patrolano","Serĝento","Leŭtenanto","Komisaro"};
            static constexpr const char* gang_ranks[]{"Novulo","Soldato","Kapo","Leŭtenanto","Estro"};
            const auto open_gear=[&](GearScreen screen,int selected) {
                std::vector<MenuRow> rows;
                std::string title,subtitle,back="BACK";
                const auto& bag=loot_session.inventory();
                switch(screen) {
                case GearScreen::Desk:
                    title="POLICIO DE VERDA";subtitle="The desk";back="LEAVE";
                    rows.push_back({"Partner",player_crew.partner()>=0 ? "Officer Novak rides with you" : "ask for one to ride with you","",player_crew.partner()<0});
                    rows.push_back({"Armory","free kit - the heavier guns come with rank",""});
                    rows.push_back({"Hand in your badge","leave the Policio de Verda",""});
                    break;
                case GearScreen::Armory:
                    title="ARMILEJO DE LA POLICIO";subtitle=std::string("Free - ")+career.rank_name()+"'s issue";
                    for(std::size_t i=0;i<armory::armory.size();++i) {
                        const auto& a=armory::armory[i];
                        const bool may=armory::police_may_draw(career.rank(),i);
                        const bool owned=bag.count(a.item)>0;
                        rows.push_back({a.name,may ? (owned && a.ammo[0] ? "in your bag - more rounds" : owned ? "in your bag" : "") : std::string("for a ")+police_ranks[a.police_rank],
                                        may ? "Free" : "Rank",may,owned});
                    }
                    break;
                case GearScreen::Talk: {
                    back="LEAVE";
                    const auto gang_of=[&]{return talk_crew>=0 && talk_crew<static_cast<int>(mafio.crews().size()) ? mafio.crews()[static_cast<std::size_t>(talk_crew)].gang : 0;};
                    const bool dario=talk_crew>=0 && !campaign_path.empty() && mafio.casino()==&mafio.crews()[static_cast<std::size_t>(talk_crew)] && gang_of()==0;
                    switch(talk_with) {
                    case TalkWith::OwnBoss:
                        title=gang_name(gang_of());subtitle=dario ? "Uncle Dario" : "The boss";
                        rows.push_back({"Talk","hear what he has to say",""});
                        rows.push_back({"Leave the gang","walk away - your crew goes home (you'll be asked to be sure)",""});
                        break;
                    case TalkWith::GangBoss:
                        title=gang_name(gang_of());subtitle=dario ? "Uncle Dario - family first" : "The boss looks you over";
                        rows.push_back({"Join "+gang_name(gang_of()),"run with the crew: the boss's jobs, respect, a cut - and the gang's enemies",""});
                        rows.push_back({"Talk","hear what he has to say",""});
                        break;
                    case TalkWith::Ivo: {
                        std::string why;
                        const bool raise=career.haul_rate()<1+game::career::Career::union_rate+game::career::Career::raise_max-1e-3F;
                        const bool free=career.job().kind==game::career::JobKind::None;
                        title="LA FRATECO DE KAMIONISTOJ";subtitle=std::string(brotherhood_man)+" - the brotherhood's work is yours to ask for";
                        rows.push_back({"Talk","shop talk",""});
                        rows.push_back({"Call a strike",!raise ? "the company has given all it will - for now" : !free ? "finish the job you've got first" :
                                        "a rig in the depot's gate for a minute: +5% on every load for good","500 kr",raise && free});
                        rows.push_back({"A contraband run",!free ? "finish the job you've got first" : "crates from a smugglers' jetty to a freight yard - the patrol will hear","2200 kr",free});
                        rows.push_back({"Leave the brotherhood","go it alone (you'll be asked to be sure)",""});
                        break;
                    }
                    case TalkWith::IvoJoin:
                        title="LA FRATECO DE KAMIONISTOJ";subtitle=std::string(brotherhood_man)+" - Verda's truckers stick together";
                        rows.push_back({"Join the brotherhood","the union rate on every load, brothers on the CB, the brotherhood's work when you ask",""});
                        rows.push_back({"Talk","what the brotherhood is",""});
                        break;
                    case TalkWith::Station: {
                        const std::string why=career.state().record ? "no criminals on the force - you have a record" :
                            career.state().side==game::career::Side::Gang ? "you still run with a gang - tell your boss you're out first" :
                            career.trucker() ? "you ride with the brotherhood - leave it first" : "a patrol car, a sidearm and radio calls";
                        const bool may=!career.state().record && career.state().side==game::career::Side::Civilian;
                        title="POLICIO DE VERDA";subtitle="The front desk";
                        rows.push_back({"Join the Policio de Verda",why,"",may});
                        break;
                    }
                    default: return;
                    }
                    break;
                }
                case GearScreen::Confirm:
                    title="ARE YOU SURE?";back="NO";
                    subtitle=pending==Pending::LeaveGang ? "Walk away from "+gang_name(talk_crew>=0 ? mafio.crews()[static_cast<std::size_t>(talk_crew)].gang : 0) :
                             pending==Pending::LeaveBrotherhood ? std::string("Leave La Frateco de Kamionistoj") : std::string("Hand in your badge");
                    rows.push_back({pending==Pending::HandInBadge ? "Yes - hand in my badge" : "Yes - I'm out",
                                    pending==Pending::HandInBadge ? "your car and calls go back to the Policio" : "you can come back another day",""});
                    break;
                case GearScreen::GunMan:
                    title="PAFILISTO";subtitle=gang_name(gear_gang)+" - for the family, no questions";back="LEAVE";
                    for(std::size_t i=0;i<armory::armory.size();++i) {
                        const auto& a=armory::armory[i];
                        if(a.gang_rank<0) continue;
                        const bool may=armory::gang_may_buy(career.rank(),i);
                        rows.push_back({a.name,may ? (a.ammo[0] ? TextFormat("with %d rounds",a.rounds) : "") : std::string("for a ")+gang_ranks[a.gang_rank],
                                        may ? std::to_string(a.gang_price)+" kr" : "Rank",may && loot_session.economy().money>=a.gang_price});
                    }
                    break;
                default: return;
                }
                gear_screen=screen;
                gear_menu.set_font(sign_renderer.font());
                gear_menu.open(title,subtitle,std::move(rows),back,selected);
                input_system.cancel_controls();
            };
            // Kit into the bag: the gun (or gear) if it isn't there, and its rounds.
            const auto issue=[&](std::size_t i) {
                const auto& a=armory::armory[i];
                auto& bag=loot_session.edit_inventory();
                const bool gear=!a.ammo[0];
                int got=0;
                if(gear) got+=bag.add(item_registry,a.item,std::max(1,a.rounds));
                else {
                    if(bag.count(a.item)==0) got+=bag.add(item_registry,a.item,1);
                    got+=bag.add(item_registry,a.ammo,a.rounds);
                }
                loot_session.changed();
                return got>0;
            };
            if(gear_open_desk) {gear_open_desk=false;open_gear(GearScreen::Desk,0);}
            if(talk_open) {talk_open=false;pending=Pending::None;open_gear(GearScreen::Talk,0);}
            if(gear_open_gun_man) {gear_open_gun_man=false;open_gear(GearScreen::GunMan,0);}
            if(gear_menu.is_open()) {
                if(!player_health.alive() || vehicles.driver()) {gear_menu.close();gear_screen=GearScreen::None;}
                const auto act=gear_menu.is_open() ? gear_menu.input(screen_width,screen_height,IsWindowFocused()) : game::autos::MenuAction{};
                if(act.back) {
                    if(gear_screen==GearScreen::Armory) open_gear(GearScreen::Desk,1);
                    else if(gear_screen==GearScreen::Confirm) open_gear(pending==Pending::HandInBadge ? GearScreen::Desk : GearScreen::Talk,0);
                    else {gear_menu.close();gear_screen=GearScreen::None;input_system.cancel_controls();}
                } else if(act.chosen>=0) {
                    const auto feet=Vector3{player.position.x,player.position.y-1,player.position.z};
                    switch(gear_screen) {
                    case GearScreen::Desk:
                        if(act.chosen==0) {
                            const Vector3 at=Vector3Add(feet,{std::cos(player.yaw)*1.2F,0,-std::sin(player.yaw)*1.2F});
                            gear_menu.set_status(player_crew.assign_partner(at,player.yaw*RAD2DEG+180,br_bots,match_time) ?
                                "Desk sergeant: \"Officer Novak rides with you from now on.\"" : "You already have a partner");
                            open_gear(GearScreen::Desk,0);
                        } else if(act.chosen==1) open_gear(GearScreen::Armory,0);
                        else {pending=Pending::HandInBadge;open_gear(GearScreen::Confirm,0);}
                        break;
                    case GearScreen::Talk: {
                        const auto done=[&]{gear_menu.close();gear_screen=GearScreen::None;input_system.cancel_controls();};
                        const int gang=talk_crew>=0 && talk_crew<static_cast<int>(mafio.crews().size()) ? mafio.crews()[static_cast<std::size_t>(talk_crew)].gang : 0;
                        const bool dario=talk_crew>=0 && !campaign_path.empty() && mafio.casino()==&mafio.crews()[static_cast<std::size_t>(talk_crew)] && gang==0;
                        const bool occupied=endgame.phase()==game::mafia::EndPhase::Occupied;
                        switch(talk_with) {
                        case TalkWith::OwnBoss:
                            if(act.chosen==0) {
                                chief_line=std::string(dario ? "Uncle Dario: " : "Boss: ")+game::career::Career::boss_talk(gang,true,island_ruled,occupied,boss_talks++);
                                chief_line_time=9;done();
                            } else {pending=Pending::LeaveGang;open_gear(GearScreen::Confirm,0);}
                            break;
                        case TalkWith::GangBoss:
                            if(act.chosen==0) {
                                std::string why;
                                const std::string name=gang_name(gang);
                                if(career.join_gang(gang,name,why)) {
                                    mafio.set_player_gang(gang);services.assign_player_car({});gun_audio.kill_confirm();
                                    loot_session.edit_inventory().add(item_registry,"spray_paint",4);loot_session.changed();   // cans to mark your turf
                                    island_ruled=territory.rules_island(gang);
                                    if(gang==0 && own_dario_flat()) {
                                        chief_line="Uncle Dario: \"Welcome to the family, nevo. Here - the keys to a flat just down from the bar. Your hideout. "
                                                   "Ron doesn't need to know everything.\"";chief_line_time=9;
                                    } else {chief_line="You run with "+name+" now - the boss will have work for you";chief_line_time=5;}
                                    done();
                                } else gear_menu.set_status(why);
                            } else {
                                chief_line=std::string(dario ? "Uncle Dario: " : "Boss of "+gang_name(gang)+": ")+game::career::Career::boss_talk(gang,false,false,occupied,boss_talks++);
                                chief_line_time=9;done();
                            }
                            break;
                        case TalkWith::Ivo: {
                            std::string why;
                            if(act.chosen==0) {chief_line=trucker_talk(0);chief_line_time=9;done();}
                            else if(act.chosen==1) {
                                if(career.call_strike(match_time,why)) {
                                    chief_line=std::string(brotherhood_man)+": \"That's the spirit. Get a big rig into the company's gate - your own, one off the park, trailer or no - "
                                               "and keep it there a minute. The brothers will be on the picket line with you.\"";chief_line_time=9;done();
                                } else gear_menu.set_status(why);
                            } else if(act.chosen==2) {
                                if(career.ask_for_run(why)) {chief_line=std::string(brotherhood_man)+": \"Crates came in last night. Listen to the CB - and watch for the patrol.\"";chief_line_time=7;done();}
                                else gear_menu.set_status(why);
                            } else {pending=Pending::LeaveBrotherhood;open_gear(GearScreen::Confirm,0);}
                            break;
                        }
                        case TalkWith::IvoJoin:
                            if(act.chosen==0) {
                                std::string why;
                                if(career.join_truckers(why)) {
                                    gun_audio.kill_confirm();
                                    chief_line=std::string(brotherhood_man)+": \"Welcome, brother. The union rate on every load, the CB when you need us. Sign on with "+
                                               depot_boss_name+" for loads - and come to me when you want the brotherhood's work.\"";chief_line_time=9;done();
                                } else gear_menu.set_status(why);
                            } else {chief_line=trucker_talk(0);chief_line_time=9;done();}
                            break;
                        case TalkWith::Station: {
                            std::string why;
                            if(career.join_police(talk_car,why)) {
                                services.assign_player_car(talk_car);mafio.set_player_gang(-1);gun_audio.kill_confirm();
                                // Issued: a sidearm and two magazines.
                                auto& bag=loot_session.edit_inventory();
                                if(!bag.owns_weapon(item_registry,game::combat::WeaponId::Pistol)) bag.add(item_registry,"pistol",1);
                                bag.add(item_registry,"pistol_ammo",30);loot_session.changed();
                                chief_line="Desk sergeant: \"Welcome to the Policio de Verda. Your car's out front - report to "+std::string(game::career::chief_name)+" at Verda City HQ.\"";
                                chief_line_time=9;done();
                            } else gear_menu.set_status(why);
                            break;
                        }
                        default: done(); break;
                        }
                        break;
                    }
                    case GearScreen::Confirm: {
                        leave_side(pending);pending=Pending::None;
                        gear_menu.close();gear_screen=GearScreen::None;input_system.cancel_controls();
                        break;
                    }
                    case GearScreen::Armory:
                        if(armory::police_may_draw(career.rank(),static_cast<std::size_t>(act.chosen))) {
                            gear_menu.set_status(issue(static_cast<std::size_t>(act.chosen)) ? "Signed out - bring it back in one piece" : "Your bag is full");
                            open_gear(GearScreen::Armory,act.chosen);
                        }
                        break;
                    case GearScreen::GunMan: {
                        // (The list skips what he doesn't sell: find the row's item.)
                        int row=-1;
                        for(std::size_t i=0;i<armory::armory.size();++i) {
                            if(armory::armory[i].gang_rank<0) continue;
                            if(++row==act.chosen) {
                                auto& wallet=loot_session.economy();
                                if(!armory::gang_may_buy(career.rank(),i)) break;
                                if(wallet.money<armory::armory[i].gang_price) {gear_menu.set_status("Not enough kronoj");break;}
                                if(issue(i)) {wallet.money-=armory::armory[i].gang_price;loot_session.changed();gear_menu.set_status("\"Pleasure doing business. You didn't get it from me.\"");}
                                else gear_menu.set_status("Your bag is full");
                                break;
                            }
                        }
                        open_gear(GearScreen::GunMan,act.chosen);
                        break;
                    }
                    default: break;
                    }
                }
            }
            // An officer getting into a police car takes the shotgun from its rack (once a car).
            if(const auto* car=vehicles.driver();car && career.on_duty())
                if(const auto* asset=vehicles.asset(verda_region,car->id);asset && armory::police_vehicle(asset->vehicle.definition) && !shotgun_cars.contains(car->id)) {
                    shotgun_cars.insert(car->id);
                    auto& bag=loot_session.edit_inventory();
                    const bool had=bag.count("shotgun")>0;
                    if(!had) bag.add(item_registry,"shotgun",1);
                    bag.add(item_registry,"shotgun_shells",10);loot_session.changed();
                    interaction_message=had ? "Shells from the car's rack: +10" : "The car's pump shotgun is in your hands - and ten shells";interaction_remaining=4;
                }
        }
        // ====================================================
        // THE MOTOR TRADE (game::autos): Marnafto's pumps, the Meĥanikejo bays, the car lot
        // ====================================================
        if(game_mode==game::GameMode::Explore && game::rules_for(game_mode).civilian_life && !creator_active) {
            using game::autos::MenuRow;
            namespace autos=game::autos;
            auto& wallet=loot_session.economy();
            const auto kr=[](int n){return std::to_string(n)+" kr";};
            // The cars the player bought: back where they were left, as they were fitted.
            const auto store_owned=[&]() {
                wallet.cars.clear();
                for(const auto& c:owned_cars) wallet.cars.push_back(autos::serialise(c));
            };
            const auto spawn_owned=[&](autos::OwnedCar& c) {
                if(!vehicles.spawn(verda_region,c.at,c.yaw,c.definition)) return false;
                c.vehicle=vehicles.last_spawned();
                vehicles.place(verda_region,c.vehicle,c.at,c.yaw);
                if(auto* a=vehicles.asset(verda_region,c.vehicle)) {
                    autos::apply(c.mods,a->vehicle);
                    a->vehicle.fuel=c.fuel;
                    a->vehicle.marker="owned_"+c.vehicle;   // nobody else's (island life leaves it be)
                }
                vehicle_dirty=true;
                return true;
            };
            if(!owned_ready && player_health.alive()) {
                owned_ready=true;
                for(const auto& text:wallet.cars) {
                    autos::OwnedCar c;
                    if(autos::parse(text,c) && spawn_owned(c)) owned_cars.push_back(c);
                }
#ifdef OUTLAND_DEV_TOOLS
                // OUTLAND_DEV_MONEY=N: that much in the wallet; OUTLAND_DEV_DRIVE=hatchback: at the wheel of one.
                if(const char* cash=std::getenv("OUTLAND_DEV_MONEY")) wallet.money=std::max<long long>(0,std::atoll(cash));
                if(const char* type=std::getenv("OUTLAND_DEV_DRIVE")) {
                    const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
                    if(vehicles.spawn(verda_region,feet,player.yaw*RAD2DEG,type)) {
                        const auto id=vehicles.last_spawned();
                        vehicles.place(verda_region,id,feet,player.yaw*RAD2DEG);
                        const int index=vehicles.find(id);
                        if(index>=0) vehicles.enter(verda_region,index,feet);
                        // OUTLAND_DEV_MODS="paint engine tires bumper plating gun tank exhaust spoiler tint rims glow": fitted.
                        autos::Mods m;
                        if(const char* fit=std::getenv("OUTLAND_DEV_MODS"); fit && autos::parse(fit,m))
                            if(auto* car=vehicles.asset(verda_region,id)) autos::apply(m,car->vehicle);
                    }
                }
#endif
            }
            // The brotherhood's rig for a new brother: in the truck park at the depot, the player's to keep.
            if(owned_ready && career.rig_due() && depot_boss.placed && player_health.alive()) {
                autos::OwnedCar c;
                c.definition="semi";
                // The last bay in La Ringo's truck park, kept for him.
                const auto& home=world::highways::brotherhood_home();
                const Vector3 bay=home.bay(world::highways::truck_bays-1);
                c.at={bay.x,world::terrain::TerrainHeight::sample(bay.x,bay.z),bay.z};c.yaw=home.bay_yaw();
                if(owned_cars.size()<autos::max_owned && spawn_owned(c)) {
                    owned_cars.push_back(c);store_owned();
                    chief_line=std::string(brotherhood_man)+": \"Every brother drives his own. Your rig's in the last bay out back, box trailer hitched, "
                               "tank full. Take her into the company's shed for armour and a roof gun when you can afford it.\"";chief_line_time=10;
                }
                career.rig_issued();
            }
            {
                std::string own;
                for(const auto& c:owned_cars) if(c.definition=="semi") {own=c.vehicle;break;}
                trucking.set_own_rig(own);
            }
            if((owned_sync-=dt)<=0 && !owned_cars.empty()) {
                owned_sync=3;
                for(auto& c:owned_cars)
                    if(const auto* a=vehicles.asset(verda_region,c.vehicle)) {c.at=a->position;c.yaw=a->rotation_y;c.fuel=a->vehicle.fuel;c.mods=autos::mods_of(a->vehicle);}
                store_owned();
            }
            const auto served=[&]()->world::WorldAsset*{return motor_car.empty() ? nullptr : vehicles.asset(verda_region,motor_car);};
            const auto open_motor=[&](MotorScreen screen,int selected) {
                std::vector<MenuRow> rows;
                std::string title,subtitle,back="BACK";
                const auto* car=served();
                const auto mods=car ? autos::mods_of(car->vehicle) : autos::Mods{};
                const auto part_rows=[&](const auto& parts,int fitted) {
                    for(std::size_t i=0;i<parts.size();++i) {
                        const bool on=static_cast<int>(i)==fitted;
                        rows.push_back({parts[i].name,parts[i].detail,on ? "Fitted" : parts[i].price>0 ? kr(parts[i].price) : "Free",!on,on});
                    }
                };
                switch(screen) {
                case MotorScreen::Pump: {
                    if(outbreak.active() && motor_site>=0 && motor_site<static_cast<int>(motor_sites.size())) {
                        // The outbreak: whatever is left in the station's tanks, free to whoever gets here.
                        const float left=game::outbreak::fuel_left(outbreak.state(),motor_site,motor_sites[static_cast<std::size_t>(motor_site)].origin);
                        title="MARNAFTO";subtitle=left<=.001F ? "Dry. This station is done" : TextFormat("%.1f tankfuls left in the ground",left);back="LEAVE";
                        const bool full=car && car->vehicle.fuel>.995F;
                        rows.push_back({"Fill up",TextFormat("%d%% in the tank",car ? static_cast<int>(car->vehicle.fuel*100+.5F) : 0),
                                        left<=.001F ? "DRY" : full ? "Full" : "Take it",left>.001F && !full});
                        break;
                    }
                    title="MARNAFTO";subtitle="Nafto el niaj propraj platformoj";back="LEAVE";
                    const int cost=car ? autos::fill_cost(car->vehicle.fuel) : 0;
                    rows.push_back({"Fill up",TextFormat("%d%% in the tank",car ? static_cast<int>(car->vehicle.fuel*100+.5F) : 0),cost>0 ? kr(cost) : "Full",cost>0});
                    break;
                }
                case MotorScreen::Depot: {
                    // Verda Transporto's shed: the company's mechanics service a rig and fit it out for the road.
                    title="VERDA TRANSPORTO · SERVO";subtitle=career.trucker() ? "A brother pays 30% less" : "Service and fit-out for working trucks";back="LEAVE";
                    const int service=car ? autos::repair_cost(car->vehicle)+(car->vehicle.fuel<.999F ? autos::fill_cost(car->vehicle.fuel) : 0) : 0;
                    rows.push_back({"Service",service>0 ? "repairs, new tyres, a full tank" : "nothing to do",service>0 ? kr(service) : "",service>0});
                    rows.push_back({"Engine",autos::engines[static_cast<std::size_t>(mods.engine)].name,""});
                    rows.push_back({"Armour plating",autos::platings[static_cast<std::size_t>(mods.plating)].name,""});
                    rows.push_back({"Roof gun",autos::guns[static_cast<std::size_t>(mods.gun)].name,""});
                    rows.push_back({"Fuel tanks",autos::tanks[static_cast<std::size_t>(mods.tank)].name,""});
                    rows.push_back({"Paint",mods.paint>=0 ? autos::paints[static_cast<std::size_t>(mods.paint)].name : "Factory colour",""});
                    rows.push_back({"Front bumper",autos::bumpers[static_cast<std::size_t>(mods.bumper)].name,""});
                    break;
                }
                case MotorScreen::Plating: title="KIRASO";subtitle="Steel against bullets";part_rows(autos::platings,mods.plating);break;
                case MotorScreen::Gun: title="TEGMENTA PAFILO";subtitle="A gun on the roof - gangs think twice";part_rows(autos::guns,mods.gun);break;
                case MotorScreen::Tanks: title="BENZINUJOJ";subtitle="More road between the diesel lanes";part_rows(autos::tanks,mods.tank);break;
                case MotorScreen::Workshop: {
                    title="MEĤANIKEJO";subtitle="What can we do for you?";back="LEAVE";
                    const int repair=car ? autos::repair_cost(car->vehicle) : 0;
                    rows.push_back({"Paint",mods.paint>=0 ? autos::paints[static_cast<std::size_t>(mods.paint)].name : "Factory colour",""});
                    rows.push_back({"Engine",autos::engines[static_cast<std::size_t>(mods.engine)].name,""});
                    rows.push_back({"Tyres",autos::tires[static_cast<std::size_t>(mods.tires)].name,""});
                    rows.push_back({"Front bumper",autos::bumpers[static_cast<std::size_t>(mods.bumper)].name,""});
                    rows.push_back({"Exhaust",autos::exhausts[static_cast<std::size_t>(mods.exhaust)].name,""});
                    rows.push_back({"Spoiler",autos::spoilers[static_cast<std::size_t>(mods.spoiler)].name,""});
                    rows.push_back({"Window tint",autos::tints[static_cast<std::size_t>(mods.tint)].name,""});
                    rows.push_back({"Rims",autos::rims[static_cast<std::size_t>(mods.rims)].name,""});
                    rows.push_back({"Underglow",autos::glows[static_cast<std::size_t>(mods.glow)].name,""});
                    rows.push_back({"Repair",repair>0 ? "body, engine and tyres" : "nothing to fix",repair>0 ? kr(repair) : "",repair>0});
                    break;
                }
                case MotorScreen::Paint:
                    title="FARBO";subtitle="A new colour, sprayed while you wait";
                    rows.push_back({"Factory colour","as it left the works",mods.paint<0 ? "Fitted" : "Free",mods.paint>=0,mods.paint<0});
                    for(std::size_t i=0;i<autos::paints.size();++i) {
                        const bool on=static_cast<int>(i)==mods.paint;
                        rows.push_back({autos::paints[i].name,"",on ? "Fitted" : kr(autos::paints[i].price),!on,on});
                    }
                    break;
                case MotorScreen::Engine: {
                    // Petrol engines for a car, diesels for a big rig (cheapest first).
                    const bool diesel=car && car->vehicle.definition=="semi";
                    title="MOTORO";subtitle=diesel ? "More pull for the hills and the heavy loads" : "More power";
                    for(const int i:autos::engine_choices(diesel)) {
                        const auto& e=autos::engines[static_cast<std::size_t>(i)];
                        const bool on=i==mods.engine;
                        rows.push_back({e.name,e.detail,on ? "Fitted" : e.price>0 ? kr(e.price) : "Free",!on,on});
                    }
                    break;
                }
                case MotorScreen::Tires: title="PNEŬOJ";subtitle="Bigger or smaller wheels";part_rows(autos::tires,mods.tires);break;
                case MotorScreen::Bumper: title="BUFROJ";subtitle="Something on the front";part_rows(autos::bumpers,mods.bumper);break;
                case MotorScreen::Exhaust: title="ELLASILO";subtitle="Let it be heard";part_rows(autos::exhausts,mods.exhaust);break;
                case MotorScreen::Spoiler: title="SPOILERO";subtitle="Something on the back";part_rows(autos::spoilers,mods.spoiler);break;
                case MotorScreen::Tint: title="FENESTROJ";subtitle="Darker glass";part_rows(autos::tints,mods.tint);break;
                case MotorScreen::Rims: title="RADRINGOJ";subtitle="New wheels";part_rows(autos::rims,mods.rims);break;
                case MotorScreen::Glow: title="SUBLUMO";subtitle="Lights under the car";part_rows(autos::glows,mods.glow);break;
                case MotorScreen::Dealer:
                    title="AŬTOVENDEJO VERDA";subtitle="Drive it home today";back="LEAVE";
                    for(const auto& sale:autos::dealer) rows.push_back({sale.name,sale.detail,kr(sale.price),true});
                    break;
                default: return;
                }
                motor_screen=screen;
                motor_menu.set_font(sign_renderer.font());
                motor_menu.open(title,subtitle,std::move(rows),back,selected);
            };
            const auto leave_motor=[&]() {
                motor_menu.close();motor_screen=MotorScreen::None;motor_served=motor_site;motor_car.clear();
                input_system.cancel_controls();
            };
            if(depot_barred) {
                depot_barred=false;
                interaction_message="The trailer won't go in the shed - drop it out here first (J), then drive in";interaction_remaining=4;
            }
            // Pull up at a pump or into a workshop bay (and stop): the attendant or the mechanic comes over.
            const auto* driven=vehicles.driver();
            if(!motor_menu.is_open() && player_health.alive()) {
                for(std::size_t i=0;i<motor_sites.size();++i) {
                    const auto& site=motor_sites[i];
                    const bool there=driven && Vector3Distance(site.origin,driven->position)<40 &&
                        ((site.kind==world::motor::Kind::Gas && world::motor::at_pump(site,driven->position)) ||
                         ((site.kind==world::motor::Kind::Mechanic || site.kind==world::motor::Kind::Depot) && world::motor::bay_at(site,driven->position)>=0));
                    if(static_cast<int>(i)==motor_served) {if(!there) motor_served=-1;continue;}
                    if(!there || std::abs(driven->speed)>.6F) continue;
                    motor_site=static_cast<int>(i);motor_car=driven->id;
                    open_motor(site.kind==world::motor::Kind::Gas ? MotorScreen::Pump : site.kind==world::motor::Kind::Depot ? MotorScreen::Depot : MotorScreen::Workshop,0);
                    break;
                }
            }
            // Marnafto's diesel lanes: a big rig pulled up beside the island and stopped fills by
            // itself - on Verda Transporto's account in one of its rigs while you drive for it.
            if(driven && player_health.alive() && !motor_menu.is_open())
                if(auto* rig=vehicles.asset(verda_region,driven->id);rig && rig->model_path.find("semi_body")!=std::string::npos) {
                    const bool at=std::any_of(diesel_lanes.begin(),diesel_lanes.end(),[&](Vector3 p){
                        return std::hypot(p.x-driven->position.x,p.z-driven->position.z)<game::freight::Diesel::reach;});
                    if(!at || std::abs(driven->speed)>game::freight::Diesel::still) diesel_paid=0;
                    else if(rig->vehicle.fuel<.999F) {
                        const bool company=trucking.hired() && game::freight::Trucking::company_rig(driven->id);
                        const auto before=rig->vehicle.fuel;
                        const auto paid=game::freight::Diesel::fill(rig->vehicle.fuel,dt,company,wallet.money,diesel_owed,rig->vehicle.range);
                        diesel_paid+=paid;vehicle_dirty=true;
                        if(paid>0) loot_session.changed();
                        interaction_message=rig->vehicle.fuel<=before ? std::string("DIZELO - not enough kronoj") :
                            TextFormat("DIZELO  %d%%  -  %s",static_cast<int>(rig->vehicle.fuel*100),
                                       company ? "on Verda Transporto's account" : TextFormat("%lld kr",static_cast<long long>(diesel_paid)));
                        interaction_remaining=1.5F;
                    } else if(diesel_paid>=0) {interaction_message="DIZELO - tank full";interaction_remaining=2;diesel_paid=-1;}
                }
            // The car lot: at the salesman's desk, on foot.
            int at_desk=-1;
            if(!driven && !motor_menu.is_open())
                for(std::size_t i=0;i<motor_sites.size();++i)
                    if(motor_sites[i].kind==world::motor::Kind::CarLot &&
                       Vector3Distance(motor_sites[i].to_world(world::motor::car_lot_desk),{player.position.x,player.position.y-1,player.position.z})<2.4F)
                        at_desk=static_cast<int>(i);
            if(at_desk>=0) {
                use_available=true;
                if(store_prompt.empty()) store_prompt="E - Buy a car (Aŭtovendejo Verda)";
                if(controls.interact && !inventory_open && !(shop_open||store_open) && !poker_open) {
                    input_system.player().interact=false;
                    motor_site=at_desk;motor_car.clear();
                    open_motor(MotorScreen::Dealer,0);
                }
            }
            if(motor_menu.is_open()) {
                const auto act=motor_menu.input(screen_width,screen_height,IsWindowFocused());
                auto* car=served();
                const bool at_depot=motor_site>=0 && motor_site<static_cast<int>(motor_sites.size()) &&
                    motor_sites[static_cast<std::size_t>(motor_site)].kind==world::motor::Kind::Depot;
                const auto pay=[&](int price) {
                    if(at_depot && career.trucker()) price=price*7/10;   // the brotherhood's price at the company shed
                    if(wallet.money<price) {motor_menu.set_status("Not enough kronoj");return false;}
                    wallet.money-=price;loot_session.changed();return true;
                };
                const auto fit=[&](autos::Mods m,int price,const char* done) {
                    if(!car || !pay(price)) return;
                    autos::apply(m,car->vehicle);vehicle_dirty=true;
                    for(auto& c:owned_cars) if(c.vehicle==car->id) c.mods=m;
                    store_owned();
                    motor_menu.set_status(done);
                };
                const int chosen=act.chosen;
                const auto mods=car ? autos::mods_of(car->vehicle) : autos::Mods{};
                if(act.back) {
                    if(at_depot && (motor_screen==MotorScreen::Paint || motor_screen==MotorScreen::Engine || motor_screen==MotorScreen::Bumper ||
                                    motor_screen==MotorScreen::Plating || motor_screen==MotorScreen::Gun || motor_screen==MotorScreen::Tanks))
                        open_motor(MotorScreen::Depot,motor_screen==MotorScreen::Engine ? 1 : motor_screen==MotorScreen::Plating ? 2 : motor_screen==MotorScreen::Gun ? 3 :
                                                      motor_screen==MotorScreen::Tanks ? 4 : motor_screen==MotorScreen::Paint ? 5 : 6);
                    else if(const auto row=std::find(workshop_rows.begin(),workshop_rows.end(),motor_screen);row!=workshop_rows.end())
                        open_motor(MotorScreen::Workshop,static_cast<int>(row-workshop_rows.begin()));
                    else leave_motor();
                } else if(chosen>=0 && (car || motor_screen==MotorScreen::Dealer)) {
                    switch(motor_screen) {
                    case MotorScreen::Pump: {
                        if(outbreak.active()) {
                            const auto& site=motor_sites[static_cast<std::size_t>(motor_site)];
                            const float got=game::outbreak::draw_fuel(outbreak.state(),motor_site,site.origin,std::max(0.0F,1-car->vehicle.fuel));
                            car->vehicle.fuel=std::min(1.0F,car->vehicle.fuel+got);vehicle_dirty=true;save_outbreak();
                            const float left=game::outbreak::fuel_left(outbreak.state(),motor_site,site.origin);
                            motor_menu.set_status(got<=.001F ? "Nothing comes out. This station is done" :
                                left<=.001F ? TextFormat("Pumped %d%% of a tank - and the pump runs dry. That's the last of it",static_cast<int>(got*100+.5F)) :
                                TextFormat("Pumped %d%% of a tank. %.1f tankfuls left here",static_cast<int>(got*100+.5F),left));
                            open_motor(MotorScreen::Pump,0);
                            break;
                        }
                        const float before=car->vehicle.fuel;
                        const auto money=wallet.money;
                        car->vehicle.fuel=autos::fill(before,wallet.money);
                        loot_session.changed();vehicle_dirty=true;
                        motor_menu.set_status(car->vehicle.fuel>before ? TextFormat("Filled up: %lld kr. Thank you for choosing Marnafto",static_cast<long long>(money-wallet.money)) : "Not enough kronoj");
                        open_motor(MotorScreen::Pump,0);
                        break;
                    }
                    case MotorScreen::Depot: {
                        const bool rig=car->vehicle.definition=="semi" || car->vehicle.definition=="truck";
                        if(chosen==0) {
                            const int cost=autos::repair_cost(car->vehicle)+(car->vehicle.fuel<.999F ? autos::fill_cost(car->vehicle.fuel) : 0);
                            if(cost>0 && pay(cost)) {autos::repair(car->vehicle);car->vehicle.fuel=1;vehicle_dirty=true;motor_menu.set_status("Serviced: good as new, tank full");}
                            open_motor(MotorScreen::Depot,0);
                        } else if((chosen==2 || chosen==3) && !rig) motor_menu.set_status("Plating and roof guns are for working trucks - a rig or a pickup");
                        else if(chosen==4 && car->vehicle.definition!="semi") motor_menu.set_status("Long-range tanks are for big rigs");
                        else open_motor(chosen==1 ? MotorScreen::Engine : chosen==2 ? MotorScreen::Plating : chosen==3 ? MotorScreen::Gun :
                                        chosen==4 ? MotorScreen::Tanks : chosen==5 ? MotorScreen::Paint : MotorScreen::Bumper,0);
                        break;
                    }
                    case MotorScreen::Plating: {auto m=mods;m.plating=chosen;fit(m,autos::platings[static_cast<std::size_t>(chosen)].price,chosen ? "Plated - let them shoot" : "The plating comes off");open_motor(MotorScreen::Plating,chosen);break;}
                    case MotorScreen::Tanks: {auto m=mods;m.tank=chosen;fit(m,autos::tanks[static_cast<std::size_t>(chosen)].price,chosen ? "The new tanks are in - more road between fills" : "Back to the factory tanks");open_motor(MotorScreen::Tanks,chosen);break;}
                    case MotorScreen::Gun: {auto m=mods;m.gun=chosen;fit(m,autos::guns[static_cast<std::size_t>(chosen)].price,chosen ? "The gun is on the roof - FIRE at the wheel works it" : "The gun comes off");open_motor(MotorScreen::Gun,chosen);break;}
                    case MotorScreen::Workshop:
                        if(chosen<static_cast<int>(workshop_rows.size())) {open_motor(workshop_rows[static_cast<std::size_t>(chosen)],0);break;}
                        if(pay(autos::repair_cost(car->vehicle))) {autos::repair(car->vehicle);vehicle_dirty=true;motor_menu.set_status("Good as new");}
                        open_motor(MotorScreen::Workshop,static_cast<int>(workshop_rows.size()));
                        break;
                    case MotorScreen::Paint: {
                        auto m=mods;m.paint=chosen-1;
                        fit(m,chosen==0 ? 0 : autos::paints[static_cast<std::size_t>(chosen-1)].price,chosen==0 ? "Back to the factory colour" : "A fresh coat of paint");
                        open_motor(MotorScreen::Paint,chosen);
                        break;
                    }
                    case MotorScreen::Engine: {
                        const auto choices=autos::engine_choices(car->vehicle.definition=="semi");
                        if(chosen>=static_cast<int>(choices.size())) break;
                        auto m=mods;m.engine=choices[static_cast<std::size_t>(chosen)];
                        fit(m,autos::engines[static_cast<std::size_t>(m.engine)].price,"The new engine is in");open_motor(MotorScreen::Engine,chosen);break;
                    }
                    case MotorScreen::Tires: {auto m=mods;m.tires=chosen;fit(m,autos::tires[static_cast<std::size_t>(chosen)].price,"New tyres on");open_motor(MotorScreen::Tires,chosen);break;}
                    case MotorScreen::Bumper: {auto m=mods;m.bumper=chosen;fit(m,autos::bumpers[static_cast<std::size_t>(chosen)].price,"Bolted on");open_motor(MotorScreen::Bumper,chosen);break;}
                    case MotorScreen::Exhaust: {auto m=mods;m.exhaust=chosen;fit(m,autos::exhausts[static_cast<std::size_t>(chosen)].price,chosen ? "Give it a rev - hear that?" : "Quiet again");open_motor(MotorScreen::Exhaust,chosen);break;}
                    case MotorScreen::Spoiler: {auto m=mods;m.spoiler=chosen;fit(m,autos::spoilers[static_cast<std::size_t>(chosen)].price,chosen ? "Bolted to the boot" : "The spoiler comes off");open_motor(MotorScreen::Spoiler,chosen);break;}
                    case MotorScreen::Tint: {auto m=mods;m.tint=chosen;fit(m,autos::tints[static_cast<std::size_t>(chosen)].price,chosen ? "Tinted" : "Clear glass again");open_motor(MotorScreen::Tint,chosen);break;}
                    case MotorScreen::Rims: {auto m=mods;m.rims=chosen;fit(m,autos::rims[static_cast<std::size_t>(chosen)].price,"New rims on");open_motor(MotorScreen::Rims,chosen);break;}
                    case MotorScreen::Glow: {auto m=mods;m.glow=chosen;fit(m,autos::glows[static_cast<std::size_t>(chosen)].price,chosen ? "Lit up underneath" : "The lights come off");open_motor(MotorScreen::Glow,chosen);break;}
                    case MotorScreen::Dealer: {
                        const auto& sale=autos::dealer[static_cast<std::size_t>(chosen)];
                        if(owned_cars.size()>=autos::max_owned) {motor_menu.set_status("That's enough cars for anyone");break;}
                        if(motor_site<0 || motor_site>=static_cast<int>(motor_sites.size()) || wallet.money<sale.price) {motor_menu.set_status("Not enough kronoj");break;}
                        const auto& lot=motor_sites[static_cast<std::size_t>(motor_site)];
                        autos::OwnedCar c;
                        c.definition=sale.definition;
                        c.at=lot.to_world(world::motor::car_lot_delivery);
                        c.at.y=world::terrain::TerrainHeight::sample(c.at.x,c.at.z);
                        c.yaw=lot.yaw+180;
                        if(!spawn_owned(c)) {motor_menu.set_status("Come back in a minute - the lot is full");break;}
                        pay(sale.price);
                        owned_cars.push_back(c);store_owned();
                        leave_motor();
                        interaction_message=std::string("Your new ")+sale.name+" is waiting by the lot's exit. It's yours: it'll be where you leave it";interaction_remaining=6;
                        break;
                    }
                    default: break;
                    }
                }
            }
        }
        if(controls.interact && !creator_active && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !at_table) {
            Vector3 exit{};
            if(bus_ride>=0) {
                // On the bus: off at a stop, toward the shelter.
                if(bus_line.alight(bus_ride)) {
                    const auto& bus=bus_line.buses()[static_cast<std::size_t>(bus_ride)];
                    Vector3 kerb=bus.position;
                    float nearest=1e9F;
                    for(const auto& stop:bus_line.stops())
                        if(const float d=Vector3Distance(stop.kerb,bus.position);d<nearest) {nearest=d;kerb=stop.shelter;}
                    Vector3 out=Vector3Subtract(kerb,bus.position);out.y=0;
                    out=Vector3Length(out)>.1F ? Vector3Normalize(out) : Vector3{1,0,0};
                    Vector3 at=Vector3Add(bus.position,Vector3Scale(out,3.0F));
                    at.y=world::physics::WorldCollision::ground_height({at.x,bus.position.y+2,at.z},bus.position.y+2,verda_region);
                    if(!std::isfinite(at.y)) at.y=world::terrain::TerrainHeight::sample(at.x,at.z);
                    player.position=Vector3Add(at,{0,1,0});player.vertical_velocity=0;
                    bus_ride=-1;vehicle_renderer.reset_camera();
                    interaction_message="Off the bus";
                } else {
                    const auto& bus=bus_line.buses()[static_cast<std::size_t>(bus_ride)];
                    interaction_message="Get off at the next stop: "+bus_line.stops()[static_cast<std::size_t>(bus.next_stop)].town;
                }
            } else if(riding_now) {
                // USE in the passenger seat: "Pull over here."
                player_crew.end_ride(br_bots,bot_drivers,vehicles,verda_region);
                interaction_message="Your crew pulls over";
            } else if(vehicles.driver()){
                // (Not on the press that got you in, or a second tap right after it: a double tap
                // on USE used to drop you straight back out.)
                if(GetTime()-entered_at<.75) {}
                else if(vehicles.exit(verda_region,exit)){player.position=Vector3Add(exit,{0,1,0});player.vertical_velocity=0;vehicle_renderer.reset_camera();interaction_message="Exited vehicle";}
                else interaction_message="Stop and leave space beside the vehicle to exit";
            }else {
                const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
                int vehicle=vehicles.nearest(verda_region,feet);
                // Residents' cars: a moving one can't be boarded; taking one is a crime.
                const std::string id=vehicle>=0 ? vehicles.vehicles()[static_cast<std::size_t>(vehicle)].id : std::string();
                // At a bus standing at its stop: on board, for the fare.
                if(const int b=bus_line.running() && game::rules_for(game_mode).civilian_life ? bus_line.bus_at_stop(feet,9) : -1;b>=0) {
                    constexpr int fare=4;
                    if(loot_session.economy().money<fare) interaction_message="The fare is 4 kr";
                    else if(!bus_line.board(b)) interaction_message="The bus is full - wait for the next one";
                    else {
                        loot_session.economy().money-=fare;loot_session.changed();
                        bus_ride=b;input_system.cancel_controls();vehicle_orbit=vehicle_pitch=0;vehicle_renderer.reset_camera();
                        interaction_message="On the bus - 4 kr. USE at a stop to get off";
                    }
                    interaction_remaining=3;
                    vehicle=-1;
                }
                // At the passenger door with the crew about: one of them drives you - to the map's
                // waypoint, or else the job in hand.
                bool passenger=false;
                if(vehicle>=0 && player_crew.size()>0 && game_mode==game::GameMode::Explore) {
                    const auto& car=vehicles.vehicles()[static_cast<std::size_t>(vehicle)];
                    const float a=car.yaw*DEG2RAD;
                    passenger=(feet.x-car.position.x)*-std::cos(a)+(feet.z-car.position.z)*std::sin(a)>.4F;
                }
                if(passenger) {
                    const auto& car=vehicles.vehicles()[static_cast<std::size_t>(vehicle)];
                    std::optional<Vector3> goal=map_screen.pin();
                    if(!goal && !career_path.empty() && career.job().kind!=game::career::JobKind::None) goal=career.job().target;
                    if(!goal) interaction_message="Set a waypoint on the map (M) or take a job - your crew will drive you there";
                    else if(island_life.car_moving(id) && std::abs(car.speed)>3) interaction_message="It's moving";
                    else if(std::hypot(goal->x-car.position.x,goal->z-car.position.z)<70) interaction_message="That's just round the corner - walk it";
                    else if(const int driver=player_crew.ride(id,car.position,*goal,br_bots,bot_drivers,vehicles,verda_region);driver>=0) {
                        if(game::rules_for(game_mode).civilian_life) island_life.take_car(id);
                        input_system.cancel_controls();vehicle_orbit=vehicle_pitch=0;vehicle_renderer.reset_camera();
                        interaction_message=driver==player_crew.partner() ? "Your partner drives - USE to get out" : "Your crew drives - USE to get out";
                    } else interaction_message="Your crew can't find a road from here";
                }
                else if(vehicle>=0 && island_life.car_moving(id) && std::abs(vehicles.vehicles()[static_cast<std::size_t>(vehicle)].speed)>3) interaction_message="It's moving";
                else if(vehicle>=0 && vehicles.enter(verda_region,vehicle,feet)){
                    vehicle_orbit=vehicle_pitch=0;input_system.cancel_controls();drive_hint=8;entered_at=GetTime();
                    interaction_message=touch_ui ? "Steer left, GAS and BRAKE right; USE gets out; tap the speedometer for the cab view" : "";
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
        // LIGHTS / L in a police car: the light bar and siren on or off (they stay on if you get out).
        if(controls.lights && driving) {
            const auto* car=vehicles.driver();
            if(const auto* asset=vehicles.asset(verda_region,car->id);
               asset && (asset->vehicle.definition=="police" || asset->vehicle.definition=="interceptor")) {
                lights_car=lights_car==car->id ? std::string() : car->id;
                interaction_message=lights_car.empty() ? "Emergency lights off" : "Emergency lights ON - traffic, make way";
                interaction_remaining=2;
            }
        }
        if(!lights_car.empty() && vehicles.find(lights_car)<0) lights_car.clear();
        // HITCH / J at the wheel of a big rig, stopped: drop the trailer, or back under it and hitch it.
        {
            const auto* car=vehicles.driver();
            const auto* asset=car ? vehicles.asset(verda_region,car->id) : nullptr;
            bool hitch=controls.hitch;
            static std::vector<int> hitch_touches;
            if(asset && asset->vehicle.definition=="semi" && touch_ui) {
                std::vector<int> now;
                const Vector2 spot{screen_width*.42F-150,screen_height-92.0F};
                for(int i=0;i<GetTouchPointCount();++i) {
                    now.push_back(GetTouchPointId(i));
                    if(std::find(hitch_touches.begin(),hitch_touches.end(),GetTouchPointId(i))==hitch_touches.end() &&
                       Vector2Distance(GetTouchPosition(i),spot)<46) hitch=true;
                }
                hitch_touches=now;
            }
            if(hitch && asset && asset->vehicle.definition=="semi") {
                interaction_message=std::abs(car->speed)>1 ? std::string("Stop the rig first") : freight.drop_or_hitch(verda_region,vehicles,car->id);
                interaction_remaining=3;
            }
        }
        // RADIO / O at the wheel: the next station, then off.
        if(controls.radio && driving) {car_radio.press();store_settings();}
        // BACKUP / Z for a trucker: the CB - two brothers from the nearest truck stop.
        if(controls.backup && career.trucker() && game_mode==game::GameMode::Explore && player_health.alive() && !kamionistoj.empty()) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const Kamionisto* stop=&kamionistoj.front();
            for(const auto& k:kamionistoj) if(Vector3Distance(k.at,feet)<Vector3Distance(stop->at,feet)) stop=&k;
            const world::roads::RoadGraph graph(verda_region);
            const Vector3 kerb=graph.nearest(stop->at).point;
            const std::string before=cb_backup.car();
            interaction_message=cb_backup.call(feet,match_time,kerb,stop->yaw,br_bots,bot_drivers,vehicles,verda_region);
            if(cb_backup.car()!=before && !cb_backup.car().empty()) {bot_drivers.reserve(cb_backup.car());vehicle_dirty=true;}
            br_bots.sync_bodies(npcs,&character_registry);
            interaction_remaining=4;
        }
        // BACKUP / Z: an officer radios the nearest station for two more.
        if(controls.backup && career.serving() && game_mode==game::GameMode::Explore && player_health.alive() && services.active()) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const game::services::Unit* station=nullptr;
            for(const auto& u:services.units())
                if(u.kind==game::services::UnitKind::Police && (!station || Vector3Distance(u.home,feet)<Vector3Distance(station->home,feet))) station=&u;
            if(station) {
                const std::string before=backup.car();
                interaction_message=backup.call(feet,match_time,station->home,station->home_yaw,br_bots,bot_drivers,vehicles,verda_region);
                if(backup.car()!=before && !backup.car().empty()) {bot_drivers.reserve(backup.car());vehicle_dirty=true;}
                br_bots.sync_bodies(npcs,&character_registry);
                interaction_remaining=4;
            }
        }
        // Handbrake on JUMP (Space), boost on SPRINT (Shift), parking brake on CROUCH, horn on H.
        // Throttle: W/S (move_y) or the touch pedals - BRAKE slows, then reverses.
        const float pedal_throttle=std::clamp(-controls.move_y+controls.gas-controls.brake_pedal,-1.0F,1.0F);
        game::vehicles::VehicleInput driving_input{pedal_throttle,-controls.move_x,controls.brake,controls.crouch,IsKeyDown(KEY_H),controls.sprint};
        // In a LAN match the world goes on for everyone, whatever menu this player has open.
        const bool vehicle_paused=lan_match ? creator_active : creator_active||inventory_open||shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()||poker_open||mode_session.terminal()||!IsWindowFocused()
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
                if(store_open) {
                    const auto action=store_ui.input(wardrobe_catalogue,screen_width,screen_height,IsWindowFocused());
                    auto& state=loot_session.economy();
                    const bool member=mafio.player_gang()==0;
                    if(action.buy_look) {
                        store_status=game::wardrobe::buy(state,wardrobe_catalogue,wardrobe_catalogue.looks()[static_cast<std::size_t>(store_ui.look())].id,member);
                        loot_session.changed();
                    }
                    if(action.buy_hat && store_ui.hat()>=0) {
                        store_status=game::wardrobe::buy(state,wardrobe_catalogue,wardrobe_catalogue.hats()[static_cast<std::size_t>(store_ui.hat())].id,member);
                        loot_session.changed();
                    }
                    if(action.no_hat && !state.hat.empty()) {game::wardrobe::take_off_hat(state);store_status="Hat off";loot_session.changed();}
                    if(action.close) {store_open=false;input_system.cancel_controls();}
                }
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
            if(game::rules_for(game_mode).civilian_life && penthouse.inside(loot_feet) && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open) {
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
            // The mansion on the parking garage's roof: buy it inside, sleep in the master bedroom.
            const bool in_mansion=garage_here && game::rules_for(game_mode).civilian_life && !vehicles.driver() && garage_site.in_mansion(loot_feet);
            if(in_mansion && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open) {
                const bool owned=game::economy::SkyMansion::owned(loot_session.economy());
                if(!was_in_mansion) {interaction_message=owned ? "Home: the mansion on Parkumejo Centra" : "The mansion on Parkumejo Centra - for sale, with its own parking bay";interaction_remaining=3;}
                const auto bed=garage_site.to_world(world::garage::bed);
                const bool by_bed=std::hypot(loot_feet.x-bed.x,loot_feet.z-bed.z)<2.4F && std::abs(loot_feet.y-bed.y)<1.5F;
                if(controls.interact && !home_used && loot_session.world().nearest(loot_feet,2.8F,reachable_pickup)<0) {
                    if(!owned) {
                        interaction_message=game::economy::SkyMansion::buy(loot_session.economy());loot_session.changed();
                        if(game::economy::SkyMansion::owned(loot_session.economy())) {gun_audio.kill_confirm();haptics.hit_given(true);}
                        interaction_remaining=5;home_used=true;
                    } else if(by_bed) {
                        const auto clock=island_life.clock();
                        const int days=clock.minute_of_day()>=7*60 ? clock.day()+1 : clock.day();
                        island_life.set_clock(game::life::WorldClock::at(days,7,0));
                        player_health.reset();health_seen=player_health.current();
                        interaction_message="Slept until morning - "+island_life.clock().label();interaction_remaining=4;home_used=true;
                    }
                }
            }
            was_in_mansion=in_mansion;
            // The hidden stash: USE by the holdall.
            if(stash && game::rules_for(game_mode).civilian_life && !vehicles.driver() && !game::economy::Stash::taken(loot_session.economy()) &&
               std::hypot(loot_feet.x-stash->at.x,loot_feet.z-stash->at.z)<1.8F && std::abs(loot_feet.y-stash->at.y)<1.5F &&
               controls.interact && !home_used && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open) {
                interaction_message=game::economy::Stash::take(loot_session.economy());interaction_remaining=6;home_used=true;
                loot_session.changed();gun_audio.kill_confirm();haptics.hit_given(true);
                furnish_island();   // the holdall goes with the player
            }
            // Fulmo at the race meet: the stakes, then the bet.
            if(street_race.ready() && game::rules_for(game_mode).civilian_life && !vehicles.driver() && controls.interact && !home_used &&
               !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open &&
               std::hypot(loot_feet.x-street_race.organiser().x,loot_feet.z-street_race.organiser().z)<game::racing::StreetRace::talk_range) {
                bool paid=false;
                const auto said=street_race.talk(loot_session.economy().money,paid);
                if(paid) {loot_session.economy().money-=game::racing::StreetRace::bet;loot_session.changed();}
                if(!said.empty()) {chief_line=said;chief_line_time=7;}
                home_used=true;input_system.player().interact=false;
            }
            // A word with a resident: what's going round, or the weather.
            if(game::rules_for(game_mode).civilian_life && !vehicles.driver() && controls.interact && !home_used && !island_life.empty() &&
               !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open &&
               vehicles.nearest(verda_region,loot_feet)<0 && loot_session.world().nearest(loot_feet,2.8F,reachable_pickup)<0)
                if(const auto* resident=island_life.nearest(loot_feet,2.6F)) {
                    const auto clock=island_life.clock();
                    const int day=clock.day();
                    int in=0;
                    const int feast=game::culture::next_feast(day,in);
                    std::string small;
                    switch((resident->id+day)%4) {
                        case 0: small=std::string(game::culture::feasts[static_cast<std::size_t>(feast)].name)+(in==1 ? " tomorrow" : " in "+std::to_string(in)+" days")+". My mother's baking already.";break;
                        case 1: small=clock.night() ? "Late to be out. Mind the ring road - the young ones race there." : "Fine day. Too fine. Something's bound to happen.";break;
                        case 2: {
                            const auto table=league.standings(clock.minutes);
                            small=table.empty() ? "Nothing much. Same as ever." : std::string(game::sports::teams[static_cast<std::size_t>(table.front().team)].name)+" are top of the Basballigo. Don't get me started.";
                            break;
                        }
                        default: small="Quiet, thank God. Long may it last.";break;
                    }
                    const std::string town=resident->settlement>=0 && resident->settlement<static_cast<int>(verda_region.settlements().size()) ?
                        verda_region.settlements()[static_cast<std::size_t>(resident->settlement)].id : std::string();
                    chief_line=resident->first_name+" "+resident->family_name+": \""+gossip.tell(clock.minutes,town,static_cast<std::uint32_t>(resident->id*31+day),small)+"\"";
                    chief_line_time=7;
                    home_used=true;input_system.player().interact=false;
                }
            // The ballparks' stands: USE to sit down and watch; USE (or walk off) to get up.
            if(game::rules_for(game_mode).civilian_life && !vehicles.driver()) {
                if(stand_seat.on) {
                    if(controls.interact || std::abs(controls.move_x)>.3F || std::abs(controls.move_y)>.3F || controls.jump) {
                        stand_seat.on=false;home_used=true;input_system.player().interact=false;
                        player.position={stand_seat.at.x,stand_seat.at.y+1.0F,stand_seat.at.z};player.vertical_velocity=0;
                    }
                } else if(controls.interact && !home_used) {
                    Vector3 seat{};float yaw=0;
                    if(stand_seat_near(loot_feet,seat,yaw)) {
                        stand_seat={true,seat,yaw};home_used=true;input_system.player().interact=false;
                        interaction_message="You take a seat in the stands. USE to get up";interaction_remaining=3;
                    }
                }
            } else stand_seat.on=false;
            // Its parking bay: the owner's; anyone else is told it's private.
            if(garage_here && game::rules_for(game_mode).civilian_life) {
                const auto* car=vehicles.driver();
                const bool in_bay=car && std::abs(car->speed)<.8F && garage_site.in_bay(car->position);
                if(in_bay && !was_in_bay) {
                    interaction_message=game::economy::SkyMansion::owned(loot_session.economy()) ? "Your own bay, at your own front door. It stays here till you're back"
                                                                                                  : "PRIVATA - the mansion's owner parks here";
                    interaction_remaining=4;
                }
                was_in_bay=in_bay;
            }
            // The homes for sale: buy the one you stand in; at home, sleep in your bed.
            home_here=nullptr;
            if(game::rules_for(game_mode).civilian_life && !vehicles.driver())
                for(const auto& home:homes_for_sale) if(game::economy::inside(home,loot_feet)) {home_here=&home;break;}
            // Uncle Ron's house, before he has handed it over, is just his.
            if(home_here && home_here->price<=0 && !game::economy::owns(loot_session.economy(),*home_here)) home_here=nullptr;
            if(home_here && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open && !home_used) {
                const bool owned=game::economy::owns(loot_session.economy(),*home_here);
                if(home_here!=was_in_home) {interaction_message=owned ? "Home: "+home_here->name : home_here->name+" - for sale";interaction_remaining=3;}
                if(controls.interact && loot_session.world().nearest(loot_feet,2.8F,reachable_pickup)<0) {
                    if(!owned) {
                        interaction_message=game::economy::buy(loot_session.economy(),*home_here);loot_session.changed();
                        if(game::economy::owns(loot_session.economy(),*home_here)) {
                            gun_audio.kill_confirm();haptics.hit_given(true);
                            furnish_island();rebuild_signs();   // the furniture moves in; the board comes down
                        }
                        interaction_remaining=4;home_used=true;
                    } else {
                        bool by_bed=false;
                        for(const auto& piece:game::interiors::furnish_home(game::economy::place_of(*home_here),home_here->centre.y))
                            if(piece.model.ends_with("bed_double.glb") && std::hypot(piece.at.x-loot_feet.x,piece.at.z-loot_feet.z)<2.4F) by_bed=true;
                        if(by_bed) {
                            const auto clock=island_life.clock();
                            const int days=clock.minute_of_day()>=7*60 ? clock.day()+1 : clock.day();
                            island_life.set_clock(game::life::WorldClock::at(days,7,0));
                            player_health.reset();health_seen=player_health.current();
                            interaction_message="Slept until morning - "+island_life.clock().label();interaction_remaining=4;home_used=true;
                        }
                    }
                }
            }
            was_in_home=home_here;
            if(controls.interact && !home_used && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !at_table && !vehicles.driver() && vehicles.nearest(verda_region,loot_feet)<0 &&
               prison_until<0 && booking.stage<0){
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

        // At the wheel, VIEW (V) swaps the chase camera for the cab view; on foot, third person
        // for first. On a touch screen, a tap on the speedometer does it at the wheel (no extra
        // button over the road).
        if (controls.toggle_view) {
            if(vehicles.driver()) vehicle_renderer.set_cab(!vehicle_renderer.cab());
            else player.third_person=!player.third_person;
        }
        if(vehicles.driver() && touch_ui) {
            static std::vector<int> touches_before;
            std::vector<int> touches_now;
            const Vector2 dial{screen_width*.42F,screen_height-92.0F};
            for(int i=0;i<GetTouchPointCount();++i) {
                const int id=GetTouchPointId(i);
                touches_now.push_back(id);
                if(std::find(touches_before.begin(),touches_before.end(),id)==touches_before.end() &&
                   Vector2Distance(GetTouchPosition(i),dial)<70) vehicle_renderer.set_cab(!vehicle_renderer.cab());
            }
            touches_before=std::move(touches_now);
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
            if(controls.sight && !driving && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open())) sight_mounts.cycle(weapons.selected(),sights_owned);
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
        const float speed = creator_flying ? 12.0F:player_health.downed() ? walk_speed * game::lan::SquadRevive::crawl : controls.crouch ? walk_speed * 0.45F : controls.aim ? walk_speed * 0.55F :
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
                    resolve_player_body(
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

        // Sitting in the stands at a ballpark: stay put (look round as you like).
        if(stand_seat.on) {player.position={stand_seat.at.x,stand_seat.at.y+.55F,stand_seat.at.z};player.vertical_velocity=0;player.grounded=true;}
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
        vehicle_audio.update(vehicles,verda_region,dt,vehicle_paused || environment_audio.muted(),environment_audio.volume(),&camera.position);
        {
            const bool radio_paused=vehicle_paused || pause_menu.is_open();
            if(!radio_paused) radio_clock+=dt;
            // What the breaks talk about: the island's hour, and La Verda Basballigo's latest result.
            int winner=-1,loser=-1,radio_hour=12;
            if(game_mode==game::GameMode::Explore && !island_life.empty()) {
                const double now=island_life.clock().minutes;
                radio_hour=static_cast<int>(island_life.clock().hour());
                double latest=-1;
                for(int i=0;i<static_cast<int>(league.fixtures().size());++i) {
                    const double pitch=league.first_pitch_at(i);
                    if(pitch>now) continue;
                    const auto& line=league.linescore(i);
                    if(pitch+line.length>now || pitch+line.length<latest) continue;
                    const auto& f=league.fixtures()[static_cast<std::size_t>(i)];
                    const int home=line.total(true),away=line.total(false);
                    if(home==away) continue;
                    latest=pitch+line.length;
                    winner=home>away ? f.home : f.away;loser=home>away ? f.away : f.home;
                }
            }
            car_radio.set_context(radio_hour,winner,loser);
            car_radio.update(vehicles.driver()!=nullptr && player_health.alive(),radio_clock,radio_paused ? 0.0F : dt);
            radio_audio.update(car_radio,radio_paused,environment_audio.muted() ? 0.0F : environment_audio.volume()*settings.radio_volume);
        }

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
        // Riding with the crew: in the passenger seat, the camera behind the car; out at the end.
        riding_now=false;
        if(const int ride=player_crew.ride_car().empty() ? -1 : vehicles.find(player_crew.ride_car());ride>=0) {
            const auto car=vehicles.vehicles()[static_cast<std::size_t>(ride)];
            const float a=car.yaw*DEG2RAD;
            const Vector3 passenger_side{-std::cos(a),0,std::sin(a)};
            if(player_crew.riding(bot_drivers) && player_health.alive()) {
                riding_now=true;
                player.position={car.position.x+passenger_side.x*.45F,car.position.y+1.2F,car.position.z+passenger_side.z*.45F};
                player.vertical_velocity=0;
            } else {
                Vector3 out{car.position.x+passenger_side.x*2.4F,0,car.position.z+passenger_side.z*2.4F};
                out.y=world::physics::WorldCollision::ground_height({out.x,car.position.y+2,out.z},car.position.y+2,verda_region);
                if(!std::isfinite(out.y)) out.y=world::terrain::TerrainHeight::sample(out.x,out.z);
                player.position=Vector3Add(out,{0,1,0});player.vertical_velocity=0;
                player_crew.end_ride(br_bots,bot_drivers,vehicles,verda_region);
                vehicle_renderer.reset_camera();
                if(player_health.alive()) {interaction_message="Here we are";interaction_remaining=3;}
            }
        } else if(!player_crew.ride_car().empty()) player_crew.end_ride(br_bots,bot_drivers,vehicles,verda_region);
        // On the bus: a seat halfway down, the camera behind the bus; off if it leaves the line.
        if(bus_ride>=0) {
            const bool ok=bus_ride<static_cast<int>(bus_line.buses().size()) && bus_line.buses()[static_cast<std::size_t>(bus_ride)].running &&
                          bus_line.buses()[static_cast<std::size_t>(bus_ride)].player && player_health.alive();
            if(ok) {
                const auto& bus=bus_line.buses()[static_cast<std::size_t>(bus_ride)];
                const float a=bus.yaw*DEG2RAD;
                player.position={bus.position.x-std::cos(a)*.6F-std::sin(a)*1.0F,bus.position.y+1.3F,bus.position.z+std::sin(a)*.6F-std::cos(a)*1.0F};
                player.vertical_velocity=0;
            } else {
                if(bus_ride<static_cast<int>(bus_line.buses().size())) {
                    const auto& bus=bus_line.buses()[static_cast<std::size_t>(bus_ride)];
                    bus_line.alight(bus_ride);
                    const float a=bus.yaw*DEG2RAD;
                    Vector3 at{bus.position.x-std::cos(a)*3.0F,0,bus.position.z+std::sin(a)*3.0F};
                    at.y=world::terrain::TerrainHeight::sample(at.x,at.z);
                    player.position=Vector3Add(at,{0,1,0});
                }
                bus_ride=-1;vehicle_renderer.reset_camera();
            }
        }
        {
            // A rig's trailer: the camera rides over it.
            float trailer=0;
            if(const auto* v=vehicles.driver())
                for(const auto& t:freight.trailers())
                    if(t.vehicle==v->id) trailer=Vector3Distance({t.centre.x,0,t.centre.z},{v->position.x,0,v->position.z})*1.6F;
            vehicle_renderer.set_trailer(trailer);
        }
        if(vehicles.driver())vehicle_renderer.camera(camera,vehicles,verda_region,dt,vehicle_orbit,vehicle_pitch,combat_world);
        else if(bus_ride>=0) vehicle_renderer.camera(camera,vehicles,verda_region,dt,vehicle_orbit,vehicle_pitch,combat_world,bus_line.buses()[static_cast<std::size_t>(bus_ride)].vehicle);
        else if(riding_now) vehicle_renderer.camera(camera,vehicles,verda_region,dt,vehicle_orbit,vehicle_pitch,combat_world,player_crew.ride_car());
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
        // The driver keeps both hands on the wheel: no shooting from the driver's seat (the crew in
        // the back does it - game::career::Crew - and a rig's roof gun is worked from the cab).
        const bool drive_by=false;
        // Riding with the crew, the player shoots out of the passenger window: any gun.
        const int ride_index=riding_now ? vehicles.find(player_crew.ride_car()) : -1;
        const bool ride_by=ride_index>=0 && weapons.available() && !creator_active;
        if(drive_by || ride_by) {
            const auto* car=drive_by ? vehicles.driver() : &vehicles.vehicles()[static_cast<std::size_t>(ride_index)];
            const Vector3 seat{car->position.x,car->position.y+1.35F,car->position.z};
            const Vector3 sight_from=Vector3Add(camera.position,Vector3Scale(camera_direction,Vector3Distance(camera.position,seat)+3.0F));
            const auto past_car=combat_world.trace_segment(sight_from,Vector3Add(camera.position,Vector3Scale(camera_direction,500.0F)),true,true,true);
            const Vector3 target=past_car.hit() ? past_car.position : Vector3Add(camera.position,Vector3Scale(camera_direction,500.0F));
            Vector3 out=Vector3Subtract(target,seat);out.y=0;
            out=Vector3Length(out)>.01F ? Vector3Normalize(out) : Vector3{0,0,1};
            gun_muzzle=Vector3Add(seat,Vector3Scale(out,2.8F));
            gun_direction=Vector3Normalize(Vector3Subtract(target,gun_muzzle));
        }
        // TAG / T: spray your gang's tag on the wall in front of you (a can of spray paint).
        if(controls.tag && !driving && !creator_active && !inventory_open && player_gang()>=0 && player_health.alive()) {
            const int gang=player_gang();
            // Level, where the player faces, at a few heights (a can reaches about arm's length plus a step).
            Vector3 level{look_direction.x,0,look_direction.z};
            level=Vector3Length(level)>.01F ? Vector3Normalize(level) : Vector3{std::sin(player.yaw),0,std::cos(player.yaw)};
            game::combat::BulletHit wall;
            for(const float h:{.55F,.15F,.95F}) {
                const Vector3 from{player.position.x,player.position.y+h,player.position.z};
                wall=combat_world.trace_segment(from,Vector3Add(from,Vector3Scale(level,4.5F)),false,false,false);
                if((wall.kind==game::combat::HitKind::Building || wall.kind==game::combat::HitKind::Structure) && std::abs(wall.normal.y)<.45F) break;
            }
            if(loot_session.inventory().count("spray_paint")<=0) interaction_message="No spray paint - general stores sell it, or find some";
            else if((wall.kind!=game::combat::HitKind::Building && wall.kind!=game::combat::HitKind::Structure) || std::abs(wall.normal.y)>.45F)
                interaction_message="Face a wall to tag it";
            else {
                const Vector3 face=Vector3DotProduct(wall.normal,Vector3Subtract(player.position,wall.position))<0 ? Vector3Negate(wall.normal) : wall.normal;
                const int turf=territory.tag(wall.position,face,gang);
                loot_session.edit_inventory().remove("spray_paint",1);loot_session.changed();
                sound_bus.emit(game::sound::SoundKind::Footstep,wall.position,12,0,match_time);
                if(turf<0) interaction_message="Tagged - but nobody out here cares";
                else {
                    const auto& t=territory.turfs()[static_cast<std::size_t>(turf)];
                    if(t.ranch>=0) interaction_message="Tagged the "+territory.families()[static_cast<std::size_t>(t.ranch)].name+" family's land";
                    else if(t.owner==gang) interaction_message=t.name+" is ours - the tag shores up our hold";
                    else {
                        interaction_message=TextFormat("Tagged %s's wall in %s - hold %d. Their patrols will find it",gang_name(t.owner).c_str(),t.name.c_str(),static_cast<int>(t.hold));
                        // Nobody saw you do it - yet: the town's street patrols will find it (below).
                        fresh_tags.push_back({wall.position,t.owner,gang,turf});
                    }
                    career.tagged(turf);
                }
            }
            interaction_remaining=3;
        }
        if (!controls.fire) trigger_ready=true;
        game::combat::WeaponInput weapon_input;
        weapon_input.fire=controls.fire && trigger_ready && (!driving || drive_by) && bus_ride<0 && !creator_active && !inventory_open;
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
        // The roof gun: fired from the cab, it shoots from the mount over the cab wherever the camera looks.
        {
            if(!roof_gun_ready) {roof_gun.reset(true);roof_gun.select(game::combat::WeaponId::Rifle);roof_gun_ready=true;}
            const auto* car=vehicles.driver();
            const auto* asset=car ? vehicles.asset(verda_region,car->id) : nullptr;
            roof_gun_on=asset && asset->vehicle.gun && !asset->vehicle.destroyed && !creator_active && !inventory_open && player_health.alive();
            if(roof_gun_on) {
                const auto* def=vehicles.definition(*asset);
                const Vector3 mount{car->position.x,car->position.y+(def ? def->height : 3.0F)+.45F,car->position.z};
                const Vector3 from=Vector3Add(camera.position,Vector3Scale(camera_direction,Vector3Distance(camera.position,mount)+4.0F));
                const Vector3 far=Vector3Add(camera.position,Vector3Scale(camera_direction,600.0F));
                const auto past=combat_world.trace_segment(from,far,true,true,true);
                const Vector3 target=past.hit() ? past.position : far;
                const Vector3 dir=Vector3Normalize(Vector3Subtract(target,mount));
                game::combat::WeaponInput fire;
                fire.fire=controls.fire && bus_ride<0;fire.aim=true;fire.grounded=true;
                roof_gun.update(vehicle_paused ? 0.0F : dt,fire,{Vector3Add(mount,Vector3Scale(dir,1.1F)),dir},combat_world);
                for(int shot=0;shot<roof_gun.events().shots;++shot) {
                    gun_audio.player_shot(game::combat::WeaponId::Rifle);
                    sound_bus.emit(game::sound::SoundKind::Gunshot,mount,game::ai::BattleRoyaleBots::gunshot_radius_rifle,0,match_time);
                }
                for(const auto& note:roof_gun.events().hits) combat_feedback.hit(note.position,note.damage,note.headshot);
            } else if(roof_gun_ready) roof_gun.update(dt,{},{},combat_world);   // its rounds still in the air fly on
        }
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
        if(controls.heal && !creator_active && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && player_health.alive() && !mode_session.terminal() &&
           game_mode!=game::GameMode::DevLab) {
            if(const auto said=loot_session.quick_heal(weapons,player_health); !said.empty()) {interaction_message=said;interaction_remaining=2.5F;}
        }
        if(!mode_session.terminal() && !vehicle_paused) loot_session.tick(dt,player_health);
        if(auto picked=loot_session.take_auto_message(); !picked.empty()) {interaction_message=picked;interaction_remaining=1.6F;}
        if(controls.grenade && !driving && !creator_active && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && player_health.alive() && !mode_session.terminal()) {
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
            const bool friendly=blast_index<grenades.exploded_friendly().size() && grenades.exploded_friendly()[blast_index];
            if(!driving && !friendly && near<reach && open(chest)) {
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
        for(const Vector3 where:pending_kills) {combat_feedback.kill(where);stats.kill();}
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
        // The outbreak's scenes, its cast holding their ground, and what the player comes across.
        if(outbreak.active()) {
            game::outbreak::RunFrame frame;
            frame.dt=vehicle_paused ? 0.0F : dt;
            frame.feet={player.position.x,player.position.y-1,player.position.z};
            frame.alive=player_health.alive();
            if(const auto* car=vehicles.driver()) frame.driving=car->id;
            const auto& happened=outbreak.update(frame,verda_region,npcs,vehicles,zombies);
            const Vector3 ear=Vector3Subtract(camera.target,camera.position);
            for(const auto& shot:happened.shots) {
                sound_bus.emit(game::sound::SoundKind::Gunshot,shot.from,game::sound::radius::pistol,-2,match_time);
                police_tracers.push_back({shot.from,shot.to,.09F});
                gun_audio.world_shot(static_cast<game::combat::WeaponId>(std::clamp(shot.weapon,0,static_cast<int>(game::combat::weapon_count)-1)),shot.from,camera.position,ear);
            }
            if(!happened.message.empty()) {interaction_message=happened.message;interaction_remaining=6;}
            if(happened.rifle) {
                auto& bag=loot_session.edit_inventory();
                if(bag.count("hunting_rifle")==0) bag.add(item_registry,"hunting_rifle",1);
                bag.add(item_registry,"rifle_ammo",30);loot_session.changed();
                gun_audio.kill_confirm();haptics.hit_given(true);
            }
            if(happened.save) {save_outbreak();loot_session.save(weapons);}
            if(outbreak.has_hideout() && (outbreak_autosave+=dt)>120) {outbreak_autosave=0;save_outbreak();}
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
        // Cars and people. A moving car that reaches someone on foot knocks them down and hurts them
        // (more the faster it goes, much more from a big rig). The player who does it stops and waits
        // for the ambulance - or it's a hit and run (game::law::HitAndRun). An officer running with
        // the lights on is exempt: people get out of the way of a police car with its lights on, and
        // one who doesn't is at fault. Somebody else's car that hits someone is news.
        if(game::rules_for(game_mode).civilian_life && !vehicle_paused && game_mode==game::GameMode::Explore) {
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            const auto* mine=vehicles.driver();
            const bool lights_on=mine && career.on_duty() && lights_car==mine->id;
            for(const auto& v:vehicles.vehicles()) {
                if(std::abs(v.speed)<3.5F || std::hypot(v.position.x-feet.x,v.position.z-feet.z)>160) continue;
                const auto* asset=vehicles.asset(verda_region,v.id);
                const auto* def=asset ? vehicles.definition(*asset) : nullptr;
                if(!def || asset->vehicle.destroyed) continue;
                const bool player_car=mine && mine->id==v.id;
                const float a=v.yaw*DEG2RAD;
                const Vector3 fwd{std::sin(a),0,std::cos(a)},right{std::cos(a),0,-std::sin(a)};
                const float half_l=def->length*.5F+.35F,half_w=def->width*.5F+.35F;
                const float heavy=def->length>9 ? 2.2F : 1.0F;   // a rig or a bus hits much harder
                // A police car running with its lights: the people ahead get out of the way.
                if(player_car && lights_on && std::abs(v.speed)>7)
                    for(const auto& actor:npcs.actors()) {
                        if(actor.resident<0 || actor.state==characters::NpcState::Dead || !actor.active) continue;
                        const Vector3 d{actor.position.x-v.position.x,0,actor.position.z-v.position.z};
                        const float along=d.x*fwd.x+d.z*fwd.z,side=d.x*right.x+d.z*right.z;
                        if(along<0 || along>std::abs(v.speed)*1.8F+half_l || std::abs(side)>half_w+2.5F) continue;
                        const float away=side>=0 ? 1.0F : -1.0F;
                        const Vector3 aside=Vector3Add(actor.position,Vector3Scale(right,away*(half_w+3.0F-std::abs(side))));
                        npcs.direct_resident(actor.resident,aside,1.0F,true);
                    }
                for(std::size_t i=0;i<npcs.actors().size();++i) {
                    const auto& actor=npcs.actors()[i];
                    if(!actor.active || actor.hidden || actor.state==characters::NpcState::Dead) continue;
                    if(actor.bot>=0 && (actor.bot>=910000 || br_bots.hidden(actor.bot) || br_bots.mounted(actor.bot))) continue;   // the island's named people stand clear; riders are in cars
                    const Vector3 d{actor.position.x-v.position.x,0,actor.position.z-v.position.z};
                    const float along=d.x*fwd.x+d.z*fwd.z,side=d.x*right.x+d.z*right.z;
                    if(std::abs(along)>half_l || std::abs(side)>half_w || std::abs(actor.position.y-v.position.y)>2.2F) continue;
                    const int key=actor.bot>=0 ? actor.bot : 2000000+static_cast<int>(i);
                    if(const auto hit=struck_at.find(key);hit!=struck_at.end() && match_time-hit->second<2.5) continue;
                    struck_at[key]=match_time;
                    const float amount=std::clamp((std::abs(v.speed)-2.5F)*7.0F*heavy,8.0F,220.0F);
                    if(actor.bot>=0) {
                        if(!player_car) continue;   // the gangs and the troops step out of each other's cars' way
                        if(mafio.active()) mafio.provoke(actor.bot,match_time);
                        if(br_bots.damage_bot(actor.bot,amount,v.position,match_time)) {
                            pending_kills.push_back(actor.position);
                            career.killed(faction_of(actor.bot),true);
                        }
                        continue;
                    }
                    npcs.damage(i,amount,v.position);
                    if(player_car) {
                        if(lights_on) {interaction_message="They didn't get out of the way of the lights - not your fault";interaction_remaining=3;}
                        else if(hit_and_run.struck(actor.position,false)) {
                            interaction_message="You hit someone! Stop and wait for the ambulance and the police - or it's a hit and run";interaction_remaining=5;
                        }
                    } else if(!services.siren(v.id) && !services.unit(v.id) && !vehicles.kinematic(v.id) && !services.pursued(v.id)) {
                        // Somebody else's car drove on: a hit and run, and the police go after it
                        // (a police car or an ambulance on a call is exempt; so is the bus on its line).
                        const bool chased=services.pursue(v.id,actor.position,vehicles);
                        if(match_time-npc_hit_news>25 || chased) {
                            npc_hit_news=match_time;
                            tell(chased ? "HIT AND RUN: a car hit someone and drove on - the police are after it" : "HIT AND RUN: a car hit someone and drove on",
                                 6,career.on_duty());
                        }
                    }
                }
            }
            // A hit pending: stopped (or on foot) near the scene for the ambulance, or gone.
            const bool stopped=!mine || std::abs(mine->speed)<1;
            switch(hit_and_run.update(feet,stopped,dt)) {
                case game::law::HitAndRun::Outcome::Stayed:
                    interaction_message="The ambulance has them, and the police took your statement. You stayed - no charges";interaction_remaining=5;
                    break;
                case game::law::HitAndRun::Outcome::Ran: {
                    // Somebody saw the car.
                    if(!island_life.empty()) {
                        const auto* a=mine ? vehicles.asset(verda_region,mine->id) : nullptr;
                        const auto* d=a ? vehicles.definition(*a) : nullptr;
                        const int paint=a && a->vehicle.painted ? a->vehicle.mods[0] : -1;
                        const std::string car=d ? game::culture::Gossip::car(paint>=0 && paint<static_cast<int>(game::autos::paints.size()) ? game::autos::paints[static_cast<std::size_t>(paint)].name : "",d->name) : "a car";
                        std::string town="the road";float best=1e30F;
                        for(const auto& s:verda_region.settlements()) {
                            if(s.id.starts_with("ranch_")) continue;
                            if(const float dd=std::hypot(s.center.x-feet.x,s.center.z-feet.z);dd<best) {best=dd;town=s.name;}
                        }
                        gossip.hear("somebody in "+car+" ran a man down in "+town+" and never stopped",island_life.clock().minutes,"",true);
                    }
                    game::law::JusticeFrame tip;tip.now=match_time;tip.player=feet;
                    justice.tip_off(1,hit_and_run.scene(),tip,island_life,"HIT AND RUN - a pedestrian down and the driver gone. Busted, it's prison");
                    career.crime(false);
                    break;
                }
                default: break;
            }
            if(hit_and_run.wanted() && justice.wanted()==0) hit_and_run.lost_them();
            // A loaded rig on the road draws the gangs: now and then a carload comes to hijack it.
            const auto* rig_asset=mine ? vehicles.asset(verda_region,mine->id) : nullptr;
            const bool loaded=rig_asset && rig_asset->vehicle.definition=="semi" && freight.hitched(mine->id) && mafio.active() && !career.on_duty() &&
                (trucking.job().stage==game::freight::HaulStage::Deliver || (career.job().kind==game::career::JobKind::Contraband && career.job().taken));
            if(!loaded) next_hijack=-1;
            else {
                static const bool dev_hijack=[]{const char* v=std::getenv("OUTLAND_DEV_HIJACK");return v && *v=='1';}();
                if(next_hijack<0) next_hijack=match_time+(dev_hijack ? 15.0 : 240.0+static_cast<double>(GetRandomValue(0,240)));
                if(match_time>=next_hijack) {
                    next_hijack=-1;
                    if(dev_hijack || GetRandomValue(0,99)<55)
                        if(const int gang=mafio.send_hijackers(mine->position,player_gang(),br_bots,bot_drivers,vehicles,verda_region,match_time);gang>=0) {
                            gang_news="HIJACKERS: "+gang_name(gang)+" want your load - a car is coming";gang_news_time=8;
                        }
                }
            }
        }
        if(game::rules_for(game_mode).civilian_life && !island_life.empty()) {
            game::law::JusticeFrame frame;
            frame.dt=vehicle_paused ? 0.0F : dt;frame.now=match_time;
            frame.player={player.position.x,player.position.y-1.0F,player.position.z};
            frame.player_alive=player_health.alive();frame.player_fired=weapons.events().shots>0;
            frame.player_armed=weapons.available();frame.player_driving=vehicles.driver()!=nullptr;
            frame.daylight=last_daylight;
            frame.line_of_sight=[&](Vector3 a,Vector3 b){return !combat_world.trace_segment(a,b,false,false,true).hit();};
            // The Krimuloj paid the police off: shooting on their turf, or in a fight with a gang, is
            // nobody's business.
            bool gang_business=mafio.active() && mafio.turf_at(frame.player,60)>=0;
            for(int g=0;g<static_cast<int>(game::mafia::gangs.size());++g) gang_business|=mafio.provoked(g,match_time);
            gang_business|=endgame.fighting();   // a siege or a war on the occupiers: nobody calls the police over it
            if(frame.player_fired && !gang_business && !career.on_duty()) justice.crime(game::law::CrimeKind::ShotsFired,frame.player,-1,frame,island_life);
            // Your crew runs this town: the more of Verda it runs, the more often nobody saw a thing.
            if(const int mine=player_gang();mine>=0 && !crimes.empty())
                if(const int here=territory.turf_at(frame.player);here>=0) {
                    const auto& t=territory.turfs()[static_cast<std::size_t>(here)];
                    const float share=territory.balance(mine).crime;
                    if(t.ranch<0 && !t.cleared && t.owner==mine && static_cast<float>(GetRandomValue(0,999))<share*1000) {
                        crimes.clear();
                        interaction_message="Your crew's town - nobody saw a thing";interaction_remaining=3;
                    }
                }
            for(const auto& c:crimes) {
                // An officer who hurts the public (or another officer) is off the force - but a
                // bystander caught in a gunfight with a gang is crossfire, not a crime (game::career::Career::stray_round).
                const bool harm=c.kind==game::law::CrimeKind::Assault || c.kind==game::law::CrimeKind::Murder;
                if(harm && career.on_duty() && gang_business) {career.stray_round(c.kind==game::law::CrimeKind::Murder);continue;}
                career.crime(harm || c.kind==game::law::CrimeKind::AttackOnPolice);
                // Officers and paramedics from the services saw what happened to them.
                if(game::services::EmergencyServices::crew_member(c.victim)) justice.witnessed(c.kind,frame.player,frame,island_life);
                else justice.crime(c.kind,frame.player,c.victim,frame,island_life);
            }
            frame.patrol_officers=services.officers_on_foot(npcs);
            if(!vehicle_paused) justice.update(frame,island_life,npcs);
            // Caught (one star, an officer at arm's length): off to the state prison.
            bool busted=justice.arrested();
#ifdef OUTLAND_DEV_TOOLS
            // OUTLAND_DEV_JAIL=1: arrested a few seconds in (to look at the prison).
            if(static bool jailed_once=false;!jailed_once && match_time>4 && std::getenv("OUTLAND_DEV_JAIL") && *std::getenv("OUTLAND_DEV_JAIL")) {jailed_once=true;busted=true;}
#endif
            if(busted && game_mode==game::GameMode::Explore && !career_path.empty() && prison_until<0 && booking.stage<0 && player_health.alive()) {
                // Busted: cuffed, into the back of a police car, and off to the state prison (a fade),
                // then the booking photo. The sentence starts once the photo is taken.
                booking={0,0,career.jailed()+hit_and_run.busted()};   // a hit and run is mandatory time
                stats.busted();
                if(backup.active()) backup.dismiss(br_bots,match_time);
                Vector3 exit{};
                if(vehicles.driver()) vehicles.exit(verda_region,exit,true);
                weapons.holster();
                in_shu=false;inside_lines.clear();inside_line=0;
                interaction_message="BUSTED - cuffed and in the back of a Policio car";interaction_remaining=3;
            }
            if(booking.stage>=0 && booking.stage<4) {
                booking.t+=dt;
                if(booking.stage==0 && booking.t>=1.4F) {
                    // Black: the ride to Ŝtata Malliberejo. Into the cell.
                    const Vector3 cell=prison_cell(false);
                    player.position={cell.x,cell.y+1,cell.z};player.vertical_velocity=0;
                    player.yaw=(world::layout::prison_yaw+90)*DEG2RAD;
                    booking.stage=1;booking.t=0;
                } else if(booking.stage==1 && booking.t>=2.8F) {booking.stage=2;booking.t=0;}
                else if(booking.stage==2 && booking.t>=1.2F && (controls.interact || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_ENTER))) {
                    // The photo is taken: the sentence starts, and a guard has a word for the new inmate.
                    input_system.player().interact=false;
                    prison_until=match_time+booking.sentence;
                    booking.stage=-1;
                    inside_lines.clear();inside_line=0;
                    using game::campaign::Line;
                    if(!career.state().inside_told) {
                        inside_lines.push_back(Line{"Guard","Welcome to Ŝtata Malliberejo. That's your cell. Your bunk's on the right."});
                        inside_lines.push_back(Line{"Guard","Two ways to do your time. Lie down on the bunk (USE) and sleep it off - you wake up at the gate."});
                        inside_lines.push_back(Line{"Guard","Or walk the block and the yard till your number's up. The gate's watched. Behave."});
                        inside_lines.push_back(Line{"Guard","Start trouble and you go to the SHU - solitary. Nothing in there but a bunk and the walls."});
                        career.inside_told();
                    }
                    if(career.state().side==game::career::Side::Gang) {
                        if(prison_yard.mark_snitch()>=0) {
                            const std::string gang(game::mafia::gangs[static_cast<std::size_t>(std::max(0,career.state().gang))].name);
                            inside_lines.push_back(Line{"A cellmate","Word from "+gang+": there's a snitch in the yard - he talked to the Policio. He's marked."});
                            inside_lines.push_back(Line{"A cellmate","You've got a shank in your sock. Put in work for the gang (USE on him) - or keep your head down. Your call."});
                        }
                    }
                }
                if(booking.stage>=0) {player.vertical_velocity=0;input_system.cancel_controls();}
            }
            // The guard's word, and the gang's: USE for the next line.
            if(inside_line<inside_lines.size() && booking.stage<0 && controls.interact) {
                ++inside_line;input_system.player().interact=false;
            }
            if(prison_until>=0) {
                const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
                if(!player_health.alive()) prison_until=-1;
                else if(match_time>=prison_until) {
                    // Out the gate, free.
                    const Vector3 out=prison_at(0,-46);
                    player.position={out.x,out.y+1,out.z};player.vertical_velocity=0;
                    prison_until=-1;
                    career.released();
                    interaction_message=career.message();interaction_remaining=5;
                    // The campaign: Uncle Ron comes for the player - the first time, and only then.
                    if(!campaign_path.empty() && story.stage()==game::campaign::Stage::Free && !campaign.kicked_out) {
                        if(!campaign.picked_up) {
                            campaign.picked_up=true;
                            const bool gang=career.state().side==game::career::Side::Gang;
                            const Vector3 ron=floor_at(prison_at(2.2F,-48));
                            place_ron(ron,facing(ron,out));
                            park_ron_truck(floor_at(prison_at(-4,-52)),world::layout::prison_yaw+90);
                            const auto& pickup=game::campaign::Story::pickup_lines(gang);
                            ron_pending.assign(pickup.begin(),pickup.end());ron_pending_home=true;
                            if(gang) {campaign.ultimatum=true;campaign.ultimatum_left=game::campaign::Story::ultimatum_seconds;}
                            interaction_message="Uncle Ron is waiting at the gate";interaction_remaining=4;
                        } else {
                            interaction_message="Nobody at the gate this time. Uncle Ron said he wouldn't come again";interaction_remaining=5;
                        }
                    }
                } else if(booking.stage!=4 && (!inside_prison(feet) || vehicles.driver() || (in_shu && Vector3Distance(feet,prison_cell(true))>3.4F))) {
                    Vector3 exit{};
                    if(vehicles.driver()) vehicles.exit(verda_region,exit,true);
                    const Vector3 cell=prison_cell(in_shu);
                    player.position={cell.x,cell.y+1,cell.z};player.vertical_velocity=0;
                    interaction_message="Back inside, inmate - you go when your time is done";interaction_remaining=3;
                } else if(inside_line>=inside_lines.size() && sleep_fade<0 && controls.interact) {
                    // The bunk: sleep off the sentence. The snitch: a shank, and the SHU.
                    const int snitch=prison_yard.snitch();
                    const auto* mark=snitch>=0 ? prison_yard.find(snitch) : nullptr;
                    if(Vector3Distance(feet,prison_bunk(in_shu))<2.2F) {
                        sleep_fade=0;input_system.player().interact=false;
                    } else if(mark && career.state().side==game::career::Side::Gang && !in_shu &&
                              Vector3Distance(feet,prison_yard.world(*mark))<2.6F) {
                        input_system.player().interact=false;
                        npcs.set_bot(snitch,prison_yard.world(*mark),prison_yard.world_yaw(*mark),0,characters::NpcState::Dead);
                        prison_yard.remove(snitch);
                        gun_audio.kill_confirm();
                        career.shanked_snitch();
                        interaction_message=career.message();interaction_remaining=6;
                        // The guards drag the player off to the SHU (a fade), a little more time on top.
                        in_shu=true;prison_until+=30;
                        booking={0,0,0};booking.stage=4;
                    }
                }
#ifdef OUTLAND_DEV_TOOLS
                // OUTLAND_DEV_SHANK=1: once the talk is done, beside the snitch (to look at the shanking and the SHU).
                if(const char* dev=std::getenv("OUTLAND_DEV_SHANK");dev && *dev && inside_line>=inside_lines.size() && !in_shu)
                    if(static bool moved=false;!moved) if(const auto* mark=prison_yard.snitch()>=0 ? prison_yard.find(prison_yard.snitch()) : nullptr) {
                        const Vector3 w=prison_yard.world(*mark);
                        player.position={w.x+1.2F,w.y+1.2F,w.z};player.vertical_velocity=0;player.yaw=-PI/2;moved=true;
                    }
#endif
                if(sleep_fade>=0) {
                    sleep_fade+=dt;
                    input_system.cancel_controls();
                    if(sleep_fade>=2.2F) {prison_until=match_time;sleep_fade=-1;}   // woken at the gate
                }
                weapons.holster();   // no guns inside
                // The snitch stops to see what the new man wants.
                if(const int snitch=prison_yard.snitch();snitch>=0 && career.state().side==game::career::Side::Gang)
                    if(const auto* mark=prison_yard.find(snitch);mark && Vector3Distance(feet,prison_yard.world(*mark))<6) prison_yard.pause(snitch,2);
            }
            if(prison_until<0) {in_shu=false;sleep_fade=-1;}
            // Dragged to the SHU: a short fade, then the solitary cell.
            if(booking.stage==4) {
                booking.t+=dt;
                input_system.cancel_controls();
                if(booking.t>=1.3F) {
                    const Vector3 cell=prison_cell(true);
                    player.position={cell.x,cell.y+1,cell.z};player.vertical_velocity=0;
                    player.yaw=world::layout::prison_yaw*DEG2RAD;
                    booking.stage=-1;
                    inside_lines.assign({game::campaign::Line{"Guard","The SHU. Nobody comes, nobody goes. Lie down on that bunk (USE) and sleep out your time."}});
                    inside_line=0;
                }
            }
            if(services.active() && !vehicle_paused) {
                game::services::ServicesFrame call;
                call.dt=dt;call.now=match_time;call.player=frame.player;
                call.player_velocity=dt>0 ? Vector3Scale(Vector3Subtract(frame.player,last_player_feet),1.0F/dt) : Vector3{};
                call.player_alive=player_health.alive();call.player_driving=driving;
                call.wanted=justice.wanted();call.last_known=justice.last_known();
                call.sight=frame.line_of_sight;
                services.update(call,vehicles,verda_region,npcs,&island_life);
                if(!services.events().message.empty()) {interaction_message=services.events().message;interaction_remaining=3;}
                // A hit-and-run driver pulled over and taken in: the car goes to the pound.
                // The driver goes to Ŝtata Malliberejo for a week.
                for(const auto& car:services.events().arrested) {
                    const auto taken=island_life.take_car(car);
                    vehicles.hold(car,true);
                    const int resident=taken.occupied ? taken.owner : -1;
                    std::string who="the driver";
                    if(resident>=0) {
                        npcs.despawn_resident(resident);
                        island_life.imprison(resident,prison_intake,island_life.clock().minutes+game::law::JailTerms::week);
                        who=island_life.residents()[static_cast<std::size_t>(resident)].full_name();
                    }
                    const int inmate=prison_yard.admit(false,prison_body);
                    if(inmate>=0) for(const auto& p:prison_yard.people()) if(p.bot==inmate) prison_spawn(p);
                    jail_terms.book(inmate,resident,island_life.clock().minutes);
                    tell("HIT AND RUN: the police caught "+who+" - a week in Ŝtata Malliberejo",7,career.on_duty());
                }
                if(services.events().backup)   // a Ŝosea Patrolo cruiser found you: the Policio sends more
                    justice.tip_off(std::min(5,justice.wanted()+1),frame.player,frame,island_life,"The Ŝosea Patrolo radioed the Policio for backup");
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
                    if(net::decode(message.payload,hit) && player_health.alive() && !mode_session.terminal() &&
                       !(hit.attacker>=0 && teammate(static_cast<std::uint8_t>(hit.attacker)))) {
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
                    if(grenade.rocket) grenades.launch_rocket(grenade.origin,grenade.velocity,true,teammate(from));
                    else grenades.spawn_remote(grenade.origin,grenade.velocity,teammate(from));
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
                frame.player_alive=player_health.alive() && !player_health.downed();
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
                if(const auto* car=vehicles.driver()) {
                    const auto* a=vehicles.asset(verda_region,car->id);
                    frame.player_police_car=a && (a->vehicle.definition=="police" || a->vehicle.definition=="interceptor" || a->vehicle.definition=="highway_patrol");
                    frame.player_lights=frame.player_police_car && lights_car==car->id;
                }
                if(!player_health.alive()) mafio.forgive();
                // The street patrols: one that walks past a fresh tag of the player's on its gang's
                // wall finds it. The first: the gang hunts the player. After that: war.
                for(std::size_t i=0;i<fresh_tags.size();) {
                    const auto f=fresh_tags[i];
                    if(f.owner<0 || f.owner>=game::mafia::gang_count || mafio.spotted_by_patrol(f.at,f.owner,br_bots)<0) {++i;continue;}
                    fresh_tags.erase(fresh_tags.begin()+static_cast<std::ptrdiff_t>(i));
                    const std::string town=f.turf>=0 && static_cast<std::size_t>(f.turf)<territory.turfs().size() ? territory.turfs()[static_cast<std::size_t>(f.turf)].name : std::string("town");
                    if(++tags_found[static_cast<std::size_t>(f.owner)]==1) {
                        mafio.provoke_gang(f.owner,match_time,gang_name(f.owner)+"'s patrol found your tag in "+town+" - they're out looking for you");
                        continue;
                    }
                    mafio.provoke_gang(f.owner,match_time,gang_name(f.owner)+"'s patrol found another of your tags in "+town+" - it's war");
                    int from=-1,to=-1;float near_from=1e9F,near_to=1e9F;
                    for(std::size_t c=0;c<mafio.crews().size();++c) {
                        const auto& crew=mafio.crews()[c];
                        const float d=Vector3Distance(crew.door,f.at);
                        if(crew.gang==f.owner && !crew.disbanded && d<near_from) {near_from=d;from=static_cast<int>(c);}
                        if(crew.gang==f.tagger && !crew.disbanded && d<near_to) {near_to=d;to=static_cast<int>(c);}
                    }
                    if(from>=0 && to>=0) mafio.send_hit_squad(from,to,br_bots,bot_drivers,vehicles,verda_region,match_time);
                }
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
                // A CLEAN UP under way: that crew's dead stay down, its squads come home.
                mafio.besiege(career_path.empty() ? -1 : career.besieging(),br_bots,frame);
                mafio.update(frame,br_bots,bot_world,bot_drivers,vehicles,verda_region);
                // The player's own crew: keep up, ride along, come along for the ride.
                {
                    if(!career.on_duty() && player_crew.partner()>=0) player_crew.dismiss_partner(br_bots);
                    if(career.state().side!=game::career::Side::Gang && !player_crew.members().empty())
                        for(const int id:player_crew.dismiss(br_bots)) mafio.lend(id,false,br_bots,feet);
                    game::career::Crew::Frame cf;
                    cf.player=feet;cf.alive=player_health.alive();cf.riding=player_crew.riding(bot_drivers);
                    if(const auto* car=vehicles.driver()) {cf.driving=car->id;cf.car=car->position;cf.car_yaw=car->yaw;}
                    std::string said;
                    for(const int id:player_crew.update(cf,br_bots,verda_region,said)) mafio.lend(id,false,br_bots,feet);
                    if(!said.empty()) {interaction_message=said;interaction_remaining=3;}
                    if(!player_health.alive() && player_crew.size()>0)
                        for(const int id:player_crew.dismiss(br_bots)) mafio.lend(id,false,br_bots,feet);
                }
                // The brothers on the CB: the same, for a trucker.
                if(cb_backup.active()) {
                    if(!career.trucker()) cb_backup.dismiss(br_bots,match_time);
                    bool fighting=false;
                    for(const auto& m:mafio.members()) {
                        if(fighting) break;
                        const auto* bot=br_bots.find(m.bot);
                        fighting=bot && bot->alive() && !br_bots.hidden(m.bot) && std::hypot(bot->position().x-feet.x,bot->position().z-feet.z)<60;
                    }
                    if(const auto said=cb_backup.update(feet,match_time,player_health.alive(),fighting,br_bots,bot_drivers,vehicles,verda_region);!said.empty()) {
                        interaction_message=said;interaction_remaining=3;
                    }
                }
                // A drive-by with the crew in the back: close to the target crew they open up, and what
                // they bring down counts for the job (the driver drives).
                if(const auto& job=career.job();job.kind==game::career::JobKind::DriveBy && vehicles.driver() && !player_crew.aboard_now().empty()) {
                    const Vector3 car=vehicles.driver()->position;
                    if(std::hypot(car.x-job.target.x,car.z-job.target.z)<45 && !mafio.provoked(job.gang,match_time))
                        mafio.provoke_gang(job.gang,match_time,"Your crew opens up from the car - "+gang_name(job.gang)+" shoot back");
                    if(drive_by_alive.size()!=mafio.members().size()) drive_by_alive.assign(mafio.members().size(),1);
                    for(std::size_t i=0;i<mafio.members().size();++i) {
                        const auto& m=mafio.members()[i];
                        const auto* bot=br_bots.find(m.bot);
                        const bool alive=bot && bot->alive();
                        if(drive_by_alive[i] && !alive && mafio.gang_of(m.bot)==job.gang && bot &&
                           std::hypot(bot->position().x-car.x,bot->position().z-car.z)<70) career.killed(job.gang,true);
                        drive_by_alive[i]=alive;
                    }
                } else drive_by_alive.clear();
                // The officer's backup: arrive, keep close, go home.
                if(backup.active()) {
                    if(!career.serving()) backup.dismiss(br_bots,match_time);
                    bool fighting=false;
                    for(const auto& m:mafio.members()) {
                        if(fighting) break;
                        const auto* bot=br_bots.find(m.bot);
                        fighting=bot && bot->alive() && !br_bots.hidden(m.bot) && std::hypot(bot->position().x-feet.x,bot->position().z-feet.z)<60;
                    }
                    if(const auto said=backup.update(feet,match_time,player_health.alive(),fighting,br_bots,bot_drivers,vehicles,verda_region);!said.empty()) {
                        interaction_message=said;interaction_remaining=3;
                    }
                }
                // The career: what the world holds for a job, and what the player did.
                if(!career_path.empty()) {
                    game::career::CareerFrame work;
                    work.now=match_time;work.player=feet;work.alive=player_health.alive();
                    if(const auto* car=vehicles.driver()) {
                        work.driving=car->id;
                        const auto* asset=vehicles.asset(verda_region,car->id);
                        work.driving_police=asset && (asset->vehicle.definition=="police" || asset->vehicle.definition=="interceptor" ||
                                                      asset->vehicle.definition=="highway_patrol");
                    }
                    for(const auto& g:game::mafia::gangs) work.gang_names.emplace_back(g.name);
                    for(std::size_t c=0;c<mafio.crews().size();++c) {
                        const auto& crew=mafio.crews()[c];
                        int alive=0;
                        for(const int id:crew.members) if(const auto* bot=br_bots.find(id);bot && bot->alive()) ++alive;
                        work.crews.push_back({static_cast<int>(c),crew.gang,crew.door,crew.town,alive,crew.casino,crew.disbanded});
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
                    for(const auto& town:verda_region.settlements()) if(!town.id.starts_with("ranch_")) work.towns.push_back({town.name,town.center});
                    work.chief=chief.at;work.chief_known=chief.placed;
                    work.crew_share=territory.balance(player_gang()).crime;
                    work.camp=police_camp.line();work.camp_known=police_camp.placed();
                    // The Ŝosea Patrolo's next outpost; the depot gate and the freight yards for the truckers.
                    if(patrol_open<static_cast<int>(patrol_posts.size())) {work.next_post=patrol_posts[static_cast<std::size_t>(patrol_open)].at;work.post_known=true;}
                    work.driving_rig=(game::freight::Trucking::company_rig(work.driving) || (!work.driving.empty() && work.driving==trucking.own_rig())) &&
                                     freight.hitched(work.driving);
                    work.depot=haul_world.depot;work.depot_known=depot_boss.placed;
                    // A strike: the big rig the player brought (the last one driven, its trailer on
                    // or not) standing in the gate, the player at its wheel or on the picket line
                    // beside it. (The rigs parked in the truck park don't count.)
                    if(const auto* d=vehicles.driver())
                        if(const auto* a=vehicles.asset(verda_region,d->id);a && a->vehicle.definition=="semi") strike_rig=d->id;
                    if(career.strike_on() && depot_boss.placed && !strike_rig.empty() &&
                       std::hypot(player.position.x-haul_world.depot.x,player.position.z-haul_world.depot.z)<strike_picket_reach)
                        if(const auto* a=vehicles.asset(verda_region,strike_rig);a && !a->vehicle.destroyed &&
                           std::hypot(a->position.x-haul_world.depot.x,a->position.z-haul_world.depot.z)<strike_gate_reach)
                            work.rig_at_gate=true;
                    for(const auto& t:haul_world.towns) if(t.name!="Verda") work.yards.push_back({t.name,t.point});
                    // The Chief stands at his post; the range runs its drill (counting the shots fired).
                    if(chief.placed) npcs.set_bot(chief_bot,chief.at,chief.yaw,100,characters::NpcState::Idle);
                    for(std::size_t i=0;i<kamionistoj.size();++i)
                        npcs.set_bot(kamionisto_bot+static_cast<int>(i),kamionistoj[i].at,kamionistoj[i].yaw,100,characters::NpcState::Idle);
                    // The picket line: out at the gate while a strike is on, signs planted beside them.
                    {
                        const bool picket=picket_placed && career.strike_on() &&
                                          std::hypot(player.position.x-haul_world.depot.x,player.position.z-haul_world.depot.z)<300;
                        if(picket!=picket_out) {
                            picket_out=picket;
                            auto signage=sign_renderer.signage();
                            std::erase_if(signage.signs,[](const game::culture::Sign& sign){return sign.place.starts_with("picket_");});
                            for(int k=0;k<picket_size;++k) {
                                npcs.set_bot_hidden(picket_bot+k,!picket);
                                if(!picket) continue;
                                const auto& at=picket_spots[static_cast<std::size_t>(k)];
                                const float yaw=picket_yaws[static_cast<std::size_t>(k)]*DEG2RAD;
                                const Vector3 aside{std::cos(yaw)*.8F,0,-std::sin(yaw)*.8F};   // at his right hand
                                game::culture::Sign sign;
                                sign.kind=game::culture::SignKind::Homemade;
                                sign.place="picket_"+std::to_string(k);
                                sign.position={at.x+aside.x,at.y+2.05F,at.z+aside.z};
                                sign.yaw=picket_yaws[static_cast<std::size_t>(k)];
                                sign.max_width=1.1F;
                                static constexpr const char* words[picket_size][2]{{"STRIKO!","LA FRATECO"},{"JUSTA SALAJRO","POR ĈIU ŜARĜO"},
                                                                                   {"STRIKO!","NI NE MOVIĜOS"},{"KAMIONISTOJ","KUNE"}};
                                sign.front={{words[k][0],.2F},{words[k][1],.12F}};
                                sign.back=sign.front;
                                sign.board=Color{240,236,226,255};sign.ink=Color{176,28,30,255};
                                sign.post=1.65F;
                                signage.signs.push_back(std::move(sign));
                            }
                            sign_renderer.set(std::move(signage));
                        }
                        if(picket_out)
                            for(int k=0;k<picket_size;++k)
                                npcs.set_bot(picket_bot+k,picket_spots[static_cast<std::size_t>(k)],picket_yaws[static_cast<std::size_t>(k)],100,characters::NpcState::Idle);
                    }
                    for(const auto& g:gun_men) npcs.set_bot(gun_man_bot+g.gang,g.at,g.yaw,100,characters::NpcState::Idle);
                    // A secure Verda: the last of the gangs gone, the Green Hand's bar opens as a cultural society.
                    static const bool dev_secure=[]{const char* v=std::getenv("OUTLAND_DEV_SECURE");return v && *v=='1';}();   // dev: the society opens at once
                    if(!society.open && mafio.active() && !mafio.crews().empty() &&
                       (dev_secure || std::all_of(mafio.crews().begin(),mafio.crews().end(),[](const game::mafia::Crew& c){return c.disbanded;})))
                        if(const auto* home=mafio.casino()) {
                            Vector3 out=Vector3Subtract(home->door,home->interior);out.y=0;
                            out=Vector3Length(out)>.1F ? Vector3Normalize(out) : Vector3{0,0,1};
                            society={true,Vector3Add(home->door,Vector3Scale(out,2.0F)),std::atan2(out.x,out.z)*RAD2DEG};
                            const Vector3 side{-out.z,0,out.x};
                            for(int k=0;k<society_size;++k) {
                                const Vector3 at=Vector3Add(society.at,Vector3Scale(side,(static_cast<float>(k)-1.5F)*1.3F));
                                const float top=at.y+2;
                                npcs.spawn_bot(society_bot+k,outfit_body,{at.x,world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region),at.z},society.yaw);
                            }
                            TraceLog(LOG_INFO,"OUTLAND La Verda Mano Kultura Societo opens at %.0f %.0f",society.at.x,society.at.z);
                        }
                    if(society.open) {
                        Vector3 out{std::sin(society.yaw*DEG2RAD),0,std::cos(society.yaw*DEG2RAD)};
                        const Vector3 side{-out.z,0,out.x};
                        for(int k=0;k<society_size;++k) {
                            const Vector3 at=Vector3Add(society.at,Vector3Scale(side,(static_cast<float>(k)-1.5F)*1.3F));
                            const float top=at.y+2;
                            npcs.set_bot(society_bot+k,{at.x,world::physics::WorldCollision::ground_height({at.x,top,at.z},top,verda_region),at.z},society.yaw,100,characters::NpcState::Idle);
                        }
                    }
                    // The campaign: the story moves on (the drive home, the house handed over), Uncle Ron
                    // turns to the player when he's close, and the game saves itself now and then.
                    if(!campaign_path.empty()) {
                        campaign.played+=dt;
                        // Uncle Ron and the road the player takes.
                        if(story.stage()==game::campaign::Stage::Free) {
                            using game::campaign::Path;
                            const Path path=career.on_duty() ? Path::Cop : career.state().side==game::career::Side::Gang ? Path::Gang :
                                            trucking.hired() ? Path::Trucker : Path::None;
                            const auto react=[&](bool& heard,Path p,const char* phone) {
                                if(path!=p || heard) return;
                                heard=true;
                                const auto& lines=game::campaign::Story::reaction_lines(p);
                                if(!ron_pending_home) ron_pending.assign(lines.begin(),lines.end());
                                chief_line=std::string("Uncle Ron (phone): ")+phone;chief_line_time=7;
                            };
                            react(campaign.heard_trucker,Path::Trucker,"\"Verda Transporto? Come home - I want to hear everything!\"");
                            react(campaign.heard_cop,Path::Cop,"\"THE POLICIO?! Come home this minute, I have to see you!\"");
                            react(campaign.heard_gang,Path::Gang,"\"...Dario called. Come home. We need to talk.\"");
                            // The ultimatum: out of the gang in time, or out of his house.
                            if(campaign.ultimatum && !campaign.kicked_out) {
                                if(path!=Path::Gang) {
                                    campaign.ultimatum=false;
                                    chief_line="Uncle Ron (phone): \"You left them? ...Thank you, nevo. Thank you.\"";chief_line_time=7;
                                } else if((campaign.ultimatum_left-=dt)<=0) {
                                    campaign.ultimatum=false;campaign.kicked_out=true;
                                    if(ron_home>=0 && ron_home<static_cast<int>(homes_for_sale.size())) {
                                        auto& owned=loot_session.economy().properties;
                                        std::erase(owned,homes_for_sale[static_cast<std::size_t>(ron_home)].id);
                                        loot_session.changed();furnish_island();rebuild_signs();
                                    }
                                    const auto& lines=game::campaign::Story::kicked_out_lines();
                                    if(!ron_pending_home) ron_pending.assign(lines.begin(),lines.end());
                                    chief_line="Uncle Ron (phone): \"I warned you. Don't come home.\"";chief_line_time=8;
                                }
                            }
                            // Shown the door, then out of the gang for good: he takes the player back.
                            if(campaign.kicked_out && path!=Path::Gang) {
                                campaign.kicked_out=false;
                                own_ron_house();
                                const auto& lines=game::campaign::Story::welcome_back_lines();
                                if(!ron_pending_home) ron_pending.assign(lines.begin(),lines.end());
                                chief_line="Uncle Ron (phone): \"You're out of it? Come home, nevo.\"";chief_line_time=7;
                            }
                            story.set_mood(path,campaign.kicked_out);
                        }
                        // Left Ron waiting at the prison gate: he goes home without the player.
                        if(ron_pending_home && uncle_ron.placed && Vector3Distance(uncle_ron.at,player.position)>150) {
                            ron_pending.clear();ron_pending_home=false;ron_home_now(false);
                        }
                        // Walked off mid-word (or drove away): the talk is over.
                        if(story.stage()==game::campaign::Stage::Free && story.line() &&
                           (!uncle_ron.placed || Vector3Distance(uncle_ron.at,{player.position.x,player.position.y-1,player.position.z})>(story.saying() ? 12.0F : 5.0F)))
                            story.hush();
                        story.update(dt);
                        if(story.take_teleport()) ron_home_now(true);
                        if(story.take_home_given()) {
                            own_ron_house();
                            interaction_message="Uncle Ron's house is your home now - Okcidenta Verda, Verda City";interaction_remaining=6;
                        }
                        if(uncle_ron.placed) {
                            const float near=Vector3Distance(feet,uncle_ron.at);
                            const float yaw=near<7 && story.stage()!=game::campaign::Stage::Ride ? facing(uncle_ron.at,feet) : uncle_ron.yaw;
                            npcs.set_bot(uncle_ron_bot,uncle_ron.at,yaw,100,characters::NpcState::Idle);
                        }
                        autosave_note=std::max(0.0F,autosave_note-dt);
                        const bool moved_on=story.take_changed();
                        if(moved_on || (story.stage()==game::campaign::Stage::Free && player_health.alive() && (autosave_wait-=dt)<=0)) {
                            autosave_wait=autosave_every;
                            if(save_campaign(true)) autosave_note=2.5F;
                        }
                    }
                    // Verda Transporto: the boss at his door; the load in hand (cattle called, loaded, shipped).
                    if(depot_boss.placed) {
                        npcs.set_bot(depot_boss_bot,depot_boss.at,depot_boss.yaw,100,characters::NpcState::Idle);
                        game::freight::TruckFrame haul;
                        haul.dt=dt;haul.now=match_time;haul.player=feet;
                        if(const auto* car=vehicles.driver()) {haul.driving=car->id;haul.speed=car->speed;}
                        if(!haul.driving.empty() && !freight.hitched(haul.driving)) haul.driving.clear();   // no trailer, no load
                        const int ranch=trucking.job().ranch;
                        switch(trucking.update(haul)) {
                            case game::freight::TruckEvent::CallCattle: ranches.call_cattle(ranch,4);break;
                            case game::freight::TruckEvent::LoadCattle: trucking.cattle_loaded(ranches.load_cattle(ranch,4));break;
                            case game::freight::TruckEvent::ShipCattle: ranches.ship_cattle(ranch,trucking.job().head);gun_audio.kill_confirm();break;
                            case game::freight::TruckEvent::Loaded: case game::freight::TruckEvent::Delivered: gun_audio.kill_confirm();break;
                            default: break;
                        }
                        if(const int pay=trucking.take_pay();pay>0) {
                            // A brother drives for the union rate.
                            const int paid=static_cast<int>(std::lround(static_cast<float>(pay)*career.haul_rate()));
                            loot_session.economy().money+=paid;loot_session.changed();
                            stats.hauled();
                            if(career.trucker()) {
                                career.haul_done();
                                interaction_message=TextFormat("Union rate: %d kr for the load (company rate %d)",paid,pay);interaction_remaining=4;
                            }
                        }
                    }
                    if(police_camp.placed()) {
                        police_camp.update(dt,feet,weapons.events().shots);
                        if(police_camp.take_graduation()) career.passed_training();
                    }
                    chief_line_time=std::max(0.0F,chief_line_time-dt);
                    // The underworld: docks, ranches and towns, and who holds them.
                    work.docks=smuggling_docks;
                    for(const auto& place:island_life.places())
                        if(place.kind==game::life::PlaceKind::Shop && place.indoor && static_cast<std::size_t>(place.settlement)<verda_region.settlements().size()) {
                            const auto& town=verda_region.settlements()[static_cast<std::size_t>(place.settlement)];
                            work.shops.push_back({"the shop on "+(town.name.empty() ? town.id : town.name)+"'s high street",town.name.empty() ? town.id : town.name,place.interior});
                        }
                    for(std::size_t i=0;i<territory.turfs().size();++i) {
                        const auto& t=territory.turfs()[i];
                        if(t.ranch>=0) {
                            const auto& family=territory.families()[static_cast<std::size_t>(t.ranch)];
                            if(static_cast<std::size_t>(t.ranch)<ranches.ranches().size())
                                work.ranches.push_back({t.ranch,t.name,family.name,ranches.ranches()[static_cast<std::size_t>(t.ranch)].frame.gate,
                                                        territory.controls(static_cast<int>(i),player_gang())});
                        } else work.turfs.push_back({static_cast<int>(i),t.name,t.centre,t.owner,t.hold,t.cleared});
                    }
                    // AI gangs push on each other's towns (a hit squad goes in); towns change hands.
                    if(const auto raid=territory.update(dt,match_time,player_gang(),player_gang()>=0 && career.state().honourable ? 2.0F : 1.0F,
                                                        territory.balance(player_gang()).law);raid.turf>=0) {
                        const auto& target=territory.turfs()[static_cast<std::size_t>(raid.turf)];
                        int from=-1;float nearest=1e9F;
                        for(std::size_t c=0;c<mafio.crews().size();++c) {
                            const auto& crew=mafio.crews()[c];
                            const float d=Vector3Distance(crew.door,target.centre);
                            if(crew.gang==raid.attacker && d<nearest) {nearest=d;from=static_cast<int>(c);}
                        }
                        if(from>=0 && target.crew>=0) mafio.send_hit_squad(from,target.crew,br_bots,bot_drivers,vehicles,verda_region,match_time);
                        if(target.owner==player_gang()) {gang_news=gang_name(raid.attacker)+" are hitting "+target.name+" - your turf. Defend it";gang_news_time=7;}
                        else if(Vector3Distance(target.centre,feet)<700) tell(gang_name(raid.attacker)+" hit "+faction_name(target.owner)+" at "+target.name,5,underworld());
                    }
                    for(const auto& flip:territory.take_flips()) {
                        const auto& t=territory.turfs()[static_cast<std::size_t>(flip.turf)];
                        if(t.crew>=0) mafio.set_crew_gang(t.crew,flip.to);
                        tell(gang_name(flip.to)+" took "+t.name+" from "+faction_name(flip.from),7,underworld());
                        if(flip.to==player_gang()) career.turf_taken(flip.turf,t.name);
                        else if(flip.from==player_gang()) {gang_news="You lost "+t.name+" to "+gang_name(flip.to);gang_news_time=8;}
                    }
                    if(!player_health.alive() && !career_dead) {career.died();stats.died();career_dead=true;}
                    if(player_health.alive()) career_dead=false;
                    mafio.set_calm(territory.balance(player_gang()).law);   // a safer Verda has fewer gang wars
                    // Crime calls: now and then the dispatcher puts out a robbery or a drive-by instead
                    // of the next job; the call follows its suspects; the cuffed walk with the officer.
                    {
                        game::career::Callouts::Frame call;
                        call.dt=dt;call.now=match_time;call.officer=feet;call.yaw=player.yaw;call.alive=player_health.alive();
                        if(work.driving_police) if(const auto* car=vehicles.driver()) {call.police_car=car->id;call.car=car->position;}
                        if(career.due_call(match_time) && (career.state().callouts==0 || GetRandomValue(0,99)<65))
                            callouts.dispatch(call,call_scenes,career,mafio,br_bots,vehicles,verda_region,static_cast<std::uint64_t>(GetRandomValue(1,1<<30)));
                        std::string said;
                        callouts.update(call,career,mafio,br_bots,vehicles,verda_region,said);
                        if(!said.empty()) {interaction_message=said;interaction_remaining=4;}
#ifdef OUTLAND_DEV_TOOLS
                        // OUTLAND_DEV_HEIST=1: skip the chase - the officer and the car beside the scene,
                        // the getaway car gone (the suspects stop where they are), then up close to one who gave up.
                        if(const char* dev=std::getenv("OUTLAND_DEV_HEIST");dev && *dev && callouts.active()) {
                            static int step=0;static double since=0;static std::string mine;
                            for(const auto& h:mafio.heists()) {
                                if(h.car!=callouts.car()) continue;
                                if(step==0 && h.stage==game::mafia::HeistStage::Waiting && match_time>10) {
                                    const int v=vehicles.find(h.car);
                                    const Vector3 kerb=v>=0 ? vehicles.vehicles()[static_cast<std::size_t>(v)].position : h.scene;
                                    const float kerb_yaw=v>=0 ? vehicles.vehicles()[static_cast<std::size_t>(v)].yaw : 0;
                                    Vector3 out=Vector3Subtract(kerb,h.scene);
                                    out.y=0;out=Vector3Length(out)>.1F ? Vector3Normalize(out) : Vector3{0,0,1};
                                    if(const auto* car=vehicles.driver()) mine=car->id;
                                    Vector3 exit{};
                                    if(!mine.empty()) vehicles.exit(verda_region,exit,true);
                                    vehicles.remove(verda_region,h.car);
                                    const Vector3 stand=Vector3Add(h.scene,Vector3Scale(out,3));
                                    if(!mine.empty()) vehicles.place(verda_region,mine,kerb,kerb_yaw);
                                    player.position={stand.x,stand.y+1.2F,stand.z};player.vertical_velocity=0;
                                    player.yaw=std::atan2(-out.x,-out.z);
                                    step=1;since=match_time;vehicle_dirty=true;
                                    TraceLog(LOG_INFO,"OUTLAND dev heist: at the scene %.1f %.1f",stand.x,stand.z);
                                } else if(step==1 && !h.surrendered.empty() && match_time-since>3) {
                                    if(const auto* bot=br_bots.find(h.surrendered.front())) {
                                        const Vector3 b=bot->position();
                                        Vector3 to=Vector3Subtract(Vector3{player.position.x,b.y,player.position.z},b);to.y=0;
                                        to=Vector3Length(to)>.1F ? Vector3Normalize(to) : Vector3{0,0,1};
                                        player.position={b.x+to.x*2,b.y+1.2F,b.z+to.z*2};
                                        player.yaw=std::atan2(-to.x,-to.z);
                                        TraceLog(LOG_INFO,"OUTLAND dev heist: beside a suspect with his hands up");
                                    }
                                    step=2;
                                }
                                if(step>=1) TraceLog(LOG_DEBUG,"heist stage %d surrendered %d running %d",static_cast<int>(h.stage),static_cast<int>(h.surrendered.size()),static_cast<int>(h.running.size()));
                            }
                            // ...then back in the car with the cuffed (they climb in), and off to the prison.
                            if(step==2 && !callouts.escorts().empty()) {step=3;since=match_time;}
                            if(step==3 && match_time-since>5 && !vehicles.driver()) {
                                if(const int v=vehicles.find(mine);v>=0) {
                                    const Vector3 at=vehicles.vehicles()[static_cast<std::size_t>(v)].position;
                                    vehicles.enter(verda_region,v,at);vehicle_dirty=true;
                                    TraceLog(LOG_INFO,"OUTLAND dev heist: in the car, %d in custody",career.custody());
                                }
                                step=4;since=match_time;
                            }
                        }
                        if(const char* dev=std::getenv("OUTLAND_DEV_HEIST");dev && *dev) {
                            static int leg=0;static double since=0;
                            if(leg==0 && career.custody()>0 && vehicles.driver()) {leg=1;since=match_time;}
                            // (A car can't be moved with someone at the wheel: out, move it, back in.)
                            const auto move_car=[&](Vector3 to) {
                                const std::string id=vehicles.driver()->id;
                                Vector3 exit{};
                                vehicles.exit(verda_region,exit,true);
                                vehicles.place(verda_region,id,to,world::layout::prison_yaw);
                                if(const int v=vehicles.find(id);v>=0) vehicles.enter(verda_region,v,vehicles.vehicles()[static_cast<std::size_t>(v)].position);
                                vehicle_dirty=true;
                            };
                            if(leg==1 && match_time-since>3) {
                                move_car(prison_at(0,-150));leg=2;
                                TraceLog(LOG_INFO,"OUTLAND dev heist: on the prison road");
                            }
                            if(leg==2 && prison_tour.line<0 && career.state().prison_toured && vehicles.driver()) {
                                move_car(prison_at(0,-44));leg=3;
                                TraceLog(LOG_INFO,"OUTLAND dev heist: at the gate");
                            }
                        }
#endif
                        if(callouts.take_partner_wanted() && player_crew.partner()<0) {
                            const Vector3 at=Vector3Add(feet,{std::cos(player.yaw)*1.6F,0,-std::sin(player.yaw)*1.6F});   // at the officer's side
                            if(player_crew.assign_partner(at,player.yaw*RAD2DEG+180,br_bots,match_time)) vehicle_dirty=true;
                        }
                    }
                    career.update(work);
#ifdef OUTLAND_DEV_TOOLS
                    // OUTLAND_DEV_STRIKE=1: a brother calls a strike as soon as the depot is known.
                    if(static bool dev_strike=[]{const char* v=std::getenv("OUTLAND_DEV_STRIKE");return v && *v=='1';}();dev_strike && career.trucker() && work.depot_known) {
                        std::string why;
                        dev_strike=!career.call_strike(match_time,why);
                    }
#endif
                    // A strike on: a line on screen says where it stands (the bag has the rest).
                    if(career.strike_on() && (interaction_remaining<.3F || interaction_message.starts_with("STRIKE"))) {
                        const auto& job=career.job();
                        const int left=std::max(0,job.needed-job.done);
                        const float far=std::hypot(player.position.x-job.target.x,player.position.z-job.target.z);
                        interaction_message=career.striking() ? TextFormat("STRIKE  -  the gate is blocked: hold it %d:%02d more",left/60,left%60) :
                            far<strike_picket_reach+30 ? TextFormat("STRIKE  -  park a big rig in the gate (the marker), then hold it %d s",left) :
                            far<600 ? TextFormat("STRIKE  -  bring a big rig to Verda Transporto's gate: %.0f m",far) : std::string();
                        if(!interaction_message.empty()) interaction_remaining=.6F;
                    }
#ifdef OUTLAND_DEV_TOOLS
                    if(static game::career::JobKind logged=game::career::JobKind::None;career.job().kind!=logged) {
                        logged=career.job().kind;
                        if(logged!=game::career::JobKind::None) TraceLog(LOG_INFO,"OUTLAND job: %s - %s (chief %d, respect %.0f)",career.job().title.c_str(),career.job().detail.c_str(),work.chief_known ? 1 : 0,career.state().respect);
                    }
#endif
                    // What the job set off: the police tipped off, a demand at a ranch, a job paid out.
                    if(career.take_tipoff()) {
                        game::law::JusticeFrame tip;tip.now=match_time;tip.player=feet;
                        justice.tip_off(2,feet,tip,island_life,career.job().kind==game::career::JobKind::RobStore ?
                                        "Store robbery: the alarm went off - the police are on the way" :
                                        career.job().kind==game::career::JobKind::Contraband ? "Contraband in a big rig: the Ŝosea Patrolo is out for it" :
                                        "Contraband run: someone tipped off the police");
                    }
                    // TAG UP: the boss hands over a bag of cans with the job.
                    if(const int cans=career.take_cans();cans>0) {
                        loot_session.edit_inventory().add(item_registry,"spray_paint",cans);loot_session.changed();
                        interaction_message=TextFormat("The boss hands you %d cans of spray paint",cans);interaction_remaining=4;
                    }
                    if(career.take_extort_arrival()) {
                        const int ranch=career.job().ranch,gang=player_gang();
                        if(ranch>=0 && territory.extort(ranch,gang)) {
                            career.extort_result(true,game::mafia::family_faction(ranch));
                            gang_news="The "+territory.families()[static_cast<std::size_t>(ranch)].name+" family pays "+gang_name(gang);gang_news_time=6;
                        } else if(ranch>=0) {
                            const auto& family=territory.families()[static_cast<std::size_t>(ranch)];
                            if(static_cast<std::size_t>(ranch)<ranches.ranches().size())
                                ranches.provoke_ranch(ranch,match_time,"The "+family.name+" family won't be squeezed");
                            if(family.stance==game::mafia::Stance::Allied && family.gang>=0 && family.gang!=gang)
                                mafio.provoke_gang(family.gang,match_time,gang_name(family.gang)+" stand by the "+family.name+" family - they want you dead");
                            career.extort_result(false,game::mafia::family_faction(ranch));
                        }
                    }
                    if(game::career::Job done;career.take_finished(done)) {
                        if(done.kind==game::career::JobKind::Smuggle) territory.delivered(territory.turf_at(done.target),player_gang());
                        if(done.kind==game::career::JobKind::Extort && done.ranch>=0) territory.subdued(done.ranch,player_gang());
                        if(done.kind==game::career::JobKind::HitChief && player_gang()>=0) {
                            // The Chief is dead: the gang takes Verda, and every crew wears its colours.
                            const int mine=player_gang();
                            territory.take_island(mine);
                            for(std::size_t c=0;c<mafio.crews().size();++c) mafio.set_crew_gang(static_cast<int>(c),mine);
                            territory.take_flips();
                            island_ruled=true;banner_key.clear();
                            gang_news="THE CHIEF IS DEAD - "+gang_name(mine)+" TAKE VERDA";gang_news_time=14;
                            gun_audio.kill_confirm();
                            game::law::JusticeFrame tip;tip.now=match_time;tip.player=feet;
                            justice.tip_off(3,feet,tip,island_life,"The Chief has been shot - every officer on the island is coming");
                        }
                        if(done.kind==game::career::JobKind::CleanUp && done.crew>=0 && static_cast<std::size_t>(done.crew)<mafio.crews().size()) {
                            // Broken up for good: the hideout stands empty, the town is nobody's.
                            mafio.disband(done.crew);
                            for(std::size_t i=0;i<territory.turfs().size();++i)
                                if(territory.turfs()[i].crew==done.crew) territory.clear_town(static_cast<int>(i));
                            banner_key.clear();
                            gang_news="The Policio broke up the "+gang_name(mafio.crews()[static_cast<std::size_t>(done.crew)].gang)+" in "+
                                      mafio.crews()[static_cast<std::size_t>(done.crew)].town;
                            gang_news_time=8;
                        }
                    }
                    if(const int mine=player_gang();mine>=0 && !island_ruled && territory.rules_island(mine)) {
                        island_ruled=true;
                        gang_news=gang_name(mine)+" RULE VERDA - every town and every ranch is yours";gang_news_time=12;
                        gun_audio.kill_confirm();
                    }
                    // How it ends: a gang that rules Verda brings on the endgame - the Green Hand
                    // march on the parliament first; then the Russians land.
                    if(endgame.ready()) {
                        // Dev: OUTLAND_DEV_ENDGAME=siege (the Green Hand's) or =landing (another gang's win).
                        if(static const char* dev=std::getenv("OUTLAND_DEV_ENDGAME");dev && endgame.phase()==game::mafia::EndPhase::None)
                            endgame.gang_won(std::string(dev)=="siege" ? 0 : 2,match_time-(std::string(dev)=="siege" ? 0 : game::mafia::Endgame::landing_delay),br_bots);
                        if(island_ruled && player_gang()>=0 && endgame.phase()==game::mafia::EndPhase::None) {
                            endgame.gang_won(player_gang(),match_time,br_bots);
                            if(endgame.phase()==game::mafia::EndPhase::Siege) {gang_news="THE GREEN HAND MARCH ON THE REGISTARO - take the parliament";gang_news_time=12;}
                        }
                        const auto before=endgame.phase();
                        if(const auto said=endgame.update(feet,player_health.alive(),match_time,br_bots);!said.empty()) {
                            gang_news=said;gang_news_time=14;gun_audio.kill_confirm();
                        }
                        const auto now_phase=endgame.phase();
                        if(before==game::mafia::EndPhase::Siege && now_phase!=game::mafia::EndPhase::Siege) {
                            career.orders_done(game::career::JobKind::Siege,25000,20);
                            banner_key.clear();
                        }
                        if(before==game::mafia::EndPhase::Occupied && now_phase==game::mafia::EndPhase::Free) {
                            const int pay=endgame.take_pay();
                            if(career.state().side==game::career::Side::Civilian) {loot_session.economy().money+=pay;loot_session.changed();}
                            else career.orders_done(game::career::JobKind::Liberate,pay,30);
                        }
                        career.set_occupied(now_phase==game::mafia::EndPhase::Occupied);
                        if(now_phase==game::mafia::EndPhase::Siege && career.in_gang(0))
                            career.orders(game::career::JobKind::Siege,endgame.hall(),
                                          "Take Registaro de Verda: the parliament's guard holds the steps - put them down and walk into the hall");
                        if(now_phase==game::mafia::EndPhase::Occupied)
                            career.orders(game::career::JobKind::Liberate,endgame.objective(feet),
                                          TextFormat("Russian troops hold %d of %d towns - clear the garrison in %s",endgame.garrisons_left(),
                                                     static_cast<int>(endgame.garrisons().size()),endgame.objective_town(feet).c_str()));
                        if(!endgame_path.empty() && game::rules_for(game_mode).civilian_life)
                            if(const auto text=endgame.serialise();text!=endgame_saved && SaveFileText(endgame_path.c_str(),const_cast<char*>(text.c_str()))) endgame_saved=text;
                    }
                    gang_news_time=std::max(0.0F,gang_news_time-dt);
                    territory_save_wait-=dt;
                    if(territory_save_wait<=0 && !territory_path.empty()) {
                        territory_save_wait=2;
                        if(const auto text=territory.serialise();text!=territory_saved && SaveFileText(territory_path.c_str(),const_cast<char*>(text.c_str()))) territory_saved=text;
                    }
                    // An officer's first drop-off: the warden shows them round before the guards take the prisoners.
                    if(prison_tour.line<0 && !career.state().prison_toured && career.custody()>0 && work.driving_police &&
                       std::hypot(feet.x-prison_intake.x,feet.z-prison_intake.z)<200) {
                        prison_tour.line=0;prison_tour.t=0;
                        tour_shot(0,prison_tour.eye,prison_tour.look);
                        prison_tour.eye=camera.position;prison_tour.look=camera.target;   // swing over from where the player looks
                    }
                    if(prison_tour.line>=0) {
                        prison_tour.t+=dt;
                        input_system.cancel_controls();
                        if(prison_tour.t>7.5F || (prison_tour.t>1 && (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))) {
                            prison_tour.t=0;
                            if(++prison_tour.line>=static_cast<int>(tour_lines.size())) {prison_tour.line=-1;career.prison_toured();}
                        }
                    }
                    // Prisoners in the back: booked in at the prison's gate from a police car.
                    if(const int held=career.custody();held>0 && prison_tour.line<0 && work.driving_police && std::hypot(feet.x-prison_intake.x,feet.z-prison_intake.z)<25 && career.deliver()>0) {
                        interaction_message=career.message();interaction_remaining=4;gun_audio.kill_confirm();
                        stats.arrests(held);
                        // Into the yard with the rest.
                        for(int k=0;k<held;++k)
                            if(const int id=prison_yard.admit(true,prison_body);id>=0) {
                                for(const auto& p:prison_yard.people()) if(p.bot==id) prison_spawn(p);
                                jail_terms.book(id,-1,island_life.clock().minutes,true);   // a week inside, with his own
                            }
                    }
                    // The prison's people go about their day. The island's people booked in do their week:
                    // they join a gang inside, or get shanked and go to the hospital (and out from there).
                    for(const auto& e:jail_terms.update(island_life.clock().minutes)) {
                        const auto& term=e.term;
                        const std::string who=term.resident>=0 ? island_life.residents()[static_cast<std::size_t>(term.resident)].full_name() : std::string();
                        if(e.fate==game::law::JailTerms::Fate::Joined && term.inmate>=0) {
                            if(const int recruit=prison_yard.recruit(term.inmate,prison_body);recruit>=0) {
                                npcs.set_bot_hidden(term.inmate,true);
                                for(const auto& p:prison_yard.people()) if(p.bot==recruit) prison_spawn(p);
                                jail_terms.moved(term.inmate,recruit);
                            }
                            if(!who.empty()) tell("PRISON: "+who+" joined "+gang_name(term.gang)+" inside",6,underworld());
                        } else if(e.fate==game::law::JailTerms::Fate::Shanked) {
                            if(term.inmate>=0) {npcs.set_bot_hidden(term.inmate,true);prison_yard.remove(term.inmate);}
                            if(term.resident>=0) island_life.hospitalise(term.resident,services.has_hospital() ? services.hospital_door() : prison_intake,
                                                                      game::services::EmergencyServices::treatment_minutes);
                            if(!who.empty()) tell("PRISON: "+who+" wouldn't join a gang - shanked in the yard. To the hospital, then out",6,underworld());
                        } else if(e.fate==game::law::JailTerms::Fate::Released) {
                            if(term.inmate>=0) {npcs.set_bot_hidden(term.inmate,true);prison_yard.remove(term.inmate);}
                            if(term.resident>=0) island_life.set_free(term.resident);
                            if(!who.empty()) tell("PRISON: "+who+" did the week and walked out of the gate",6,career.on_duty());
                        }
                    }
                    prison_yard.update(dt);
                    // La Verda Basballigo: the games, as the island clock runs.
                    if(game::rules_for(game_mode).civilian_life && !league.fixtures().empty()) {
                        using namespace game::sports;
                        const double now=island_life.clock().minutes;
                        const auto nick=[](int team){return std::string(teams[static_cast<std::size_t>(team)].nick);};
                        // The town turns out: ahead of each game its fans set off for the park - the
                        // home crowd from town, the visitors' followers across the island (by car if
                        // they have one) - and go home after it.
                        if(!island_life.empty())
                            for(int i=0;i<static_cast<int>(league.fixtures().size());++i) {
                                if(game_fans.count(i)) continue;
                                const double pitch=league.first_pitch_at(i);
                                if(now<pitch-League::pregame-90 || now>pitch+60) continue;
                                const auto& g=league.fixtures()[static_cast<std::size_t>(i)];
                                const int venue=island_life.ballpark(g.home);
                                auto& fans=game_fans[i];
                                if(venue<0) continue;
                                const auto& island=island_life.island();
                                const auto town_of=[&](int team) {
                                    Vector3 c{};
                                    for(const auto& site:world::layout::sites) if(std::string_view(site.id)==teams[static_cast<std::size_t>(team)].site) c=site.center;
                                    int best=-1;float d=1e9F;
                                    for(std::size_t k=0;k<island.settlement_centers.size();++k) {
                                        const float e=std::hypot(island.settlement_centers[k].x-c.x,island.settlement_centers[k].z-c.z);
                                        if(e<d) {d=e;best=static_cast<int>(k);}
                                    }
                                    return best;
                                };
                                const double until=pitch+league.linescore(i).length+League::postgame*.5;
                                const int seats=world::ballpark::rows*world::ballpark::seats_per_row;
                                for(const bool home_side:{true,false}) {
                                    const int town=town_of(home_side ? g.home : g.away);
                                    std::vector<std::pair<std::uint64_t,int>> order;
                                    int people=0;
                                    for(const auto& r:island.residents) {
                                        if(r.settlement!=town || !r.alive || r.occupation==game::life::Occupation::PoliceOfficer ||
                                           r.occupation==game::life::Occupation::Doctor) continue;
                                        ++people;
                                        // The travelling fans are the ones with a car first.
                                        const std::uint64_t h=(static_cast<std::uint64_t>(r.id)*0x9e3779b97f4a7c15ULL)^(static_cast<std::uint64_t>(i+league.season()*31)*0xc2b2ae3d27d4eb4fULL);
                                        order.push_back({(home_side || r.car<0 ? h|(1ULL<<63) : h&~(1ULL<<63)),r.id});
                                    }
                                    std::sort(order.begin(),order.end());
                                    const int want=home_side ? std::min(70,std::max(8,people*3/10))
                                                             : std::min(40,static_cast<int>(static_cast<float>(std::min(70,std::max(8,people*3/10)))*league.travelling(i)));
                                    int taken=0;
                                    for(const auto& [h,id]:order) {
                                        if(taken>=want) break;
                                        const double stagger=static_cast<double>(h%40);
                                        const double leave=home_side ? pitch-League::pregame+stagger : pitch-League::pregame-60+stagger;
                                        if(!island_life.attend(id,venue,leave,until+static_cast<double>((h>>8)%20))) continue;
                                        fans.push_back({id,home_side,(taken*37+i*11)%seats});
                                        ++taken;
                                    }
                                }
                                TraceLog(LOG_INFO,"OUTLAND ballgame %d: %zu fans on their way to the %s park",i,fans.size(),teams[static_cast<std::size_t>(g.home)].nick);
                            }
                        for(const auto& e:league.update(now)) {
                            if(e.kind==EventKind::NewSeason) {gang_news=TextFormat("BASBALLIGO: season %d begins - five clubs, one cup",league.season());gang_news_time=6;game_fans.clear();continue;}
                            if(e.fixture<0 || e.fixture>=static_cast<int>(league.fixtures().size())) continue;
                            const auto& g=league.fixtures()[static_cast<std::size_t>(e.fixture)];
                            const auto& site=parks[static_cast<std::size_t>(g.home)];
                            const bool here=site.team>=0 && std::hypot(feet.x-site.origin.x,feet.z-site.origin.z)<320;
                            const auto score=league.live_fixture(e.fixture,now);
                            switch(e.kind) {
                                case EventKind::GameOn: {
                                    int seated=0;
                                    for(const auto& fan:game_fans[e.fixture])
                                        seated+=island_life.in_the_stands(island_life.residents()[static_cast<std::size_t>(fan.resident)]);
                                    TraceLog(LOG_INFO,"OUTLAND ballgame %d: first pitch, %d of %zu fans in their seats",e.fixture,seated,game_fans[e.fixture].size());
                                }
                                    gang_news=(g.final ? "THE FINAL: " : "PLAY BALL: ")+nick(g.away)+" at "+nick(g.home);gang_news_time=6;
                                    break;
                                case EventKind::Run:
                                    (e.team==g.home ? park_cheer_home : park_cheer_away)[static_cast<std::size_t>(g.home)]=match_time+5;
                                    if(here) {
                                        // Who did it, and how.
                                        std::string how=nick(e.team)+" score!";
                                        if(score.play>=0) {
                                            const auto line=league.linescore(e.fixture);
                                            const auto& pl=line.plays[static_cast<std::size_t>(score.play)];
                                            how=league.roster(pl.home ? g.home : g.away).lineup[static_cast<std::size_t>(pl.batter)].name+" "+outcome_name(pl.outcome)+"!";
                                        }
                                        interaction_message=TextFormat("%s  %s %d - %d %s",how.c_str(),nick(g.home).c_str(),score.home_runs,score.away_runs,nick(g.away).c_str());interaction_remaining=4;
                                    }
                                    break;
                                case EventKind::GameOver:
                                    gang_news=TextFormat("FINAL: %s %d, %s %d",nick(g.home).c_str(),score.home_runs,nick(g.away).c_str(),score.away_runs);gang_news_time=7;
                                    break;
                                case EventKind::Fight:
                                    park_fight_until[static_cast<std::size_t>(g.home)]=match_time+45;
                                    if(here) {interaction_message="A fight breaks out between the "+nick(g.home)+" and "+nick(g.away)+" fans!";interaction_remaining=5;}
                                    else tell("BRAWL in the stands at the "+nick(g.home)+" ballpark: "+nick(g.home)+" and "+nick(g.away)+" fans",6,career.on_duty());
                                    break;
                                case EventKind::Champion:
                                    gang_news="ĈAMPIONOJ! The "+std::string(teams[static_cast<std::size_t>(e.team)].name)+" win the Basballigo!";gang_news_time=9;
                                    break;
                                case EventKind::Riot:
                                    tell("RIOT at the "+nick(g.home)+" ballpark: the "+nick(e.team)+" fans won't take it - police on the way",9,career.on_duty());
                                    riot_team=e.team;
                                    if(site.team>=0 && services.active()) services.respond(site.to_world(world::ballpark::gate),3,match_time+game::sports::League::riot_minutes*60.0/30.0,vehicles);
                                    break;
                                default: break;
                            }
                        }
                        // A riot: while one burns and the player is close, the angriest fans go for anyone
                        // near (hostile, in their club's shirts). They go home when it's over.
                        int burning=-1;
                        for(int p=0;p<5;++p) if(parks[static_cast<std::size_t>(p)].team>=0 && league.rioting(p,now)) burning=p;
                        if(burning>=0 && riot_park<0) {
                            const auto& site=parks[static_cast<std::size_t>(burning)];
                            if(std::hypot(feet.x-site.origin.x,feet.z-site.origin.z)<260) {
                                riot_park=burning;
                                const auto& fin=league.fixtures().back();
                                if(riot_team!=fin.home && riot_team!=fin.away) riot_team=league.champion(now)==fin.home ? fin.away : fin.home;
                                for(int k=0;k<12;++k) {
                                    characters::ResidentSpawn spawn;
                                    spawn.resident=rioter_base+k;
                                    spawn.character_id=outfit_body;
                                    spawn.pool=characters::CharacterPool::Hostile;
                                    const float a=static_cast<float>(k)*.9F;
                                    Vector3 local{std::cos(a)*(10.0F+static_cast<float>(k%4)*4),0,-58.0F+std::sin(a)*8};
                                    spawn.position=site.to_world(local);
                                    spawn.anchor=spawn.position;spawn.anchor_radius=14;
                                    spawn.yaw_degrees=site.yaw+static_cast<float>(k*40);
                                    npcs.spawn_resident(spawn);
                                    rioters.push_back(spawn.resident);
                                }
                                interaction_message="A riot! The "+nick(riot_team)+" fans are tearing the place up";interaction_remaining=5;
                            }
                        }
                        if(riot_park>=0 && (!league.rioting(riot_park,now) ||
                           std::hypot(feet.x-parks[static_cast<std::size_t>(riot_park)].origin.x,feet.z-parks[static_cast<std::size_t>(riot_park)].origin.z)>420)) {
                            for(const int id:rioters) npcs.despawn_resident(id);
                            rioters.clear();riot_park=-1;
                        }
                        // The clubs' buses: each in its colours; on a game day the visitors' bus is parked at
                        // the home team's park (moved while nobody's watching either end).
                        if((league_bus_sync-=dt)<=0) {
                            league_bus_sync=2;
                            for(const auto& v:vehicles.vehicles()) {
                                const auto at=v.id.find("vehicle_spawn_bus_team_");
                                if(at==std::string::npos) continue;
                                const int team=v.id.back()-'0';
                                if(team<0 || team>=team_count) continue;
                                auto* bus=vehicles.asset(verda_region,v.id);
                                if(!bus || bus->vehicle.destroyed) continue;
                                const std::size_t paint=game::autos::team_paints+static_cast<std::size_t>(team);
                                if(!bus->vehicle.painted || bus->vehicle.mods[0]!=static_cast<int>(paint)) {
                                    bus->vehicle.painted=true;bus->vehicle.mods[0]=static_cast<int>(paint);bus->vehicle.paint=game::autos::paints[paint].color;
                                }
                                if(vehicles.driver() && vehicles.driver()->id==v.id) continue;
                                int host=team;
                                for(int i=0;i<static_cast<int>(league.fixtures().size());++i) {
                                    const auto& g=league.fixtures()[static_cast<std::size_t>(i)];
                                    if(g.away==team && league.live_fixture(i,now).phase!=Phase::Quiet) host=g.home;
                                }
                                const auto& site=parks[static_cast<std::size_t>(host)];
                                if(site.team<0) continue;
                                const Vector3 bay=site.to_world(host==team ? world::ballpark::home_bus : world::ballpark::away_bus);
                                if(std::hypot(v.position.x-bay.x,v.position.z-bay.z)<8) continue;
                                if(std::hypot(feet.x-bay.x,feet.z-bay.z)<160 || std::hypot(feet.x-v.position.x,feet.z-v.position.z)<160) continue;
                                vehicles.place(verda_region,v.id,bay,site.yaw);
                            }
                        }
                    }
                    for(const auto& p:prison_yard.people())
                        npcs.set_bot(p.bot,prison_yard.world(p),prison_yard.world_yaw(p),100,prison_yard.state(p));
                    if(const int pay=career.take_pay();pay>0) {loot_session.economy().money+=pay;loot_session.changed();}
                    mafio.set_player_gang(career.state().side==game::career::Side::Gang ? career.state().gang : -1);
                    services.assign_player_car(career.on_duty() ? career.state().patrol_car : std::string());
                    services.set_player_siren(false);   // the player works their own lights (LIGHTS / L)
                    career.tick_message(dt);
                    // The record: the road and the pavement, the time, the wallet.
                    {
                        const Vector3 now{player.position.x,0,player.position.z};
                        if(const auto* car=vehicles.driver()) stats.drove(std::abs(car->speed)*dt,car->speed);
                        else if(stats_placed && !riding_now) {
                            // On foot (faster than a sprint is a ride - the bus, a passenger seat - not a walk).
                            const float step=Vector3Distance(now,stats_feet);
                            if(step<12*dt+.05F) stats.walked(step);
                        }
                        stats_feet=now;stats_placed=true;
                        stats.played(dt);
                        const std::int64_t money=loot_session.economy().money;
                        if(stats_money>=0 && money!=stats_money) stats.money(stats_money,money);
                        stats_money=money;
                    }
                    // Saved whenever it changes (at most once a second).
                    career_save_wait-=dt;
                    if(career_save_wait<=0) {
                        career_save_wait=1;
                        if(const auto text=career.serialise();text!=career_saved && SaveFileText(career_path.c_str(),const_cast<char*>(text.c_str()))) career_saved=text;
                        if(const auto text=trucking.serialise();!trucking_path.empty() && text!=trucking_saved && SaveFileText(trucking_path.c_str(),const_cast<char*>(text.c_str()))) trucking_saved=text;
                        if(const auto text=league.serialise();!league_path.empty() && text!=league_saved && SaveFileText(league_path.c_str(),const_cast<char*>(text.c_str()))) league_saved=text;
                        if(const auto text=stats.serialise();!stats_path.empty() && text!=stats_saved && SaveFileText(stats_path.c_str(),const_cast<char*>(text.c_str()))) stats_saved=text;
                    }
                }
                br_bots.sync_bodies(npcs,&character_registry);
                // The cuffed walk behind the officer, unarmed; a suspect with his hands up has dropped his gun.
                for(const auto& e:callouts.escorts()) if(const auto* bot=br_bots.find(e.bot)) {
                    npcs.set_bot(e.bot,bot->position(),bot->yaw(),bot->health(),e.walking ? characters::NpcState::Wander : characters::NpcState::Idle);
                    npcs.set_bot_weapon(e.bot,-1);
                }
                for(const auto& h:mafio.heists()) for(const int id:h.surrendered) if(!callouts.escorting(id)) npcs.set_bot_weapon(id,-1);
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
            if(lan_royale) {
                // Squad revive: the helper keeps at it while close; the downed bleed, or get up.
                if(const auto helping=squad_revive.helping();helping!=game::lan::SquadRevive::nobody) {
                    const auto* who=lan_play.find(helping);
                    const bool still=who && who->known && (who->state.flags&net::StateAlive) && down(*who) &&
                        std::hypot(who->shown.x-feet.x,who->shown.z-feet.z)<=game::lan::SquadRevive::keep_reach &&
                        player_health.alive() && !player_health.downed() && !driving && weapons.events().shots==0;
                    if(!still) squad_revive.stop_helping();
                }
                const bool helped=player_health.downed() && game::lan::SquadRevive::helped_by(lan.slot(),feet,lan_play.remotes(),teammate);
                if(squad_revive.update(dt,player_health,squad_play() && !in_drop,teammates_standing(),helped)) {
                    std::string helper="A TEAMMATE";
                    for(const auto& r:lan_play.remotes()) if(r.known && r.state.reviving==lan.slot() && teammate(r.slot)) {helper=r.name;break;}
                    const std::string line=helper+" PICKED UP "+lan.name_of(lan.slot());
                    br_bots.add_feed(line);if(lan.in_match()) lan.send(net::Msg::Feed,net::Feed{line});
                    interaction_message="BACK ON YOUR FEET";interaction_remaining=2.5F;
                }
                if(player_health.downed() && !was_downed) {
                    // Knocked down: out of any car; the side can pick you up.
                    if(vehicles.driver()) {
                        Vector3 exit{};
                        if(vehicles.exit(verda_region,exit,true)) {player.position=Vector3Add(exit,{0,1,0});player.vertical_velocity=0;vehicle_renderer.reset_camera();}
                    }
                    const std::string line=lan.name_of(lan.slot())+" IS DOWN";
                    br_bots.add_feed(line);if(lan.in_match()) lan.send(net::Msg::Feed,net::Feed{line});
                    haptics.hit_taken(60);
                }
                was_downed=player_health.downed();
                mode_session.set_lan(enemies_alive(),!bots_mirrored,teammates_alive());
            }
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
                vehicle_audio.stop();radio_audio.stop();
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
                    if(player_health.downed()) {
                        state.status|=net::StatusDowned;
                        state.revive=static_cast<std::uint8_t>(std::clamp(squad_revive.progress(),0.0F,1.0F)*255);
                    }
                    state.reviving=squad_revive.helping();
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
                // The player's own car, lights on: its siren too (loudest at the wheel).
                if(const int lit=lights_car.empty() || vehicle_paused ? -1 : vehicles.find(lights_car);lit>=0) {
                    const auto mix=audio::mix_shot(vehicles.vehicles()[static_cast<std::size_t>(lit)].position,camera.position,
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
        // (In device pixels: on a HiDPI browser screen the canvas holds more pixels than the screen's
        // CSS size the HUD is laid out in.)
        const int scene_width=std::max(64,static_cast<int>(static_cast<float>(GetRenderWidth())*render_scale.scale()));
        const int scene_height=std::max(64,static_cast<int>(static_cast<float>(GetRenderHeight())*render_scale.scale()));
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
        // Out of a LAN match with the side fighting on: over a teammate's shoulder.
        if(mode_session.squad_fighting())
            if(const auto* who=lan_play.find(static_cast<std::uint8_t>(std::max(0,spectate_slot)));who && who->known) {
                const float yaw=who->shown_yaw*DEG2RAD;   // their look (degrees)
                const Vector3 back{-std::sin(yaw),0,-std::cos(yaw)};
                const Vector3 look{who->shown.x,who->shown.y+1.6F,who->shown.z};
                camera.position=Vector3Add(look,Vector3Add(Vector3Scale(back,5.5F),{0,1.8F,0}));
                camera.target=look;
            }
        // The warden's tour: the camera glides from one view of the prison to the next.
        if(prison_tour.line>=0) {
            Vector3 eye{},look{};
            tour_shot(prison_tour.line,eye,look);
            const float k=1-std::exp(-dt*1.6F);
            prison_tour.eye=Vector3Lerp(prison_tour.eye,eye,k);prison_tour.look=Vector3Lerp(prison_tour.look,look,k);
            camera.position=prison_tour.eye;camera.target=prison_tour.look;
        }
        // The parks' scoreboards: the line score as it stands (Linescore::box - the same runs as the
        // panel's), drawn into each board's texture before the scene.
        park_boards.on.fill(false);
        if(game::rules_for(game_mode).civilian_life && !league.fixtures().empty()) {
            using namespace game::sports;
            const double now=island_life.clock().minutes;
            for(int p=0;p<5;++p) {
                const auto& site=parks[static_cast<std::size_t>(p)];
                if(site.team<0 || Vector3DistanceSqr(site.origin,camera.position)>260*260) continue;
                const auto live=league.live(p,now);
                auto& face=park_boards.face[static_cast<std::size_t>(p)];
                if(face.id==0) face=LoadRenderTexture(640,200);
                park_boards.on[static_cast<std::size_t>(p)]=face.id!=0;
                if(face.id==0) continue;
                const Font& font=sign_renderer.font();
                const auto cell=[&](const std::string& text,float x,float y,float size,Color ink){
                    const float w=MeasureTextEx(font,text.c_str(),size,1).x;
                    DrawTextEx(font,text.c_str(),{x-w/2,y},size,1,ink);
                };
                const Color amber{255,206,84,255},white{238,240,232,255},dim{120,140,128,255};
                const auto upper=[](std::string n){for(auto& c:n) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));return n;};
                // No game on: the home club's name and the league.
                if(live.phase==Phase::Quiet || live.fixture<0) {
                    const std::string key="idle";
                    if(park_boards.shows[static_cast<std::size_t>(p)]==key) continue;
                    park_boards.shows[static_cast<std::size_t>(p)]=key;
                    const auto& club=teams[static_cast<std::size_t>(site.team)];
                    BeginTextureMode(face);
                    ClearBackground(Color{14,30,22,255});
                    DrawRectangle(0,0,640,14,club.primary);DrawRectangle(0,186,640,14,club.secondary);
                    cell(upper(club.name),320,52,56,white);
                    cell("LA VERDA BASBALLIGO",320,124,28,amber);
                    EndTextureMode();
                    continue;
                }
                const auto& g=league.fixtures()[static_cast<std::size_t>(live.fixture)];
                const auto line=league.linescore(live.fixture);
                const double t=now-league.first_pitch_at(live.fixture);
                const auto box=line.box(t);
                // Seven innings on the board; extra ones as they start.
                int started=0;
                for(int k=0;k<static_cast<int>(box.away.size());++k) if(box.away[static_cast<std::size_t>(k)]>=0) started=k+1;
                const int columns=std::max(League::innings,started);
                const auto name=[&](int team){return upper(teams[static_cast<std::size_t>(team)].nick);};
                const std::string state=live.phase==Phase::Pregame ? TextFormat("FIRST PITCH %02d:%02d",static_cast<int>(League::first_pitch)/60,static_cast<int>(League::first_pitch)%60) :
                    live.phase==Phase::Over ? std::string(g.final ? "FINAL - LA FINALO" : "FINAL") :
                    TextFormat("%s %d   %d OUT",live.top ? "TOP" : "BOT",live.inning,std::min(live.outs,2));
                // What it shows, as text: redrawn only when that changes.
                std::string key=state+"|"+std::to_string(g.away)+"|"+std::to_string(g.home);
                for(const int r:box.away) key+=","+std::to_string(r);
                for(const int r:box.home) key+=";"+std::to_string(r);
                for(int k=0;k<2;++k) key+=TextFormat("/%d.%d.%d",box.runs[k],box.hits[k],box.errors[k]);
                if(park_boards.shows[static_cast<std::size_t>(p)]==key) continue;
                park_boards.shows[static_cast<std::size_t>(p)]=key;
                BeginTextureMode(face);
                ClearBackground(Color{14,30,22,255});
                const float name_w=214,col_w=std::min(40.0F,(640-name_w-150)/static_cast<float>(columns));
                for(int c=0;c<columns;++c) cell(std::to_string(c+1),name_w+col_w*(static_cast<float>(c)+.5F),6,28,dim);
                const float tx=name_w+col_w*static_cast<float>(columns)+4;
                cell("R",tx+23,6,28,amber);cell("H",tx+69,6,28,dim);cell("E",tx+115,6,28,dim);
                for(int row=0;row<2;++row) {
                    const bool home_row=row==1;
                    const float y=44+static_cast<float>(row)*54;
                    const auto& team=teams[static_cast<std::size_t>(home_row ? g.home : g.away)];
                    DrawRectangle(8,static_cast<int>(y),12,40,team.primary);
                    // The city's name, made to fit its column.
                    const std::string city=name(home_row ? g.home : g.away);
                    float size=38;
                    while(size>18 && MeasureTextEx(font,city.c_str(),size,1).x>name_w-34) size-=1;
                    DrawTextEx(font,city.c_str(),{26,y+20-size/2},size,1,white);
                    const auto& runs=home_row ? box.home : box.away;
                    for(int c=0;c<columns && c<static_cast<int>(runs.size());++c)
                        if(runs[static_cast<std::size_t>(c)]>=0) cell(std::to_string(runs[static_cast<std::size_t>(c)]),name_w+col_w*(static_cast<float>(c)+.5F),y+1,38,white);
                    const std::size_t k=home_row ? 1 : 0;
                    cell(std::to_string(box.runs[k]),tx+23,y+1,38,amber);
                    cell(std::to_string(box.hits[k]),tx+69,y+1,38,white);
                    cell(std::to_string(box.errors[k]),tx+115,y+1,38,white);
                }
                DrawRectangle(0,152,640,2,dim);
                cell(state,320,160,34,amber);
                EndTextureMode();
            }
        }
        BeginTextureMode(scene_target);

        // The sky is drawn before the world, pre-divided by the ambient multiply that follows.
        ClearBackground(world::sky::before_multiply(sky.horizon,sky.ambient));
        DrawRectangleGradientV(0,0,scene_width,scene_height*3/5,world::sky::before_multiply(sky.zenith,sky.ambient),
            world::sky::before_multiply(sky.horizon,sky.ambient));

        // High in the drop everything is far away: a farther near plane keeps the depth buffer
        // precise enough that roads do not flicker through the ground below.
        rlSetClipPlanes(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed ? .6 : RL_CULL_DISTANCE_NEAR,RL_CULL_DISTANCE_FAR);
        render::shading_frame(sky);
        BeginMode3D(camera);
        render::shading_begin();

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
        // The people on the buses (seated, two by two down the aisle) and those waiting at the stops.
        if(bus_line.running()) {
            bus_rider_pose.advance(characters::AnimationAction::Idle,dt,0);
            for(const auto& bus:bus_line.buses()) {
                if(!bus.running || Vector3DistanceSqr(bus.position,camera.position)>70*70) continue;
                const auto* asset=vehicles.asset(verda_region,bus.vehicle);
                const auto* type=asset ? vehicles.definition(*asset) : nullptr;
                if(!asset || !type) continue;
                const float scale=asset->size.x/type->width;
                characters::BodyPose sitting;sitting.seated=true;
                for(std::size_t k=0;k<bus.riders.size() && k<20;++k) {
                    if(bus.riders[k].character.empty()) continue;
                    const int row=static_cast<int>(k/2);
                    const Vector3 local{k%2 ? -.62F : .62F,type->seat.y,type->seat.z-1.6F-static_cast<float>(row)*.95F};
                    const auto seat=game::vehicles::VehicleSystem::world_point(local,asset->position,asset->rotation_y,scale);
                    character_renderer.draw(bus.riders[k].character,{seat.x,seat.y+.1F-.92F,seat.z},asset->rotation_y,1,&bus_rider_pose,sitting);
                }
            }
            for(const auto& stop:bus_line.stops()) {
                if(stop.crowd.empty() || Vector3DistanceSqr(stop.shelter,camera.position)>70*70) continue;
                Vector3 to_road=Vector3Subtract(stop.kerb,stop.shelter);to_road.y=0;
                const float length=Vector3Length(to_road);
                to_road=length>.1F ? Vector3Scale(to_road,1/length) : Vector3{0,0,1};
                const Vector3 along{-to_road.z,0,to_road.x};
                const float facing=std::atan2(to_road.x,to_road.z)*RAD2DEG;
                for(std::size_t k=0;k<stop.crowd.size() && k<8;++k) {
                    const float side=(static_cast<float>(k)-3.5F)*.9F;
                    Vector3 at=Vector3Add(Vector3Add(stop.shelter,Vector3Scale(to_road,1.6F+.3F*static_cast<float>(k%2))),Vector3Scale(along,side));
                    at.y=world::terrain::TerrainHeight::sample(at.x,at.z);
                    at.y=std::max(at.y,world::physics::WorldCollision::ground_height({at.x,at.y+1.5F,at.z},at.y+1.5F,verda_region));
                    character_renderer.draw(stop.crowd[k],at,facing+static_cast<float>((k*37)%30)-15,1,&bus_rider_pose,characters::BodyPose{});
                }
            }
        }
        // La Verda Basballigo at its parks: the crowd in the stands (home fans on the first-base side,
        // the visitors' followers across the diamond), both clubs on the field, the ball, a brawl when
        // tempers go, and after the final a riot's fires.
        if(game::rules_for(game_mode).civilian_life && !league.fixtures().empty()) {
            using namespace game::sports;
            namespace BP=world::ballpark;
            const double now=island_life.clock().minutes;
            crowd_pose.advance(characters::AnimationAction::Idle,dt,0);
            cheer_pose.advance(characters::AnimationAction::Walk,dt,.6F);
            brawl_pose.advance(characters::AnimationAction::Attack,dt,0);
            field_pose.advance(characters::AnimationAction::Idle,dt,0);
            run_pose.advance(characters::AnimationAction::Run,dt,7.0F);
            walk_pose.advance(characters::AnimationAction::Walk,dt,1.5F);
            const auto jersey=[&](int team){return team>=0 && team<5 ? outfits.team[static_cast<std::size_t>(team)].id : 0U;};
            for(int p=0;p<5;++p) {
                const auto& site=parks[static_cast<std::size_t>(p)];
                if(site.team<0 || Vector3DistanceSqr(site.origin,camera.position)>280*280) continue;
                // The scoreboard's face: the line score (or the club's name), on the board beyond centre field (facing the plate).
                if(park_boards.on[static_cast<std::size_t>(p)]) {
                    const auto& face=park_boards.face[static_cast<std::size_t>(p)];
                    const Vector3 tl=site.to_world({7.7F,9.75F,59.3F}),tr=site.to_world({-7.7F,9.75F,59.3F});
                    const Vector3 br=site.to_world({-7.7F,5.25F,59.3F}),bl=site.to_world({7.7F,5.25F,59.3F});
                    rlDisableBackfaceCulling();
                    rlSetTexture(face.texture.id);
                    rlBegin(RL_QUADS);
                    rlColor4ub(255,255,255,255);
                    rlTexCoord2f(0,1);rlVertex3f(tl.x,tl.y,tl.z);
                    rlTexCoord2f(0,0);rlVertex3f(bl.x,bl.y,bl.z);
                    rlTexCoord2f(1,0);rlVertex3f(br.x,br.y,br.z);
                    rlTexCoord2f(1,1);rlVertex3f(tr.x,tr.y,tr.z);
                    rlEnd();
                    rlSetTexture(0);
                    rlEnableBackfaceCulling();
                }
                const auto live=league.live(p,now);
                const bool riot=league.rioting(p,now);
                if(live.phase==Phase::Quiet && !riot) continue;
                const int fixture=live.fixture>=0 ? live.fixture : static_cast<int>(league.fixtures().size())-1;
                const auto& g=league.fixtures()[static_cast<std::size_t>(fixture)];
                const float facing=site.yaw;
                // The crowd: the town's own people who came to watch (island life), in their seats -
                // the home crowd by first base, the visitors' followers by third.
                if(live.phase!=Phase::Quiet) {
                    const auto it=game_fans.find(fixture);
                    const float off=Vector3Distance(site.origin,camera.position);
                    const int step=off>120 ? 2 : 1;   // fewer bodies from further off (a phone draws every one)
                    if(it!=game_fans.end())
                        for(int k=0;k<static_cast<int>(it->second.size());k+=step) {
                            const auto& fan=it->second[static_cast<std::size_t>(k)];
                            const auto& residents=island_life.residents();
                            if(fan.resident<0 || fan.resident>=static_cast<int>(residents.size())) continue;
                            const auto& r=residents[static_cast<std::size_t>(fan.resident)];
                            if(!island_life.in_the_stands(r)) continue;
                            const int row=fan.seat%BP::rows, place=fan.seat/BP::rows;
                            const Vector3 seat=site.seat(fan.home,row,place);
                            if(Vector3DistanceSqr(seat,camera.position)>170*170 || !view.sphere(seat,1.2F)) continue;
                            if(stand_seat.on && Vector3DistanceSqr(seat,stand_seat.at)<.6F*.6F) continue;   // the player's seat
                            const bool cheering=(fan.home ? park_cheer_home : park_cheer_away)[static_cast<std::size_t>(p)]>match_time;
                            characters::BodyPose pose;
                            const bool up=cheering && (k%3!=0);
                            pose.seated=!up;
                            const Vector3 at=up ? seat : Vector3{seat.x,seat.y-.42F,seat.z};
                            character_renderer.draw(r.character_id,at,site.seat_yaw(fan.home)+static_cast<float>((k*29)%24)-12,1,up ? &cheer_pose : &crowd_pose,pose);
                        }
                }
                // The game itself (game::sports::scene): the pitch, the swing, the ball in play, the
                // throw, the runners - and the teams lined up before it and celebrating after.
                if(Vector3DistanceSqr(site.origin,camera.position)<200*200) {
                    const auto line=league.linescore(fixture);
                    const auto sc=game::sports::scene(g,line,now-league.first_pitch_at(fixture),60.0F/std::max(1.0F,island_life.config().time_scale));
                    for(const auto& f:sc.figures) {
                        Vector3 feet=site.to_world(f.at);
                        if(Vector3DistanceSqr(feet,camera.position)>170*170 || !view.sphere(Vector3Add(feet,{0,1,0}),1.3F)) continue;
                        characters::BodyPose pose;
                        pose.outfit=f.role==Role::Umpire ? 0U : jersey(f.home ? g.home : g.away);
                        pose.play_phase=f.phase;
                        // His own gait: running or walking at the speed he is really covering ground
                        // (each man out of step with the next), standing still otherwise; and his
                        // facing turning to where he's going rather than snapping round.
                        const std::uint64_t key=static_cast<std::uint64_t>(p)*1000000ULL+(f.home ? 100000ULL : 0ULL)+
                            static_cast<std::uint64_t>(f.role)*1000ULL+static_cast<std::uint64_t>(f.index+100);
                        auto& body=field_bodies[key];
                        const float target_yaw=site.yaw+f.yaw;
                        const bool moving=(f.motion==Motion::Run || f.motion==Motion::Walk) && f.speed>.3F;
                        const auto action=moving ? (f.motion==Motion::Run && f.speed>2.6F ? characters::AnimationAction::Run : characters::AnimationAction::Walk)
                                                 : characters::AnimationAction::Idle;
                        const float pace=moving ? std::min(f.speed,9.0F) : 0.0F;
                        if(match_time-body.seen>1.0) {
                            body.yaw=target_yaw;
                            body.gait.advance(action,static_cast<float>(key%97)*.011F,pace);   // a step of his own
                        }
                        body.seen=match_time;
                        body.gait.advance(action,dt,pace);
                        const float turn=std::remainder(target_yaw-body.yaw,360.0F);
                        body.yaw+=turn*std::min(1.0F,dt*(moving ? 9.0F : 12.0F));
                        characters::AnimationController* anim=&body.gait;
                        switch(f.motion) {
                            case Motion::Run:case Motion::Walk:break;
                            case Motion::Sit:pose.seated=true;anim=&crowd_pose;feet.y+=.03F;break;
                            case Motion::Ready:pose.play=characters::BodyPlay::Ready;break;
                            case Motion::Squat:pose.play=characters::BodyPlay::Squat;break;
                            case Motion::Stance:pose.play=characters::BodyPlay::Stance;break;
                            case Motion::Swing:pose.play=characters::BodyPlay::Swing;break;
                            case Motion::Pitch:pose.play=characters::BodyPlay::Pitch;break;
                            case Motion::Throw:pose.play=characters::BodyPlay::Throw;break;
                            case Motion::Catch:pose.play=characters::BodyPlay::Catch;break;
                            case Motion::Scoop:pose.play=characters::BodyPlay::Scoop;break;
                            case Motion::Cheer:pose.play=characters::BodyPlay::Cheer;feet.y+=.35F*std::abs(std::sin(f.phase*2*PI));break;
                            default:break;
                        }
                        character_renderer.draw(f.role==Role::Umpire ? std::string("character_18_police") : outfit_body,feet,body.yaw,1,anim,pose);
                    }
                    // Forget the players of plays long gone.
                    if(field_bodies.size()>400) std::erase_if(field_bodies,[&](const auto& kv){return match_time-kv.second.seen>10;});
                    for(const auto& bat:sc.bats) {
                        const Vector3 a=site.to_world(bat.grip),b=site.to_world(bat.tip);
                        if(Vector3DistanceSqr(a,camera.position)<120*120) DrawCylinderEx(a,b,.018F,.034F,6,Color{196,160,112,255});
                    }
                    if(sc.ball) DrawSphere(site.to_world(sc.ball_at),.09F,RAYWHITE);
                    // The tracer: a bright line along the batted ball's flight, so you see where it went.
                    if(sc.trail.size()>1 && Vector3DistanceSqr(site.origin,camera.position)<180*180) {
                        const Color glow=Fade(Color{255,240,120,255},sc.trail_fade);
                        for(std::size_t k=1;k<sc.trail.size();++k) {
                            const Vector3 a=site.to_world(sc.trail[k-1]),b=site.to_world(sc.trail[k]);
                            if(Vector3DistanceSqr(a,b)<1e-4F) continue;
                            DrawCylinderEx(a,b,.11F,.11F,6,glow);
                        }
                    }
                }
                // A brawl: fans of both clubs at it in the car park behind the grandstand, by the gate.
                const bool brawl=park_fight_until[static_cast<std::size_t>(p)]>match_time || riot;
                if(brawl) {
                    const int pairs=riot ? 8 : 3;
                    for(int k=0;k<pairs;++k) {
                        const Vector3 c{static_cast<float>(k-pairs/2)*3.2F,0,-63.0F-static_cast<float>(k%2)*2.4F};
                        const Vector3 a=site.to_world({c.x-.45F,0,c.z}),b=site.to_world({c.x+.45F,0,c.z});
                        characters::BodyPose home_pose;home_pose.outfit=jersey(g.home);
                        characters::BodyPose away_pose;away_pose.outfit=jersey(g.away);
                        character_renderer.draw(outfit_body,a,facing+90,1,&brawl_pose,home_pose);
                        character_renderer.draw(outfit_body,b,facing-90,1,&brawl_pose,away_pose);
                    }
                }
                // The riot's fires: burning bins in the car park and on the field.
                if(riot)
                    for(int k=0;k<5;++k) {
                        const Vector3 at=site.to_world({static_cast<float>(k-2)*11.0F,0,k%2 ? -62.0F : -26.0F});
                        const float flick=.75F+.25F*std::sin(static_cast<float>(match_time)*13+static_cast<float>(k)*2);
                        DrawCylinder(at,.45F,.45F,.9F,10,Color{60,62,66,255});
                        DrawSphere(Vector3Add(at,{0,1.2F,0}),.55F*flick,Fade(ORANGE,.85F));
                        DrawSphere(Vector3Add(at,{0,1.7F,0}),.35F*flick,Fade(YELLOW,.8F));
                        DrawSphere(Vector3Add(at,{0,3.0F+flick,0}),1.1F,Fade(DARKGRAY,.35F));
                    }
            }
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
        // Everyone else at a wheel, drawn before the cars so the glass lies over them: residents
        // driving their own cars, police and ambulance crews on the move, anyone else at the wheel
        // of an autopiloted car (somebody from round here), and the player, seen from outside.
        {
            std::unordered_set<std::string> taken;
            for(const auto& bus:bus_line.buses()) taken.insert(bus.vehicle);
            for(const auto& d:ranches.drivers()) taken.insert(d.vehicle);
            for(const auto& d:freight.drivers()) taken.insert(d.vehicle);
            const std::string mine=vehicles.driver() ? vehicles.driver()->id : std::string();
            car_seat_pose.advance(characters::AnimationAction::Idle,dt,0);
            // Where a sitting body's hips go: the seat, low enough that the head clears the roof
            // (a car's seat height is set for the driver's eyes and the camera, not a body).
            const auto car_hips=[&](const world::WorldAsset& asset,const game::vehicles::VehicleDefinition& type) {
                const float scale=asset.size.x/type.width;
                Vector3 local=type.seat;
                local.y=std::min(local.y,type.height-1.14F);
                return game::vehicles::VehicleSystem::world_point(local,asset.position,asset.rotation_y,scale);
            };
            const auto sit=[&](const std::string& vehicle,const std::string& character) {
                if(character.empty() || vehicle==mine || !taken.insert(vehicle).second) return;
                const auto* asset=vehicles.asset(verda_region,vehicle);
                const auto* type=asset ? vehicles.definition(*asset) : nullptr;
                if(!asset || !type || asset->vehicle.destroyed || Vector3DistanceSqr(asset->position,camera.position)>90*90) return;
                if(asset->vehicle.mods[9]>=2) return;   // limo tint: nobody sees in
                const auto seat=car_hips(*asset,*type);
                characters::BodyPose sitting;sitting.seated=true;
                character_renderer.draw(character,{seat.x,seat.y+.1F-.92F,seat.z},asset->rotation_y,1,&car_seat_pose,sitting);
            };
            if(game::rules_for(game_mode).civilian_life) {
                const auto& owned=island_life.island().cars;
                const auto& states=island_life.cars();
                for(std::size_t k=0;k<states.size() && k<owned.size();++k) {
                    if(!states[k].driving || !states[k].physical || states[k].lost) continue;
                    const int owner=owned[k].owner;
                    if(owner>=0 && owner<static_cast<int>(island_life.residents().size()))
                        sit(owned[k].vehicle,island_life.residents()[static_cast<std::size_t>(owner)].character_id);
                }
                for(const auto& u:services.units()) {
                    using game::services::UnitState;
                    if(!u.crew.empty() || (u.state!=UnitState::Responding && u.state!=UnitState::Pursuit && u.state!=UnitState::Returning && u.state!=UnitState::Patrol)) continue;
                    sit(u.vehicle,u.kind==game::services::UnitKind::Ambulance ? "character_25_doctor" :
                        u.kind==game::services::UnitKind::Highway ? game::services::EmergencyServices::trooper_characters[0] : "character_18_police");
                }
            }
            for(const auto& car:vehicles.vehicles()) {
                if(!vehicles.autopiloted(car.id) || taken.contains(car.id)) continue;
                if(const auto* who=character_registry.choose(characters::CharacterPool::Civilian,std::hash<std::string>{}(car.id))) sit(car.id,who->id);
            }
            // Uncle Ron riding along in the outbreak: in the passenger seat.
            if(outbreak.active() && !outbreak.ron_car().empty())
                if(const auto* asset=vehicles.asset(verda_region,outbreak.ron_car())) if(const auto* type=vehicles.definition(*asset)) {
                    std::string body;
                    for(const auto& p:outbreak.people()) if(p.name==game::outbreak::Story::ron) body=p.character;
                    Vector3 local=type->seat;
                    local.x=-local.x;
                    local.y=std::min(local.y,type->height-1.14F);
                    const auto seat=game::vehicles::VehicleSystem::world_point(local,asset->position,asset->rotation_y,asset->size.x/type->width);
                    characters::BodyPose sitting;sitting.seated=true;
                    if(!body.empty() && Vector3DistanceSqr(seat,camera.position)<90*90)
                        character_renderer.draw(body,{seat.x,seat.y+.1F-.92F,seat.z},asset->rotation_y,1,&car_seat_pose,sitting);
                }
            // The player at the wheel, seen from outside (not from the cab), in their own clothes.
            if(!mine.empty() && player.third_person && !vehicle_renderer.cab() && !riding_now) {
                const auto* asset=vehicles.asset(verda_region,mine);
                const auto* type=asset ? vehicles.definition(*asset) : nullptr;
                if(asset && type && asset->vehicle.mods[9]<2) {
                    const auto seat=car_hips(*asset,*type);
                    const auto* look=game::wardrobe::wearing(loot_session.economy(),wardrobe_catalogue);
                    const bool uniform=game_mode==game::GameMode::Explore && career.on_duty() && character_registry.find(police_uniform);
                    character_renderer.set_player(uniform ? std::string(police_uniform) : look ? look->character : std::string());
                    characters::BodyPose body;body.seated=true;
                    body.outfit=uniform ? 0U : look ? outfit_texture(look->outfit) : 0U;
                    character_renderer.draw_player({seat.x,seat.y+.1F-.92F,seat.z},asset->rotation_y*DEG2RAD,body);
                }
            }
        }
        vehicle_renderer.draw(vehicles,verda_region,camera.position);
        // The outbreak: a red light blinking on the black tower's roof, night and day.
        if(outbreak.active() && outbreak.tower().found) {
            const auto& tower=outbreak.tower();
            const bool on=std::fmod(GetTime(),1.8)<.3;
            const Vector3 top{tower.centre.x,tower.centre.y+tower.size.y+.8F,tower.centre.z};
            DrawSphere(top,on ? 1.1F : .6F,on ? Color{255,30,30,255} : Color{90,10,10,255});
            if(on) DrawSphere(top,2.6F,Color{255,40,40,60});
        }
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
            for(const auto& cow:ranches.cattle_quay().pen) draw_cow(cow);
        }
        double sub_t3=GetTime();
        sign_renderer.draw(camera.position,camera.target,static_cast<float>(GetTime()));
        // The traffic signals' lamps, on island traffic's signal clock.
        render::draw_signals(island_life.signals(),island_life.signal_clock(),camera.position);
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
        if(roof_gun_ready) game::combat::CombatRenderer::draw_world(roof_gun,combat_world);
        // Roof guns on the rigs fitted with them: a post and the gun, aimed where the driver looks (or ahead).
        for(const auto& v:vehicles.vehicles()) {
            const auto* asset=vehicles.asset(verda_region,v.id);
            if(!asset || !asset->vehicle.gun || Vector3DistanceSqr(v.position,camera.position)>200*200) continue;
            const auto* def=vehicles.definition(*asset);
            const float roof=v.position.y+(def ? def->height : 3.0F);
            const Vector3 mount{v.position.x,roof+.45F,v.position.z};
            const bool mine=vehicles.driver() && vehicles.driver()->id==v.id;
            const float a=v.yaw*DEG2RAD;
            Vector3 aim=mine ? Vector3Normalize(Vector3Subtract(Vector3Add(camera.position,Vector3Scale(camera_direction,300.0F)),mount)) : Vector3{std::sin(a),0,std::cos(a)};
            DrawCylinder({v.position.x,roof,v.position.z},.09F,.12F,.45F,8,Color{40,42,46,255});
            DrawCylinder({v.position.x,roof+.05F,v.position.z},.55F,.55F,.08F,12,Color{70,74,78,255});   // the ring mount
            game::combat::CombatRenderer::draw_gun(Vector3Subtract(mount,Vector3Scale(aim,.25F)),aim,{0,1,0},game::combat::WeaponId::Rifle,0,
                                                   mine ? roof_gun.muzzle_flash() : 0,false);
        }
        // A drive-by's flash at the driver's window.
        if((drive_by || ride_by) && weapons.muzzle_flash()>0) {
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
        if(weapons.available() && !driving && !creator_active && !player_health.downed() && (!player.third_person || ads_view) && !scoped)
            game::combat::CombatRenderer::draw_gun(gun_grip,look_direction,view_up,weapons.selected(),gun_reload,weapons.muzzle_flash(),true,gun_work,sight);
        if(!creator_active)loot_ui.draw_world(item_registry,loot_session.world(),loot_feet,loot_session.inventory().state().light);

        if(perf.enabled()) perf.mark(FrameStage::World,GetTime());
        // People hold what they carry: police their pistols, Battle Royale bots their loadout.
        // Hats: the Vakeroj's cattleman hats (game::ranch).
        const auto draw_hat=[&](const characters::NpcInstance& actor,const characters::HeldGun& held) {
            if(actor.bot>=0 && ranches.vakero(actor.bot) && actor.state!=characters::NpcState::Dead) draw_cowboy_hat(held);
            if(actor.bot==chief_bot) draw_chief_cap(held);
            if(actor.bot>=0 && endgame.soldier(actor.bot)) draw_helmet(held);
            if(actor.resident>=0 && actor.state!=characters::NpcState::Dead && services.trooper(actor.resident))
                draw_cowboy_hat(held,"assets/verda/ranch/trooper_hat.glb");
        };
        // The VENDATA boards' posts (the boards are signs).
        for(const auto& home:homes_for_sale) {
            if(std::hypot(home.board.x-camera.position.x,home.board.z-camera.position.z)>120 || game::economy::owns(loot_session.economy(),home)) continue;
            const float ground=home.house ? world::terrain::TerrainHeight::sample(home.board.x,home.board.z) : home.centre.y;
            DrawCube({home.board.x,ground+.7F,home.board.z},.09F,1.4F,.09F,Color{120,124,128,255});
        }
        // Vestaĵejo Verda's mannequins (every look but your own clothes), and the hats on the stand.
        if(clothing_store && std::hypot(clothing_store->centre.x-camera.position.x,clothing_store->centre.z-camera.position.z)<60) {
            const auto& looks=wardrobe_catalogue.looks();
            // Only the floor you are on (from the street: the ground floor, through the windows),
            // and the nearest score of those in view (each is a posed body).
            std::vector<std::pair<float,std::size_t>> shown;
            for(std::size_t i=0;i<clothing_store->mannequins.size() && i+1<looks.size();++i) {
                const auto& m=clothing_store->mannequins[i];
                const float d=Vector3Distance(m.at,camera.position);
                if(std::abs(m.at.y-(player.position.y-1))>2.2F || d>34 || !view.sphere({m.at.x,m.at.y+1,m.at.z},1.2F)) continue;
                shown.emplace_back(d,i);
            }
            std::sort(shown.begin(),shown.end());
            if(shown.size()>20) shown.resize(20);
            for(const auto& [d,i]:shown) {
                const auto& m=clothing_store->mannequins[i];
                const auto& look=looks[i+1];
                characters::BodyPose pose;
                pose.outfit=outfit_texture(look.outfit);
                character_renderer.draw(look.character,m.at,m.yaw,1,&mannequin_pose,pose);
            }
            for(std::size_t i=0;i<clothing_store->hats.size() && i<wardrobe_catalogue.hats().size();++i)
                if(Model* hat=get_world_asset_model(wardrobe_catalogue.hats()[i].model,character_registry))
                    DrawModelEx(*hat,clothing_store->hats[i],{0,1,0},clothing_store->hat_yaw,{1.12F,1.12F,1.12F},WHITE);
        }
        character_renderer.draw_npcs(npcs, camera.position, &view,
            [&](const characters::NpcInstance& actor,characters::BodyPose& body) {
                // Every gang in its colours: officers sharp, soldiers in the gang's shirt (the Green Hand's white).
                if(actor.bot>=0 && actor.character_id==outfit_body && mafio.active())
                    if(const int gang=mafio.gang_of(actor.bot);gang>=0) body.outfit=gang_outfit(gang,mafio.officer(actor.bot));
                // The outbreak's people: Dario in the Green Hand's suit, the gangs in their colours.
                if(const auto* who=outbreak.active() ? outbreak.person(actor.bot) : nullptr;who && who->gang>=0 && actor.character_id==outfit_body)
                    body.outfit=gang_outfit(who->gang,who->leader || who->name==game::outbreak::Story::dario);
                // The occupiers in camouflage.
                if(actor.bot>=0 && actor.character_id==outfit_body && endgame.soldier(actor.bot)) body.outfit=outfits.occupier.id;
                // Rioters after the final: their club's shirt.
                if(actor.resident>=rioter_base && actor.resident<rioter_base+40) {body.outfit=outfits.team[static_cast<std::size_t>(std::clamp(riot_team,0,4))].id;return;}
                // A gang's gun man: the gang's colours, a rifle slung (in his hands).
                if(actor.bot>=gun_man_bot && actor.bot<gun_man_bot+5) body.outfit=gang_outfit(actor.bot-gun_man_bot,true);
                // La Verda Mano at their chapel on a Sunday: the suits.
                if(actor.bot>=crowd_bot && actor.bot<crowd_bot+crowd_green) {body.outfit=actor.bot==crowd_bot ? outfits.kapo.id : outfits.soldato.id;return;}
                // The Green Hand's cultural society: their colours, no guns.
                if(actor.bot>=society_bot && actor.bot<society_bot+society_size) {body.outfit=actor.bot==society_bot ? outfits.kapo.id : outfits.soldato.id;return;}
                // Another player knocked down (squad play): on their knees, no gun.
                if(game::lan::LanPlay::is_remote_body(actor.bot))
                    if(const auto* who=lan_play.find(game::lan::LanPlay::slot_of(actor.bot));who && down(*who)) {body.downed=1;return;}
                // Arrests: hands up, or cuffed behind the back.
                if(actor.bot>=0 && mafio.active()) {
                    if(callouts.escorting(actor.bot)) {body.cuffed=1;return;}
                    if(mafio.surrendered(actor.bot)) {body.hands_up=1;return;}
                }
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
        if (player.third_person && !driving && !riding_now && bus_ride<0 && !scoped && !ads_view && !(dropping && skydiver.stage()!=game::aircraft::DiveStage::Landed)) {

            const Vector3 character_feet{player.position.x,player.position.y-1.0F,player.position.z};

            characters::BodyPose body;
            crouch_blend+=((controls.crouch ? 1.0F : 0.0F)-crouch_blend)*(1.0F-std::exp(-dt*10.0F));
            body.crouch=crouch_blend;
            body.downed=player_health.downed() ? 1.0F : 0.0F;
            if(weapons.available() && !creator_active && !player_health.downed()) {
                const auto gun=game::combat::gun_layout(weapons.selected());
                body.hold=held_weapon.hold==game::combat::WeaponHold::Pistol ? characters::BodyHold::Pistol : characters::BodyHold::Shouldered;
                body.stock=gun.stock;body.support=gun.support;
                // Sprinting carries the gun low; otherwise the chest follows the sights.
                body.aim_pitch=weapon_input.sprint ? -.55F : shot_pitch;
            }
            // In the clothes you bought (or are trying on at Vestaĵejo Verda).
            const auto& looks=wardrobe_catalogue.looks();
            const auto* look=store_open && store_ui.look()<static_cast<int>(looks.size()) ? &looks[static_cast<std::size_t>(store_ui.look())] :
                game::wardrobe::wearing(loot_session.economy(),wardrobe_catalogue);
            // On the force: the Policio's uniform, whatever is in the wardrobe.
            const bool uniform=!store_open && game_mode==game::GameMode::Explore && career.on_duty() && character_registry.find(police_uniform);
            character_renderer.set_player(uniform ? std::string(police_uniform) : look ? look->character : std::string());
            body.outfit=uniform ? 0U : look ? outfit_texture(look->outfit) : 0U;
            if(stand_seat.on) body.seated=true;   // in the stands
            const auto held=character_renderer.draw_player(character_feet, stand_seat.on ? stand_seat.yaw*DEG2RAD : player.yaw, body);
            const auto* hat=store_open ? (store_ui.hat()>=0 ? &wardrobe_catalogue.hats()[static_cast<std::size_t>(store_ui.hat())] : nullptr) :
                uniform ? nullptr : game::wardrobe::wearing_hat(loot_session.economy(),wardrobe_catalogue);
            if(hat && !scoped) draw_cowboy_hat(held,hat->model.c_str());
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
        if(career.job().kind!=game::career::JobKind::None &&
           std::hypot(career.job().target.x-player.position.x,career.job().target.z-player.position.z)>14) {   // not when you are there
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
        // The load in hand: a green column of light.
        if(trucking.hired() && trucking.job().stage!=game::freight::HaulStage::None &&
           std::hypot(trucking.target().x-player.position.x,trucking.target().z-player.position.z)>game::freight::Trucking::stop_radius) {
            const Vector3 at=trucking.target();
            const float ground=world::terrain::TerrainHeight::sample(at.x,at.z);
            const Color beam{90,220,140,255};
            const float pulse=.75F+.25F*std::sin(static_cast<float>(GetTime())*3);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCylinder({at.x,ground,at.z},1.1F,1.1F,90,10,Fade(beam,.22F*pulse));
            DrawCylinder({at.x,ground,at.z},.35F,.35F,90,8,Fade(beam,.5F*pulse));
            EndBlendMode();
            DrawCircle3D({at.x,ground+.15F,at.z},game::freight::Trucking::stop_radius*.6F,{1,0,0},90,Fade(beam,.8F));
        }
        // The street race: the grid's flare, then the next flare along the ring, burning red.
        if(street_race.ready() && street_race.stage()!=game::racing::Stage::Closed && street_race.stage()!=game::racing::Stage::Open &&
           street_race.stage()!=game::racing::Stage::Done) {
            const float pulse=.7F+.3F*std::sin(static_cast<float>(GetTime())*7);
            const auto flare=[&](Vector3 at,float height) {
                const float ground=world::terrain::TerrainHeight::sample(at.x,at.z);
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCylinder({at.x,ground,at.z},1.4F,1.4F,height,10,Fade(Color{255,70,40,255},.25F*pulse));
                DrawCylinder({at.x,ground,at.z},.4F,.4F,height,8,Fade(Color{255,120,60,255},.55F*pulse));
                DrawSphere({at.x,ground+.4F,at.z},.5F,Fade(Color{255,60,30,255},.9F*pulse));
                EndBlendMode();
            };
            flare(street_race.target(),60);
            if(street_race.stage()==game::racing::Stage::Racing) flare(street_race.finish(),120);
        }
        // Pasko night: a candle in every hand outside the church.
        if(crowd_candles)
            for(const auto& at:crowd_spots) {
                if(Vector3DistanceSqr(at,camera.position)>80*80) continue;
                const float flicker=.8F+.2F*std::sin(static_cast<float>(GetTime())*13+at.x);
                DrawCylinder({at.x+.25F,at.y+1.0F,at.z},.025F,.025F,.22F,6,Color{236,226,196,255});
                BeginBlendMode(BLEND_ADDITIVE);
                DrawSphere({at.x+.25F,at.y+1.27F,at.z},.05F*flicker,Color{255,200,90,255});
                DrawSphere({at.x+.25F,at.y+1.27F,at.z},.3F*flicker,Fade(Color{255,160,60,255},.3F));
                EndBlendMode();
            }
        // The waypoint set on the map: a pink column of light.
        if(const auto& pin=map_screen.pin();pin && std::hypot(pin->x-player.position.x,pin->z-player.position.z)>18) {
            const float ground=world::terrain::TerrainHeight::sample(pin->x,pin->z);
            const float pulse=.75F+.25F*std::sin(static_cast<float>(GetTime())*2.4F);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCylinder({pin->x,ground,pin->z},1.0F,1.0F,120,10,Fade(Color{255,110,210,255},.2F*pulse));
            DrawCylinder({pin->x,ground,pin->z},.3F,.3F,120,8,Fade(Color{255,110,210,255},.5F*pulse));
            EndBlendMode();
        }
        game::career::draw_camp(police_camp,camera.position,static_cast<float>(GetTime()),
            [](float x,float z){return world::terrain::TerrainHeight::sample(x,z);});
        for(const auto& tag:territory.tags()) gang_logos.draw(tag,camera.position);
        // The gangs' houses wear their tag (whoever holds the town now), sprayed on the front wall
        // beside the door: found once by a ray from the street at the wall.
        house_tags.resize(mafio.crews().size());
        for(std::size_t i=0;i<mafio.crews().size();++i) {
            const auto& crew=mafio.crews()[i];
            if(!crew.house || crew.disbanded || Vector3Distance(crew.tag,camera.position)>140) continue;
            auto& spot=house_tags[i];
            if(!spot.found) {
                spot.found=true;spot.at=crew.tag;spot.normal=crew.tag_normal;
                const Vector3 from=Vector3Add(crew.tag,Vector3Scale(crew.tag_normal,2.5F));
                const auto hit=combat_world.trace_segment(from,Vector3Subtract(crew.tag,Vector3Scale(crew.tag_normal,2.5F)),false,false,false);
                if(hit.hit() && Vector3Length(hit.normal)>.5F) {spot.at=hit.position;spot.normal=Vector3Normalize(hit.normal);}
            }
            gang_logos.draw({spot.at,spot.normal,crew.gang,-1},camera.position);
        }
        if(green_hand_mural.placed && mafio.casino() && mafio.casino()->gang==0)   // painted over if the city falls
            gang_logos.draw_mural(green_hand_mural.centre,green_hand_mural.normal,green_hand_mural.width,camera.position);
        for(const auto& car:vehicles.vehicles())
            if(Vector3Distance(car.position,camera.position)<120)
                if(const auto* a=vehicles.asset(verda_region,car.id)) {
                    if(a->vehicle.definition==game::mafia::green_hand_car)
                        game::mafia::draw_green_hand_livery(car.position,car.yaw,sign_renderer.verda_mano_texture(),camera.position);
                    else if(a->vehicle.definition=="highway_patrol") patrol_art.draw_livery(car.position,car.yaw,camera.position);
                }
        for(int i=0;i<patrol_open && i<static_cast<int>(patrol_posts.size());++i)
            patrol_art.draw_outpost(patrol_posts[static_cast<std::size_t>(i)],camera.position);
        if(services.active()) game::services::draw_light_bars(services,vehicles,camera.position,static_cast<float>(GetTime()));
        if(const int lit=lights_car.empty() ? -1 : vehicles.find(lights_car);lit>=0) {
            const auto& car=vehicles.vehicles()[static_cast<std::size_t>(lit)];
            const auto* asset=vehicles.asset(verda_region,car.id);
            const bool interceptor=asset && asset->vehicle.definition=="interceptor";
            game::services::draw_light_bar(car.position,car.yaw,interceptor ? 1.36F : 1.6F,interceptor ? 0.0F : -.1F,interceptor ? .45F : .42F,
                                           lit,camera.position,static_cast<float>(GetTime()));
        }
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
        render::shading_end();
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
        render::draw_signals(island_life.signals(),island_life.signal_clock(),camera.position,true);
        if(services.active()) game::services::draw_light_bars(services,vehicles,camera.position,static_cast<float>(GetTime()));
        if(const int lit=lights_car.empty() ? -1 : vehicles.find(lights_car);lit>=0) {
            const auto& car=vehicles.vehicles()[static_cast<std::size_t>(lit)];
            const auto* asset=vehicles.asset(verda_region,car.id);
            const bool interceptor=asset && asset->vehicle.definition=="interceptor";
            game::services::draw_light_bar(car.position,car.yaw,interceptor ? 1.36F : 1.6F,interceptor ? 0.0F : -.1F,interceptor ? .45F : .42F,
                                           lit,camera.position,static_cast<float>(GetTime()));
        }
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

        // (No title, mode name or camera label in play: as much of the game in view as can be.)

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
        // At the wheel of a big rig: the trailer (dropped or hitched) - J, or a tap here on a touch screen.
        if(const auto* car=vehicles.driver();car && !creator_active && !inventory_open)
            if(const auto* asset=vehicles.asset(verda_region,car->id);asset && asset->vehicle.definition=="semi") {
                const bool on=freight.hitched(car->id);
                if(touch_ui) {
                    const Vector2 spot{screen_width*.42F-150,screen_height-92.0F};
                    DrawCircleV(spot,34,Fade(BLACK,.45F));
                    const char* l=on ? "DROP" : "HITCH";
                    DrawText(l,static_cast<int>(spot.x-MeasureText(l,16)/2.0F),static_cast<int>(spot.y-8),16,on ? RAYWHITE : ORANGE);
                } else if(std::abs(car->speed)<1) {
                    const char* l=on ? "J - DROP THE TRAILER" : "J - HITCH THE TRAILER";
                    DrawText(l,24,screen_height-236,16,on ? Color{220,220,220,200} : ORANGE);
                }
                if(asset->vehicle.gun && !touch_ui) DrawText("ROOF GUN - LMB FIRE, LOOK TO AIM",24,screen_height-256,16,Color{240,200,90,220});
            }
        // Driving with a pistol or SMG: what is in the hand for a drive-by (over the speedometer).
        if((drive_by || ride_by) && !creator_active && !inventory_open) {
            const char* line=TextFormat(ride_by ? "PASSENGER  %s  %d / %d   %s" : "DRIVE-BY  %s  %d / %d   %s",weapons.weapon().name,weapons.ammo().loaded,weapons.ammo().reserve,
                touch_ui ? "" : "LMB FIRE - LOOK TO AIM");
            const int lw=MeasureText(line,18);
            DrawRectangle(12,screen_height-206,lw+24,30,Fade(BLACK,.6F));
            DrawText(line,24,screen_height-200,18,weapons.ammo().loaded>0 ? RAYWHITE : ORANGE);
        }
        if (touch_ui && !creator_active && !inventory_open && !build_mode.active() && !driving) {
            // On a touch screen the stick lives bottom left: the gun is one short line up top instead.
            const char* held=weapons.available() ? TextFormat("%s  %d/%d",weapons.weapon().name,weapons.ammo().loaded,weapons.ammo().reserve) : weapons.holstered() ? "UNARMED" : "NO WEAPON - BAG";
            const bool empty=weapons.available() && weapons.ammo().loaded==0 && !weapons.unlimited();
            const char* line=weapons.reload_remaining()>0 ? TextFormat("%s  RELOADING",held) : empty ? TextFormat("%s  EMPTY - LOAD",held) : held;
            DrawText(line,17,49,16,Color{0,0,0,150});
            DrawText(line,16,48,16,weapons.reload_remaining()>0 || empty ? ORANGE : RAYWHITE);
        }
        else if (!creator_active && !inventory_open && !build_mode.active() && !driving) {
            DrawRectangle(12,screen_height-103,320,weapons.reload_remaining()>0 || (weapons.available() && weapons.ammo().loaded==0) ? 91 : 68,Fade(BLACK,.65F));
            {
                // Long names (VR7 ROCKET LAUNCHER) shrink to stay clear of the sight label.
                const char* held=weapons.available()?weapons.weapon().name:weapons.holstered() ? "UNARMED" : "NO WEAPON - OPEN BAG";
                int size=19;
                while(size>12 && weapons.available() && MeasureText(held,size)>170) --size;
                DrawText(held,24,screen_height-94+(19-size)/2,size,RAYWHITE);
            }
            DrawText(TextFormat("NADES %d",grenades.count()),222,screen_height-68,16,grenades.count()>0 ? Color{190,220,150,255} : GRAY);
            if(const int armour=armour_level(); armour>0)
                DrawText(armour==2 ? "ARMOUR II  -45%" : "ARMOUR I  -30%",200,screen_height-112,14,game::inventory::LootUI::rarity_color(armour==2 ? game::inventory::Rarity::Epic : game::inventory::Rarity::Rare));
            if(weapons.available()) DrawText(game::combat::sight_info(sight).name,
                200,screen_height-92,14,sight==game::combat::Sight::Iron ? GRAY : Color{235,120,110,255});
            if (weapons.unlimited()) DrawText("AMMO UNLIMITED - DEV LAB",24,screen_height-68,18,YELLOW);
            else if(!weapons.holstered()) DrawText(TextFormat("%d / %d",weapons.ammo().loaded,weapons.ammo().reserve),
                24,screen_height-68,22,RAYWHITE);
            else DrawText(TextFormat("%s: draw a gun",key_of(input::Action::NextWeapon).c_str()),24,screen_height-64,14,GRAY);
            if (weapons.reload_remaining()>0) DrawText(TextFormat("RELOADING %.1fs",weapons.reload_remaining()),
                24,screen_height-39,16,ORANGE);
            else if(weapons.available() && weapons.ammo().loaded==0 && !weapons.unlimited()) {
                // Only what needs doing: no standing list of controls (they are in the menu).
                const std::string line=touch_ui ? std::string("EMPTY - LOAD TO RELOAD") : "EMPTY - "+key_of(input::Action::Reload)+" TO RELOAD";
                DrawText(line.c_str(),24,screen_height-39,12,ORANGE);
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
            lan_play.draw_names(camera,screen_width,screen_height,[&](std::uint8_t slot){return teammate(slot);});
            if(game_mode!=game::GameMode::BattleRoyale) {
                const char* people=TextFormat("LAN - %d PLAYERS",1+static_cast<int>(lan_play.remotes().size()));
                DrawText(people,screen_width/2-MeasureText(people,18)/2,16,18,Color{120,220,255,235});
                int line=0;
                for(const auto& entry:br_bots.feed()) DrawText(entry.c_str(),screen_width/2-MeasureText(entry.c_str(),16)/2,40+20*line++,16,Color{235,215,180,255});
            }
        }
        if (!creator_active) {
            // Health: a slim bar over the compass's left end, out of the way of the view.
            {
                const float share=std::clamp(player_health.current()/std::max(1.0F,player_health.maximum()),0.0F,1.0F);
                const int bw=140,bx=screen_width/2-232,by=12;
                DrawRectangle(bx-2,by-2,bw+4,14,Fade(BLACK,.55F));
                DrawRectangle(bx,by,static_cast<int>(bw*share),10,share>.5F ? Color{90,210,110,235} : share>.25F ? Color{235,190,70,235} : Color{230,70,60,240});
                const char* hp=player_health.alive() ? TextFormat("%.0f",player_health.current()) : "DOWN";
                DrawText(hp,bx+bw+8,by-2,14,player_health.alive() ? RAYWHITE : RED);
                // The boost from food and drink: a thin amber line under it while it lasts.
                if(loot_session.boost()>0 && player_health.alive())
                    DrawRectangle(bx,by+12,static_cast<int>(bw*loot_session.boost()/game::inventory::LootSession::boost_max),3,Color{240,180,60,230});
            }

            if (br_bots.active() && game_mode==game::GameMode::BattleRoyale) {
                const int alive=br_bots.alive_bots()+(player_health.alive() ? 1 : 0)+(lan_match ? lan_play.alive() : 0);
                // The match in a corner (top left, clear of the compass and the health bar): who is
                // left and your kills - and, playing as a side, how many of your side still stand.
                const char* status=TextFormat("ALIVE %d  ·  KILLS %d%s",alive,br_bots.player_kills(),
                                              lan_match && lan_sides!=net::Sides::Free ? TextFormat("  ·  %s %d",lan_sides==net::Sides::Squad ? "SQUAD" : "TEAM",side_alive()) :
                                              lan_match ? TextFormat("  ·  LAN %d",1+static_cast<int>(lan_play.remotes().size())) : "");
                const int sw=MeasureText(status,18);
                DrawRectangle(10,8,sw+16,26,Fade(BLACK,.45F));
                DrawText(status,18,12,18,RAYWHITE);
                // Kill feed down the right, under the MAP and MENU buttons, small.
                int line=0;
                for(const auto& entry:br_bots.feed()) {
                    const int fw=MeasureText(entry.c_str(),14);
                    DrawText(entry.c_str(),screen_width-fw-16,118+18*line++,14,Color{235,215,180,230});
                }
            }
        }
        bool asset_drawer_open = false;
#ifdef OUTLAND_DEV_TOOLS
        asset_drawer_open = game_mode == game::GameMode::DevLab && creator_touch_ui.inventory_open();
#endif
        if (touch_ui && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !mode_session.terminal() && !asset_drawer_open && !pause_menu.is_open()) touch_hud.draw(
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

        if(inventory_open) {
            // The bag's QUESTS: everything in hand, in full (the HUD keeps to one line each).
            std::vector<game::inventory::QuestNote> quests;
            const auto metres=[&](Vector3 at){return std::hypot(at.x-player.position.x,at.z-player.position.z);};
            if(!campaign_path.empty() && !story.objective().empty())
                quests.push_back({"THE STORY",story.objective(),
                                  uncle_ron.placed ? TextFormat("%.0f m",metres(uncle_ron.at)) : "",Color{255,214,120,255}});
            if(career.state().side!=game::career::Side::Civilian) {
                const bool cop=career.on_duty(),trucker=career.trucker();
                const Color tint=cop ? Color{110,160,255,255} : trucker ? Color{230,150,60,255} : game::mafia::gangs[static_cast<std::size_t>(std::max(0,career.state().gang))].color;
                const std::string who=cop ? std::string("POLICIO DE VERDA") : trucker ? std::string("LA FRATECO DE KAMIONISTOJ") :
                    std::string(game::mafia::gangs[static_cast<std::size_t>(std::max(0,career.state().gang))].name);
                const auto& job=career.job();
                if(job.kind!=game::career::JobKind::None) {
                    const int left=static_cast<int>(std::max(0.0,job.deadline-match_time));
                    std::string status=job.kind==game::career::JobKind::Quest ? std::string() :
                        job.kind==game::career::JobKind::RobRig ? std::string(job.taken ? "rig taken - to the hideout" : "find the rig") :
                        std::to_string(job.done)+" of "+std::to_string(job.needed);
                    if(job.kind!=game::career::JobKind::Quest) status+=TextFormat("  ·  %d:%02d left",left/60,left%60);
                    status+=TextFormat("  ·  %.0f m",metres(job.target));
                    quests.push_back({std::string(job.title)+"  ·  "+who,job.detail,status,tint});
                }
                // Who you are on the island: rank, respect, honour and your crew.
                std::string standing=TextFormat("Respect %d",static_cast<int>(career.state().respect));
                if(cop) standing+=TextFormat("  ·  authority %d  ·  %d outposts built",static_cast<int>(career.authority()),career.outposts_built());
                else if(trucker) standing+=TextFormat("  ·  union rate +%d%%",static_cast<int>(std::lround((career.haul_rate()-1)*100)));
                if(!cop) standing+=career.state().notoriety>0 ? TextFormat("  ·  notoriety %d",static_cast<int>(career.state().notoriety)) :
                                   std::string(trucker ? "" : career.state().honourable ? "  ·  honourable" : "  ·  no honour");
                const std::string with=cop ? (player_crew.partner()>=0 ? "Your partner rides with you." : "No partner - ask at a station's desk.") :
                    trucker ? std::string("Every town has a brother in it - Z on the CB calls two to your side.") :
                    TextFormat("Your crew: %d of %d - recruit your gang's men with USE.",static_cast<int>(player_crew.members().size()),game::career::Crew::max_gang);
                quests.push_back({who+"  ·  "+career.rank_name(),
                                  (job.kind!=game::career::JobKind::None ? std::string() : std::string(cop ? "On patrol - a call will come over the radio. " : trucker ? "Listening to the CB - the brotherhood will have work. " : "Waiting for the word from the boss. "))+with,
                                  standing,tint});
            }
            if(trucking.hired() && trucking.job().stage!=game::freight::HaulStage::None) {
                std::string brief=trucking.detail();
                if(!brief.empty()) brief[0]=static_cast<char>(std::toupper(static_cast<unsigned char>(brief[0])));
                quests.push_back({trucking.title()+"  ·  Verda Transporto",brief,TextFormat("%d kr  ·  %.0f m",trucking.job().pay,metres(trucking.target())),Color{90,220,140,255}});
            }
            // The player's record, last (read it any time in the bag).
            if(!stats_path.empty()) {
                game::inventory::QuestNote record{"YOUR STATS","","",Color{170,200,235,255},{}};
                for(const auto& line:stats.lines()) record.table.emplace_back(line.label,line.value);
                quests.push_back(std::move(record));
            }
            loot_ui.set_quests(std::move(quests));
            loot_ui.draw_inventory(item_registry,loot_session,weapons,screen_width,screen_height);
        }
        if(!creator_active && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !mode_session.terminal()){
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
        if(mafio.active() && !poker_open && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open())) {
            const Font& font=sign_renderer.font();
            std::vector<Vector2> tagged; // one tag per spot on screen: a crew standing together shares one
            for(const auto& body:npcs.actors()) {
                if(body.bot<0 || body.hidden || body.state==characters::NpcState::Dead) continue;
                const int gang=mafio.gang_of(body.bot);
                if(gang<0 || Vector3Distance(body.position,camera.position)>13) continue;
                const Vector3 over{body.position.x,body.position.y+2.15F,body.position.z};
                if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) continue;
                const Vector2 at=GetWorldToScreen(over,camera);
                if(std::any_of(tagged.begin(),tagged.end(),[&](Vector2 t){return std::abs(t.x-at.x)<150 && std::abs(t.y-at.y)<24;})) continue;
                const bool angry=mafio.provoked(gang,match_time);
                if(!angry && !underworld()) continue;   // a civilian sees a gangster's colours only when they come for them
                tagged.push_back(at);
                const std::string tag=std::string(angry ? "! " : "")+std::string(game::mafia::gangs[static_cast<std::size_t>(gang)].name);
                const float tw=MeasureTextEx(font,tag.c_str(),15,1).x;
                DrawRectangle(static_cast<int>(at.x-tw/2-5),static_cast<int>(at.y-2),static_cast<int>(tw+10),19,Fade(BLACK,.45F));
                DrawTextEx(font,tag.c_str(),{at.x-tw/2,at.y},15,1,angry ? Color{255,90,80,255} : game::mafia::gangs[static_cast<std::size_t>(gang)].color);
            }
            // Paul Acosta: his name and what he is up to, close by.
            if(walking_man.active()) for(const auto& body:npcs.actors()) {
                if(body.bot!=walking_man.bot() || body.hidden || body.state==characters::NpcState::Dead) continue;
                if(Vector3Distance(body.position,camera.position)>13) break;
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
                if(!v || Vector3Distance(body.position,camera.position)>13) continue;
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
                if(row==1 && !career.trucker() && !trucking.hired()) continue;   // the rigs' news is truckers' business
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
            // Whose turf: a few seconds on arriving, or while you stand at their door.
            const int turf=mafio.turf_at({player.position.x,player.position.y-1,player.position.z},30);
            if(turf!=banner_turf) {banner_turf=turf;banner_time=5;}
            banner_time=std::max(0.0F,banner_time-GetFrameTime());
            if(turf>=0 && banner_time>0) {
                const auto& crew=mafio.crews()[static_cast<std::size_t>(turf)];
                const auto& gang=game::mafia::gangs[static_cast<std::size_t>(crew.gang)];
                const std::string title=std::string(gang.name)+"  ·  "+crew.town+" crew";
                const std::string sub=career.in_gang(crew.gang) ? std::string("Your crew - wait here for the word, bring the rig here") :
                    mafio.provoked(crew.gang,match_time) ? std::string("They are shooting at you - the police won't come") :
                    career.on_duty() ? std::string(gang.meaning)+" - they don't talk to cops" :
                    !career_path.empty() ? std::string(gang.meaning)+" - talk to their boss at the door to join" :
                    std::string(gang.meaning)+" - leave them be and they leave you be";
                const float tw=MeasureTextEx(font,title.c_str(),17,1).x,sw=MeasureTextEx(font,sub.c_str(),14,1).x;
                // A few seconds under the compass on arriving, then out of the way.
                const float alpha=std::clamp(banner_time,0.0F,1.0F);
                const float w=std::max(tw,sw)+30,x=screen_width/2.0F-w/2,y=106;
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),static_cast<int>(w),42,Fade(BLACK,.5F*alpha));
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),4,42,Fade(gang.color,alpha));
                DrawTextEx(font,title.c_str(),{x+15,y+4},17,1,Fade(gang.color,alpha));
                DrawTextEx(font,sub.c_str(),{x+15,y+24},14,1,Fade(Color{220,226,206,255},alpha));
            }
            // The Chief: his name over his head, and what he says.
            if(chief.placed && Vector3Distance(chief.at,camera.position)<14) {
                const Vector3 over{chief.at.x,chief.at.y+2.3F,chief.at.z};
                if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))>0) {
                    const Vector2 at=GetWorldToScreen(over,camera);
                    const std::string title=std::string(game::career::chief_name)+"  ·  Policio de Verda";
                    const char* sub=Vector3Distance(chief.at,{player.position.x,player.position.y-1,player.position.z})<3.2F ? "USE to talk" : "Head of every police department on Verda";
                    const float tw=MeasureTextEx(font,title.c_str(),16,1).x,sw=MeasureTextEx(font,sub,13,1).x,w=std::max(tw,sw);
                    DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),36,Fade(BLACK,.55F));
                    DrawTextEx(font,title.c_str(),{at.x-tw/2,at.y-16},16,1,Color{212,170,60,255});
                    DrawTextEx(font,sub,{at.x-sw/2,at.y+2},13,1,Color{170,200,255,255});
                }
            }
            // Aŭtovendejo Verda: a price on every car on show.
            for(const auto& site:motor_sites) {
                if(site.kind!=world::motor::Kind::CarLot || Vector3Distance(site.origin,camera.position)>60) continue;
                for(std::size_t i=0;i<game::autos::dealer.size() && i<world::motor::car_lot_spots.size()*world::motor::car_lot_rows.size();++i) {
                    const std::size_t lane=world::motor::car_lot_spots.size();
                    const Vector3 over=site.to_world({world::motor::car_lot_spots[i%lane],2.4F,world::motor::car_lot_rows[i/lane]});
                    if(Vector3Distance(over,camera.position)>16 ||
                       Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) continue;
                    const Vector2 at=GetWorldToScreen(over,camera);
                    const auto& sale=game::autos::dealer[i];
                    const char* name=sale.name;
                    const char* price=TextFormat("%d kr  ·  at the desk",sale.price);
                    const float tw=MeasureTextEx(font,name,15,1).x,pw=MeasureTextEx(font,price,13,1).x,w=std::max(tw,pw);
                    DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),36,Fade(BLACK,.55F));
                    DrawTextEx(font,name,{at.x-tw/2,at.y-16},15,1,WHITE);
                    DrawTextEx(font,price,{at.x-pw/2,at.y+2},13,1,GOLD);
                }
            }
            // Uncle Ron: his name over his head; his words in a box; the goal of the moment.
            if(!campaign_path.empty()) {
                if(uncle_ron.placed && Vector3Distance(uncle_ron.at,camera.position)<16) {
                    const Vector3 over{uncle_ron.at.x,uncle_ron.at.y+2.3F,uncle_ron.at.z};
                    if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))>0) {
                        const Vector2 at=GetWorldToScreen(over,camera);
                        const char* title="Uncle Ron  ·  Onklo Ron";
                        const char* sub=Vector3Distance(uncle_ron.at,{player.position.x,player.position.y-1,player.position.z})<3.2F ? "USE to talk" : "Your uncle - born and raised in Verda City";
                        const float tw=MeasureTextEx(font,title,16,1).x,sw=MeasureTextEx(font,sub,13,1).x,w=std::max(tw,sw);
                        DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),36,Fade(BLACK,.55F));
                        DrawTextEx(font,title,{at.x-tw/2,at.y-16},16,1,Color{255,214,120,255});
                        DrawTextEx(font,sub,{at.x-sw/2,at.y+2},13,1,Color{220,226,206,255});
                    }
                }
                if(const auto* line=story.line()) {
                    const float max_w=std::min(820.0F,screen_width-60.0F);
                    std::vector<std::string> lines;std::string row,word;
                    for(std::size_t i=0;i<=line->text.size();++i) {
                        const char c=i<line->text.size() ? line->text[i] : ' ';
                        if(c!=' ') {word+=c;continue;}
                        const std::string trial=row.empty() ? word : row+" "+word;
                        if(MeasureTextEx(font,trial.c_str(),19,1).x>max_w-36 && !row.empty()) {lines.push_back(row);row=word;}
                        else row=trial;
                        word.clear();
                    }
                    if(!row.empty()) lines.push_back(row);
                    const float h=58+22*static_cast<float>(lines.size()),x=screen_width/2-max_w/2,y=screen_height*.70F-h;
                    DrawRectangleRounded({x,y,max_w,h},.08F,8,Fade(Color{12,16,12,255},.82F));
                    DrawRectangle(static_cast<int>(x),static_cast<int>(y+8),4,static_cast<int>(h-16),Color{255,214,120,255});
                    DrawTextEx(font,line->speaker,{x+18,y+10},18,1,Color{255,214,120,255});
                    for(std::size_t k=0;k<lines.size();++k) DrawTextEx(font,lines[k].c_str(),{x+18,y+34+22*static_cast<float>(k)},19,1,Color{236,240,226,255});
                    const std::string more=story.stage()==game::campaign::Stage::Free ? std::string("USE - close") :
                        std::string("USE - continue  (")+std::to_string(story.line_number())+"/"+std::to_string(story.lines())+")";
                    const float mw=MeasureTextEx(font,more.c_str(),14,1).x;
                    DrawTextEx(font,more.c_str(),{x+max_w-mw-16,y+h-22},14,1,Color{170,190,150,255});
                }
                if(const auto goal=story.objective();!goal.empty() && !map_screen.is_open()) {
                    const std::string text="> "+goal;
                    const float gw=MeasureTextEx(font,text.c_str(),19,1).x;
                    DrawRectangle(static_cast<int>(screen_width/2-gw/2-14),132,static_cast<int>(gw+28),32,Fade(BLACK,.55F));
                    DrawTextEx(font,text.c_str(),{screen_width/2-gw/2,138},19,1,Color{255,214,120,255});
                }
                if(autosave_note>0) {
                    const float alpha=std::clamp(autosave_note,0.0F,1.0F);
                    const char* note="Autosaved";
                    const float nw=MeasureTextEx(font,note,15,1).x;
                    DrawTextEx(font,note,{screen_width-nw-18,static_cast<float>(screen_height-40)},15,1,Fade(Color{185,202,139,255},alpha));
                }
            }
            // Uncle Dario at the Green Hand's bar: his name over his head.
            if(!campaign_path.empty()) if(const auto* bar=mafio.casino();bar && bar->gang==0 && !bar->members.empty())
                if(const auto* boss=br_bots.find(bar->members.front());boss && boss->alive() && !br_bots.hidden(bar->members.front()) &&
                   Vector3Distance(boss->position(),camera.position)<34) {
                    const Vector3 over{boss->position().x,boss->position().y+2.3F,boss->position().z};
                    if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))>0) {
                        const Vector2 at=GetWorldToScreen(over,camera);
                        const std::string title=std::string(dario_name)+"  ·  La Verda Mano";
                        const bool close=std::hypot(boss->position().x-player.position.x,boss->position().z-player.position.z)<3.2F;
                        const char* sub=career.in_gang(0) ? (close ? "Your uncle - USE to talk" : "Head of the Green Hand - your uncle") :
                                        close && !career.on_duty() ? "USE to talk - he'll want you in" : "Head of the Green Hand - Ron's brother";
                        const float tw=MeasureTextEx(font,title.c_str(),16,1).x,sw=MeasureTextEx(font,sub,13,1).x,w=std::max(tw,sw);
                        DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),36,Fade(BLACK,.55F));
                        DrawTextEx(font,title.c_str(),{at.x-tw/2,at.y-16},16,1,game::mafia::gangs[0].color);
                        DrawTextEx(font,sub,{at.x-sw/2,at.y+2},13,1,Color{220,226,206,255});
                    }
                }
            // The call: the getaway car's plate, the suspects with their hands up, the cuffed; inside, the snitch.
            {
                const auto tag=[&](Vector3 over,const std::string& title,const std::string& sub,Color tint,float range) {
                    if(Vector3Distance(over,camera.position)>range) return;
                    if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) return;
                    const Vector2 at=GetWorldToScreen(over,camera);
                    const float tw=MeasureTextEx(font,title.c_str(),15,1).x,sw=sub.empty() ? 0 : MeasureTextEx(font,sub.c_str(),12,1).x,w=std::max(tw,sw);
                    DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),sub.empty() ? 22 : 34,Fade(BLACK,.6F));
                    DrawTextEx(font,title.c_str(),{at.x-tw/2,at.y-16},15,1,tint);
                    if(!sub.empty()) DrawTextEx(font,sub.c_str(),{at.x-sw/2,at.y+1},12,1,Color{226,226,220,255});
                };
                if(callouts.active())
                    if(const int v=vehicles.find(callouts.car());v>=0) {
                        const auto& car=vehicles.vehicles()[static_cast<std::size_t>(v)];
                        tag({car.position.x,car.position.y+2.4F,car.position.z},"SUSPECT VEHICLE  ·  "+callouts.plate(),callouts.look(),Color{255,90,80,255},140);
                    }
                for(const auto& h:mafio.heists())
                    for(const int id:h.squad) {
                        const auto* bot=br_bots.find(id);
                        if(!bot || !bot->alive() || br_bots.mounted(id) || callouts.escorting(id)) continue;
                        const Vector3 over{bot->position().x,bot->position().y+2.25F,bot->position().z};
                        if(std::find(h.surrendered.begin(),h.surrendered.end(),id)!=h.surrendered.end())
                            tag(over,"HANDS UP",Vector3Distance(bot->position(),{player.position.x,player.position.y-1,player.position.z})<game::career::Callouts::cuff_reach ? "USE to cuff him" : "walk up to cuff him",Color{255,214,120,255},40);
                        else if(career.on_duty()) tag(over,std::find(h.running.begin(),h.running.end(),id)!=h.running.end() ? "SUSPECT - RUNNING" : "SUSPECT","",Color{255,90,80,255},80);
                    }
                for(const auto& e:callouts.escorts())
                    if(const auto* bot=br_bots.find(e.bot)) tag({bot->position().x,bot->position().y+2.25F,bot->position().z},"CUFFED","",Color{150,190,255,255},30);
                if(prison_until>=0 && !in_shu && career.state().side==game::career::Side::Gang)
                    if(const auto* mark=prison_yard.snitch()>=0 ? prison_yard.find(prison_yard.snitch()) : nullptr) {
                        const Vector3 w=prison_yard.world(*mark);
                        tag({w.x,w.y+2.25F,w.z},"SNITCH","he talked to the Policio",Color{255,90,80,255},45);
                    }
            }
            // Every other crew's boss, by his door: who to talk to about joining.
            if(game_mode==game::GameMode::Explore && !career_path.empty())
                for(const auto& crew:mafio.crews()) {
                    if(crew.disbanded || crew.members.empty() || (!campaign_path.empty() && &crew==mafio.casino() && crew.gang==0)) continue;
                    const auto* boss=br_bots.find(crew.members.front());
                    if(!boss || !boss->alive() || br_bots.hidden(crew.members.front()) || Vector3Distance(boss->position(),camera.position)>20) continue;
                    const Vector3 over{boss->position().x,boss->position().y+2.3F,boss->position().z};
                    if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) continue;
                    const Vector2 at=GetWorldToScreen(over,camera);
                    const auto& gang=game::mafia::gangs[static_cast<std::size_t>(crew.gang)];
                    const std::string title="BOSS  ·  "+std::string(gang.name);
                    const bool close=std::hypot(boss->position().x-player.position.x,boss->position().z-player.position.z)<3.2F;
                    const char* sub=career.in_gang(crew.gang) ? "Your boss" : career.on_duty() ? "Won't talk to a cop" : close ? "USE to talk - join the gang" : "Talk to him to join";
                    const float tw=MeasureTextEx(font,title.c_str(),15,1).x,sw=MeasureTextEx(font,sub,12,1).x,w=std::max(tw,sw);
                    DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),34,Fade(BLACK,.55F));
                    DrawTextEx(font,title.c_str(),{at.x-tw/2,at.y-16},15,1,gang.color);
                    DrawTextEx(font,sub,{at.x-sw/2,at.y+1},12,1,Color{220,226,206,255});
                }
            // The boss of Verda Transporto: his name over his head.
            if(depot_boss.placed && Vector3Distance(depot_boss.at,camera.position)<14) {
                const Vector3 over{depot_boss.at.x,depot_boss.at.y+2.3F,depot_boss.at.z};
                if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))>0) {
                    const Vector2 at=GetWorldToScreen(over,camera);
                    const std::string title=std::string(depot_boss_name)+"  ·  Verda Transporto";
                    const char* sub=Vector3Distance(depot_boss.at,{player.position.x,player.position.y-1,player.position.z})<3.2F ? "USE to talk" : "The boss - loads for the ship, the towns and the ranches";
                    const float tw=MeasureTextEx(font,title.c_str(),16,1).x,sw=MeasureTextEx(font,sub,13,1).x,w=std::max(tw,sw);
                    DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),36,Fade(BLACK,.55F));
                    DrawTextEx(font,title.c_str(),{at.x-tw/2,at.y-16},16,1,Color{90,220,140,255});
                    DrawTextEx(font,sub,{at.x-sw/2,at.y+2},13,1,Color{220,226,206,255});
                }
            }
            // The load in hand (top right, under the buttons), and what is happening with it.
            if(trucking.hired() && trucking.job().stage!=game::freight::HaulStage::None && !map_screen.is_open()) {
                const std::string title=trucking.title()+"  ·  Verda Transporto";
                // One line: where to now (the full brief is in the bag's QUESTS).
                const auto stage=trucking.job().stage;
                std::string sub=stage==game::freight::HaulStage::Pickup ? "Pick up at "+trucking.job().from_name :
                                stage==game::freight::HaulStage::Deliver ? "Deliver to "+trucking.job().to_name : std::string("Back to the boss for your pay");
                // Under the buttons, clear of the compass: what to do now, wrapped to fit.
                const float max_w=std::min(420.0F,screen_width*.4F);
                std::vector<std::string> lines;std::string line,word;
                for(std::size_t i=0;i<=sub.size();++i) {
                    const char c=i<sub.size() ? sub[i] : ' ';
                    if(c!=' ') {word+=c;continue;}
                    const std::string trial=line.empty() ? word : line+" "+word;
                    if(MeasureTextEx(font,trial.c_str(),14,1).x>max_w-24 && !line.empty()) {lines.push_back(line);line=word;}
                    else line=trial;
                    word.clear();
                }
                if(!line.empty()) lines.push_back(line);
                float w=MeasureTextEx(font,title.c_str(),17,1).x;
                for(const auto& l:lines) w=std::max(w,MeasureTextEx(font,l.c_str(),14,1).x);
                w=std::min(w+24,max_w);
                const float h=26+17*static_cast<float>(lines.size()),x=screen_width-w-10,y=88;
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),static_cast<int>(w),static_cast<int>(h),Fade(BLACK,.5F));
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),4,static_cast<int>(h),Color{90,220,140,255});
                DrawTextEx(font,title.c_str(),{x+12,y+4},17,1,Color{90,220,140,255});
                for(std::size_t k=0;k<lines.size();++k) DrawTextEx(font,lines[k].c_str(),{x+12,y+24+17*static_cast<float>(k)},14,1,Color{220,226,206,255});
            }
            if(trucking.message_time()>0) {
                const float alpha=std::clamp(trucking.message_time(),0.0F,1.0F);
                const float mw=MeasureTextEx(font,trucking.message().c_str(),18,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-mw/2-12),screen_height-389,static_cast<int>(mw+24),28,Fade(Color{10,40,24,255},.65F*alpha));
                DrawTextEx(font,trucking.message().c_str(),{screen_width/2-mw/2,static_cast<float>(screen_height-384)},18,1,Fade(Color{170,240,190,255},alpha));
            }
            // The street race: lining up, the count, the place and the flares.
            if(street_race.ready()) {
                std::string status;Color tone{255,150,70,255};float size=22;
                switch(street_race.stage()) {
                    case game::racing::Stage::Lining: status="STREET RACE - line up at the flare and stop";break;
                    case game::racing::Stage::Countdown: {
                        const int n=static_cast<int>(std::ceil(street_race.countdown()));
                        status=n>0 ? std::to_string(n) : "GO!";size=64;tone=n>0 ? Color{255,210,90,255} : Color{120,255,140,255};
                        break;
                    }
                    case game::racing::Stage::Racing:
                        status=TextFormat("P%d / %d   ·   FLARE %d / %d",street_race.position(),street_race.field(),
                                          std::min(street_race.next_checkpoint()+1,static_cast<int>(street_race.checkpoints().size())),static_cast<int>(street_race.checkpoints().size()));
                        break;
                    default: break;
                }
                if(!status.empty()) {
                    const float w=MeasureTextEx(font,status.c_str(),size,1).x;
                    DrawRectangle(static_cast<int>(screen_width/2-w/2-14),118,static_cast<int>(w+28),static_cast<int>(size+14),Fade(BLACK,.5F));
                    DrawTextEx(font,status.c_str(),{screen_width/2-w/2,125},size,1,tone);
                }
            }
            if(chief_line_time>0 && !chief_line.empty()) {
                // A dialogue box, wrapped to two or three lines.
                const float alpha=std::clamp(chief_line_time,0.0F,1.0F);
                const float max_w=std::min(760.0F,screen_width-60.0F);
                std::vector<std::string> lines;std::string line,word;
                for(std::size_t i=0;i<=chief_line.size();++i) {
                    const char c=i<chief_line.size() ? chief_line[i] : ' ';
                    if(c!=' ') {word+=c;continue;}
                    const std::string trial=line.empty() ? word : line+" "+word;
                    if(MeasureTextEx(font,trial.c_str(),18,1).x>max_w && !line.empty()) {lines.push_back(line);line=word;}
                    else line=trial;
                    word.clear();
                }
                if(!line.empty()) lines.push_back(line);
                const float h=static_cast<float>(lines.size())*24+20,y=screen_height-300-h;
                DrawRectangle(static_cast<int>(screen_width/2-max_w/2-16),static_cast<int>(y),static_cast<int>(max_w+32),static_cast<int>(h),Fade(Color{10,16,36,255},.82F*alpha));
                DrawRectangle(static_cast<int>(screen_width/2-max_w/2-16),static_cast<int>(y),4,static_cast<int>(h),Fade(Color{212,170,60,255},alpha));
                for(std::size_t i=0;i<lines.size();++i)
                    DrawTextEx(font,lines[i].c_str(),{screen_width/2-max_w/2,y+10+static_cast<float>(i)*24},18,1,Fade(i==0 ? Color{235,215,160,255} : RAYWHITE,alpha));
            }
            // The training camp: its name and the drills, the drill in progress, the result.
            if(police_camp.placed() && Vector3Distance(police_camp.line(),camera.position)<75) {
                const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
                const float x=screen_width-350.0F,y=screen_height*.235F;
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),340,118,Fade(BLACK,.6F));
                DrawRectangle(static_cast<int>(x),static_cast<int>(y),4,118,Color{90,150,255,255});
                DrawTextEx(font,"POLICIO TRAINING CAMP",{x+14,y+6},18,1,Color{150,190,255,255});
                for(int d=0;d<static_cast<int>(game::career::drills.size());++d) {
                    const bool ok=police_camp.qualified(d),now=police_camp.phase()!=game::career::CampPhase::Idle && police_camp.drill()==d;
                    const char* row=TextFormat("%s %s   best %d/%d, need %d",ok ? "[OK]" : "[  ]",game::career::drills[static_cast<std::size_t>(d)].name,
                        police_camp.best(d),game::career::drills[static_cast<std::size_t>(d)].count,game::career::drills[static_cast<std::size_t>(d)].pass);
                    DrawTextEx(font,row,{x+14,y+32+static_cast<float>(d)*20},15,1,ok ? Color{140,230,140,255} : now ? Color{255,226,140,255} : Color{205,212,196,255});
                }
                const char* hint=police_camp.graduated() ? "Qualified on every drill" :
                    police_camp.at_line(feet) ? TextFormat("USE at the line: %s",game::career::drills[static_cast<std::size_t>(police_camp.next_drill())].name) :
                    "Stand at the firing line (the lectern) to begin";
                DrawTextEx(font,hint,{x+14,y+96},14,1,Color{220,226,206,255});
                if(police_camp.phase()==game::career::CampPhase::Countdown) {
                    const char* count=TextFormat("%d",static_cast<int>(std::ceil(police_camp.clock())));
                    const float cw=MeasureTextEx(font,count,72,1).x;
                    DrawTextEx(font,count,{screen_width/2-cw/2,screen_height*.3F},72,1,Color{255,226,140,255});
                } else if(police_camp.phase()==game::career::CampPhase::Running) {
                    const auto& d=game::career::drills[static_cast<std::size_t>(police_camp.drill())];
                    const char* line=TextFormat("%s   TARGET %d/%d   HITS %d (need %d)   SHOTS %d",d.name,police_camp.shown(),d.count,police_camp.hits(),d.pass,police_camp.shots());
                    const float lw=MeasureTextEx(font,line,20,1).x;
                    DrawRectangle(static_cast<int>(screen_width/2-lw/2-14),static_cast<int>(screen_height*.18F),static_cast<int>(lw+28),32,Fade(BLACK,.6F));
                    DrawTextEx(font,line,{screen_width/2-lw/2,screen_height*.18F+6},20,1,RAYWHITE);
                }
                if(police_camp.message_time()>0) {
                    const float alpha=std::clamp(police_camp.message_time(),0.0F,1.0F);
                    const float mw=MeasureTextEx(font,police_camp.message().c_str(),19,1).x;
                    DrawRectangle(static_cast<int>(screen_width/2-mw/2-14),screen_height-166,static_cast<int>(mw+28),30,Fade(BLACK,.6F*alpha));
                    DrawTextEx(font,police_camp.message().c_str(),{screen_width/2-mw/2,static_cast<float>(screen_height-161)},19,1,Fade(Color{150,200,255,255},alpha));
                }
            }
            // At a police station: how to sign up (or why not).
            if(!career_path.empty() && !career.on_duty() && services.active() &&
               services.station_near({player.position.x,player.position.y-1,player.position.z},14) && !vehicles.driver()) {
                const char* line=career.state().chief_dead ? "POLICIO DE VERDA  ·  leaderless since the Chief was hit - no recruits" :
                                 career.state().record ? "POLICIO DE VERDA  ·  no criminals on the force - you have a record" :
                                 career.state().side==game::career::Side::Gang ? "POLICIO DE VERDA  ·  leave your gang before you sign up" :
                                                         "POLICIO DE VERDA  ·  USE to join: a patrol car and radio calls";
                const float lw=MeasureTextEx(font,line,17,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-lw/2-12),screen_height-246,static_cast<int>(lw+24),28,Fade(BLACK,.55F));
                DrawTextEx(font,line,{screen_width/2-lw/2,static_cast<float>(screen_height-241)},17,1,career.state().record ? Color{255,140,120,255} : Color{150,190,255,255});
            }
            // The turf you stand on: its holder and their hold (gang members); the underworld's news.
            if(player_gang()>=0) {
                const int here=territory.turf_at({player.position.x,player.position.y-1,player.position.z});
                if(here!=shown_turf) {shown_turf=here;turf_banner=4;}
                turf_banner=std::max(0.0F,turf_banner-GetFrameTime());
                if(here>=0 && turf_banner>0) {
                    const auto& t=territory.turfs()[static_cast<std::size_t>(here)];
                    const bool held=territory.controls(here,player_gang());
                    const int holder=t.ranch>=0 ? (territory.families()[static_cast<std::size_t>(t.ranch)].gang>=0 &&
                        territory.families()[static_cast<std::size_t>(t.ranch)].stance!=game::mafia::Stance::Independent ?
                        territory.families()[static_cast<std::size_t>(t.ranch)].gang : t.owner) : t.owner;
                    const std::string line=t.cleared ? t.name+"  ·  cleared by the Policio - nobody's turf" :
                        t.ranch>=0 ? t.name+"  ·  "+faction_name(t.owner)+(held ? "  ·  WITH YOU" : "") :
                        t.name+"  ·  "+gang_name(t.owner)+(held ? "  ·  YOURS" : "")+TextFormat("  ·  HOLD %d",static_cast<int>(t.hold));
                    const float lw=MeasureTextEx(font,line.c_str(),15,1).x;
                    const float x=screen_width/2.0F-(lw+30)/2,y=78;
                    DrawRectangle(static_cast<int>(x),static_cast<int>(y),static_cast<int>(lw+30),24,Fade(BLACK,.55F));
                    DrawRectangle(static_cast<int>(x),static_cast<int>(y),5,24,faction_color(holder));
                    if(t.ranch<0) DrawRectangle(static_cast<int>(x+5),static_cast<int>(y+21),static_cast<int>((lw+25)*t.hold/100),3,faction_color(holder));
                    DrawTextEx(font,line.c_str(),{x+16,y+4},15,1,held ? Color{170,240,170,255} : faction_color(holder));
                }
            }
            if(gang_news_time>0 && !gang_news.empty()) {
                const float alpha=std::clamp(gang_news_time,0.0F,1.0F);
                const float mw=MeasureTextEx(font,gang_news.c_str(),19,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-mw/2-14),screen_height-384,static_cast<int>(mw+28),30,Fade(Color{30,10,10,255},.7F*alpha));
                DrawTextEx(font,gang_news.c_str(),{screen_width/2-mw/2,static_cast<float>(screen_height-379)},19,1,Fade(Color{255,190,140,255},alpha));
            }
            if(society.open && !(island_ruled && player_gang()>=0)) {
                const char* crown="VERDA IS SECURE";
                const float cw=MeasureTextEx(font,crown,18,1).x;
                DrawTextEx(font,crown,{screen_width/2-cw/2,124},18,1,Color{150,200,255,255});
            } else if((island_ruled && player_gang()>=0) || endgame.phase()==game::mafia::EndPhase::Occupied || endgame.phase()==game::mafia::EndPhase::Free) {
                using game::mafia::EndPhase;
                const auto phase=endgame.phase();
                const std::string gang=player_gang()>=0 ? gang_name(player_gang()) : std::string();
                const std::string crown=phase==EndPhase::Siege ? gang+" RULE VERDA  ·  TAKE THE REGISTARO" :
                    phase==EndPhase::Occupied ? TextFormat("RUSSIAN TROOPS HOLD %d OF %d TOWNS",endgame.garrisons_left(),static_cast<int>(endgame.garrisons().size())) :
                    phase==EndPhase::Free ? (gang.empty() ? std::string("VERDA IS FREE") : gang+" RULE A FREE VERDA") : gang+" RULE VERDA";
                const float cw=MeasureTextEx(font,crown.c_str(),18,1).x;
                DrawTextEx(font,crown.c_str(),{screen_width/2-cw/2,124},18,1,phase==EndPhase::Occupied ? Color{240,110,90,255} : Color{240,200,90,255});
            }
            // (The career - side, rank, respect, the job in hand - is in the bag's QUESTS.)
            // Doing time, or prisoners in the back of the car.
            if(game_mode==game::GameMode::Explore && (prison_until>=0 || career.custody()>0)) {
                const int left=static_cast<int>(std::ceil(std::max(0.0,prison_until-match_time)));
                const std::string line=prison_until>=0 ? TextFormat("ŜTATA MALLIBEREJO  ·  inmate No. %05d%s  ·  %d:%02d to go",career.inmate_number(),in_shu ? "  ·  SHU" : "",left/60,left%60) :
                    TextFormat("IN CUSTODY: %d  ·  drive them to the state prison in a police car",career.custody());
                const float lw=MeasureTextEx(font,line.c_str(),18,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-lw/2-12),screen_height-292,static_cast<int>(lw+24),28,Fade(BLACK,.6F));
                DrawTextEx(font,line.c_str(),{screen_width/2-lw/2,static_cast<float>(screen_height-287)},18,1,prison_until>=0 ? Color{230,180,90,255} : Color{150,190,255,255});
            }
            // A line of talk in a box (the partner, a guard, the warden): who, what, and how to go on.
            const auto speech_box=[&](const std::string& speaker,const std::string& text,Color accent,const std::string& footer) {
                const float max_w=std::min(820.0F,screen_width-60.0F);
                std::vector<std::string> rows;std::string row,word;
                for(std::size_t i=0;i<=text.size();++i) {
                    const char c=i<text.size() ? text[i] : ' ';
                    if(c!=' ') {word+=c;continue;}
                    const std::string trial=row.empty() ? word : row+" "+word;
                    if(MeasureTextEx(font,trial.c_str(),19,1).x>max_w-36 && !row.empty()) {rows.push_back(row);row=word;}
                    else row=trial;
                    word.clear();
                }
                if(!row.empty()) rows.push_back(row);
                const float h=58+22*static_cast<float>(rows.size()),x=screen_width/2-max_w/2,y=screen_height*.70F-h;
                DrawRectangleRounded({x,y,max_w,h},.08F,8,Fade(Color{10,14,20,255},.84F));
                DrawRectangle(static_cast<int>(x),static_cast<int>(y+8),4,static_cast<int>(h-16),accent);
                DrawTextEx(font,speaker.c_str(),{x+18,y+10},18,1,accent);
                for(std::size_t k=0;k<rows.size();++k) DrawTextEx(font,rows[k].c_str(),{x+18,y+34+22*static_cast<float>(k)},19,1,Color{236,240,226,255});
                if(!footer.empty()) {
                    const float fw=MeasureTextEx(font,footer.c_str(),14,1).x;
                    DrawTextEx(font,footer.c_str(),{x+max_w-fw-16,y+h-22},14,1,Color{170,180,190,255});
                }
            };
            if(game_mode==game::GameMode::Explore) {
                // The partner talks the officer through the first call.
                if(const auto* line=callouts.line();line && booking.stage<0 && prison_tour.line<0)
                    speech_box(line->speaker,line->text,Color{120,170,255,255},"");
                // The warden's tour.
                if(prison_tour.line>=0)
                    speech_box("Warden Brana  ·  Ŝtata Malliberejo",tour_lines[static_cast<std::size_t>(prison_tour.line)],Color{230,180,90,255},
                               TextFormat("USE - next  (%d/%d)",prison_tour.line+1,static_cast<int>(tour_lines.size())));
                // A guard's word inside, and the gang's.
                if(inside_line<inside_lines.size() && booking.stage<0)
                    speech_box(inside_lines[inside_line].speaker,inside_lines[inside_line].text,Color{230,180,90,255},
                               TextFormat("USE - continue  (%d/%d)",static_cast<int>(inside_line)+1,static_cast<int>(inside_lines.size())));
                // Inside: the bunk, and the snitch.
                if(prison_until>=0 && booking.stage<0 && inside_line>=inside_lines.size() && sleep_fade<0) {
                    const Vector3 me{player.position.x,player.position.y-1,player.position.z};
                    const char* prompt=nullptr;
                    if(Vector3Distance(me,prison_bunk(in_shu))<2.2F) prompt="E - Lie down and sleep: do your time";
                    else if(const auto* mark=prison_yard.snitch()>=0 ? prison_yard.find(prison_yard.snitch()) : nullptr;
                            mark && career.state().side==game::career::Side::Gang && !in_shu && Vector3Distance(me,prison_yard.world(*mark))<2.6F)
                        prompt="E - Shank the snitch";
                    if(prompt) {
                        const float pw=MeasureTextEx(font,prompt,20,1).x;
                        DrawRectangle(static_cast<int>(screen_width/2-pw/2-12),screen_height-250,static_cast<int>(pw+24),32,Fade(BLACK,.6F));
                        DrawTextEx(font,prompt,{screen_width/2-pw/2,static_cast<float>(screen_height-245)},20,1,Color{255,214,120,255});
                    }
                }
                // Busted: the ride in (black), the booking photo; sleeping; dragged to the SHU.
                const auto black=[&](float alpha,const char* caption) {
                    DrawRectangle(0,0,screen_width,screen_height,Fade(BLACK,std::clamp(alpha,0.0F,1.0F)));
                    if(caption && alpha>.6F) {
                        const float cw=MeasureTextEx(font,caption,24,1).x;
                        DrawTextEx(font,caption,{screen_width/2-cw/2,screen_height/2.0F-12},24,1,Fade(Color{230,226,214,255},std::clamp((alpha-.6F)*2.5F,0.0F,1.0F)));
                    }
                };
                if(booking.stage==0) black(booking.t/1.4F,"BUSTED");
                if(booking.stage==1) black(1,"In the back of a Policio car - to Ŝtata Malliberejo");
                if(booking.stage==4) black(booking.t/1.0F,"The guards drag you off to the SHU");
                if(sleep_fade>=0) black(sleep_fade/1.6F,"You sleep off the rest of your sentence...");
                if(booking.stage==2) {
                    // The booking photo: the player's face against the height chart, a board with the number.
                    if(booking.t<=dt+1e-4F || mugshot.target.id==0) {
                        if(mugshot.target.id==0) mugshot.target=LoadRenderTexture(512,512);
                        const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
                        const Vector3 ahead{std::sin(player.yaw),0,std::cos(player.yaw)};   // the way the body faces
                        Camera3D shot{};
                        shot.target={feet.x,feet.y+1.58F,feet.z};
                        shot.position=Vector3Add(shot.target,Vector3Scale(ahead,1.05F));
                        shot.up={0,1,0};shot.fovy=32;shot.projection=CAMERA_PERSPECTIVE;
                        BeginTextureMode(mugshot.target);
                        ClearBackground(Color{196,198,192,255});
                        for(int k=0;k<12;++k) {
                            const int y=40+k*38;
                            DrawRectangle(0,y,512,2,Color{120,124,126,255});
                            DrawText(TextFormat("%d",200-k*10),10,y-18,16,Color{80,84,88,255});
                        }
                        BeginMode3D(shot);
                        const auto* look=game::wardrobe::wearing(loot_session.economy(),wardrobe_catalogue);
                        character_renderer.set_player(look ? look->character : std::string());
                        characters::BodyPose pose;
                        pose.outfit=look ? outfit_texture(look->outfit) : 0U;
                        character_renderer.draw_player(feet,player.yaw,pose);
                        EndMode3D();
                        DrawRectangle(96,400,320,86,Color{22,24,26,255});
                        DrawRectangleLines(96,400,320,86,Color{200,200,196,255});
                        const char* board="ŜTATA MALLIBEREJO";
                        DrawTextEx(font,board,{256-MeasureTextEx(font,board,20,1).x/2,410},20,1,WHITE);
                        const char* number=TextFormat("No. %05d",career.inmate_number());
                        DrawTextEx(font,number,{256-MeasureTextEx(font,number,30,1).x/2,440},30,1,Color{255,214,120,255});
                        EndTextureMode();
                    }
                    DrawRectangle(0,0,screen_width,screen_height,Fade(BLACK,.82F));
                    const float s=std::min(screen_width/1000.0F,screen_height/560.0F);
                    const float cw=920*s,ch=470*s,cx=screen_width/2-cw/2,cy=screen_height/2-ch/2;
                    DrawRectangleRounded({cx,cy,cw,ch},.03F,8,Color{232,228,214,255});
                    DrawRectangle(static_cast<int>(cx),static_cast<int>(cy),static_cast<int>(cw),static_cast<int>(54*s),Color{40,46,60,255});
                    DrawTextEx(font,"POLICIO DE VERDA  ·  BOOKING",{cx+20*s,cy+14*s},26*s,1,WHITE);
                    const Rectangle photo{cx+24*s,cy+74*s,370*s,370*s};
                    DrawTexturePro(mugshot.target.texture,{0,0,512,-512},photo,{0,0},0,WHITE);
                    DrawRectangleLinesEx(photo,2,Color{40,46,60,255});
                    const float tx=cx+420*s;float ty=cy+84*s;
                    const auto row=[&](const char* label,const std::string& value,Color tint) {
                        DrawTextEx(font,label,{tx,ty},17*s,1,Color{90,96,104,255});
                        DrawTextEx(font,value.c_str(),{tx,ty+20*s},25*s,1,tint);
                        ty+=58*s;
                    };
                    const Color ink{30,32,36,255};
                    row("INMATE No.",TextFormat("%05d",career.inmate_number()),Color{160,40,30,255});
                    const auto& st=career.state();
                    row("AFFILIATION",st.side==game::career::Side::Gang ? std::string(game::mafia::gangs[static_cast<std::size_t>(std::max(0,st.gang))].name) :
                        st.side==game::career::Side::Trucker ? std::string("La Frateco de Kamionistoj") : std::string("none known"),ink);
                    row("CHARGE",justice.reports()>0 ? std::string("Wanted - taken in by the Policio") : std::string("Resisting arrest"),ink);
                    const int sentence=static_cast<int>(booking.sentence);
                    row("SENTENCE",TextFormat("%d min %02d s  ·  Ŝtata Malliberejo",sentence/60,sentence%60),ink);
                    row("NOTORIETY",TextFormat("%d / 100",static_cast<int>(st.notoriety)),ink);
                    if(booking.t>1.2F) {
                        const char* go="USE - to your cell";
                        DrawTextEx(font,go,{cx+cw-MeasureTextEx(font,go,18*s,1).x-20*s,cy+ch-34*s},18*s,1,Color{40,46,60,255});
                    }
                }
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
        // The outbreak: names over the family and the holdouts' leaders, the line being said,
        // and what to do next.
        if(outbreak.active()) {
            const Font& font=sign_renderer.font();
            const Vector3 feet{player.position.x,player.position.y-1,player.position.z};
            for(const auto& p:outbreak.people()) {
                if(p.hidden || p.dead || (!p.leader && p.name!=game::outbreak::Story::ron && p.name!=game::outbreak::Story::dario)) continue;
                if(Vector3Distance(p.at,camera.position)>18) continue;
                const Vector3 over{p.at.x,p.at.y+2.3F,p.at.z};
                if(Vector3DotProduct(Vector3Subtract(over,camera.position),Vector3Subtract(camera.target,camera.position))<=0) continue;
                const Vector2 at=GetWorldToScreen(over,camera);
                const std::string title=p.holdout>=0 ? p.name+"  ·  "+std::string(game::outbreak::holdouts[static_cast<std::size_t>(outbreak.holdouts()[static_cast<std::size_t>(p.holdout)].spec)].faction) : p.name;
                const bool talkable=p.leader || outbreak.has_hideout();
                const char* sub=talkable && Vector3Distance(p.at,feet)<game::outbreak::Run::talk_range ? "USE to talk" : p.holdout>=0 ? "Holding out" : "Family";
                const float tw=MeasureTextEx(font,title.c_str(),16,1).x,sw=MeasureTextEx(font,sub,13,1).x,w=std::max(tw,sw);
                DrawRectangle(static_cast<int>(at.x-w/2-6),static_cast<int>(at.y-18),static_cast<int>(w+12),36,Fade(BLACK,.55F));
                DrawTextEx(font,title.c_str(),{at.x-tw/2,at.y-16},16,1,Color{255,214,120,255});
                DrawTextEx(font,sub,{at.x-sw/2,at.y+2},13,1,Color{220,226,206,255});
            }
            if(const auto* line=outbreak.line()) {
                const std::string said=line->text;
                const float max_w=std::min(820.0F,screen_width-60.0F);
                std::vector<std::string> rows;std::string row,word;
                for(std::size_t i=0;i<=said.size();++i) {
                    const char c=i<said.size() ? said[i] : ' ';
                    if(c!=' ') {word+=c;continue;}
                    const std::string trial=row.empty() ? word : row+" "+word;
                    if(MeasureTextEx(font,trial.c_str(),19,1).x>max_w-36 && !row.empty()) {rows.push_back(row);row=word;}
                    else row=trial;
                    word.clear();
                }
                if(!row.empty()) rows.push_back(row);
                const float h=58+22*static_cast<float>(rows.size()),x=screen_width/2-max_w/2,y=screen_height*.70F-h;
                const Color edge=std::string(line->speaker)==game::outbreak::Story::rancher ? Color{230,90,70,255} : Color{255,214,120,255};
                DrawRectangleRounded({x,y,max_w,h},.08F,8,Fade(Color{12,16,12,255},.82F));
                DrawRectangle(static_cast<int>(x),static_cast<int>(y+8),4,static_cast<int>(h-16),edge);
                DrawTextEx(font,line->speaker,{x+18,y+10},18,1,edge);
                for(std::size_t k=0;k<rows.size();++k) DrawTextEx(font,rows[k].c_str(),{x+18,y+34+22*static_cast<float>(k)},19,1,Color{236,240,226,255});
                const char* more=outbreak.line_is_story() ? "USE - continue" : "USE - close";
                const float mw=MeasureTextEx(font,more,14,1).x;
                DrawTextEx(font,more,{x+max_w-mw-16,y+h-22},14,1,Color{170,190,150,255});
            }
            if(const auto goal=outbreak.story().objective(outbreak.infected_left());!goal.empty() && !map_screen.is_open()) {
                const std::string text="> "+goal;
                const float gw=MeasureTextEx(font,text.c_str(),19,1).x;
                DrawRectangle(static_cast<int>(screen_width/2-gw/2-14),132,static_cast<int>(gw+28),32,Fade(BLACK,.55F));
                DrawTextEx(font,text.c_str(),{screen_width/2-gw/2,138},19,1,Color{255,214,120,255});
            }
        }
        if(bus_ride>=0 && bus_ride<static_cast<int>(bus_line.buses().size()) && !map_screen.is_open()) {
            const auto& bus=bus_line.buses()[static_cast<std::size_t>(bus_ride)];
            const std::string line=std::string(bus.dwell>0 ? "LINIO 1  ·  at " : "LINIO 1  ·  next stop: ")+
                bus_line.stops()[static_cast<std::size_t>(bus.next_stop)].town+TextFormat("  ·  %d aboard  ·  ",bus_line.aboard(bus_ride))+
                (bus.dwell>0 ? "USE to get off here" : "USE at a stop to get off");
            const Font& font=sign_renderer.font();
            const float lw=MeasureTextEx(font,line.c_str(),16,1).x;
            DrawRectangle(static_cast<int>(screen_width/2-lw/2-12),screen_height-160,static_cast<int>(lw+24),26,Fade(BLACK,.55F));
            DrawTextEx(font,line.c_str(),{screen_width/2-lw/2,static_cast<float>(screen_height-155)},16,1,Color{150,220,255,255});
        }
        if (interaction_remaining > 0) {
            // Word-wrapped to most of the screen's width, the last line where a single one sits.
            std::vector<std::string> lines;
            std::string current;
            const int room=static_cast<int>(screen_width*.82F);
            std::size_t at=0;
            while(at<interaction_message.size()) {
                std::size_t end=interaction_message.find(' ',at);
                if(end==std::string::npos) end=interaction_message.size();
                const std::string word=interaction_message.substr(at,end-at);
                const std::string tried=current.empty() ? word : current+" "+word;
                if(!current.empty() && MeasureTextEx(sign_renderer.font(),tried.c_str(),18,1).x>room) {lines.push_back(current);current=word;}
                else current=tried;
                at=end+1;
            }
            if(!current.empty()) lines.push_back(current);
            const int top=screen_height-205-static_cast<int>(lines.size()-1)*22;
            for(std::size_t i=0;i<lines.size();++i)
                DrawTextEx(sign_renderer.font(),lines[i].c_str(),{screen_width/2-MeasureTextEx(sign_renderer.font(),lines[i].c_str(),18,1).x/2,static_cast<float>(top+static_cast<int>(i)*22)},18,1,RAYWHITE);
        }

        // MENU (pause, settings, controls, save, quit); N still mutes.
        DrawRectangleRec(audio_button, Fade(BLACK, 0.60F));
        DrawText(touch_ui ? "MENU" : "MENU · ESC", screen_width - 165, 53, 14, RAYWHITE);
        if(environment_audio.ready() && environment_audio.muted()) DrawText("MUTED", screen_width - 62, 53, 14, Color{230,120,100,255});
        if(game_mode!=game::GameMode::DevLab) {
            const auto map_button=game::MapScreen::map_button(screen_width,screen_height);
            DrawRectangleRec(map_button,Fade(BLACK,.60F));
            DrawText("MAP · M",static_cast<int>(map_button.x)+14,53,14,RAYWHITE);
            if(game::rules_for(game_mode).civilian_life && !map_screen.is_open() && !inventory_open && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()))
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
            // The car radio's "now playing", small and out of the way: tucked left of the gauge for a
            // few seconds after getting in, tuning or a new song. (Titles are Esperanto: the sign font.)
            if(const float shown=car_radio.note(); shown>0 && !map_screen.is_open() && !inventory_open) {
                const Font& font=sign_renderer.font();
                const std::string station=car_radio.note_station(),song=car_radio.note_title();
                const float right=screen_width*.42F-82,top=screen_height-110.0F;
                const auto line=[&](const std::string& text,float y,float size,Color ink) {
                    const float w=MeasureTextEx(font,text.c_str(),size,1).x;
                    DrawTextEx(font,text.c_str(),{right-w+1,y+1},size,1,Fade(BLACK,.6F*shown));
                    DrawTextEx(font,text.c_str(),{right-w,y},size,1,Fade(ink,shown));
                };
                line(station,top,13,Color{170,215,180,255});
                line(song,top+16,15,Color{235,235,235,255});
            }
            drive_hint=std::max(0.0F,drive_hint-GetFrameTime());
            if(!touch_ui && drive_hint>0)
                DrawText("W/S drive | A/D steer | SPACE handbrake | SHIFT boost | C park | H horn | V cab view | E exit",
                         static_cast<int>(screen_width*.42F)+84,screen_height-26,15,YELLOW);
        }
        else if(!creator_active && !inventory_open && store_prompt.empty() && vehicles.nearest(verda_region,{player.position.x,player.position.y-1,player.position.z})>=0)DrawText("USE / E - Enter vehicle",screen_width/2-120,screen_height-100,20,YELLOW);
        if(game_mode!=game::GameMode::DevLab && !mode_session.terminal() && !(shop_open||store_open||motor_menu.is_open()||gear_menu.is_open()) && !poker_open && !inventory_open) {
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
                DrawText(TextFormat("%lld kr",static_cast<long long>(loot_session.economy().money)),20,objective_bottom+6,16,GOLD);
                if(nearby_shop && !at_table && !poker_open) DrawText(TextFormat("E / USE: %s",nearby_shop->name.c_str()),screen_width/2-160,screen_height-165,20,GOLD);
                if(home_here) {
                    const bool owned=game::economy::owns(loot_session.economy(),*home_here);
                    const std::string line=!owned ? home_here->name+" - FOR SALE "+std::to_string(home_here->price)+" kr   USE: BUY" :
                        "Home: "+home_here->name+" - USE by the bed: SLEEP";
                    const Font& font=sign_renderer.font();
                    const float tw=MeasureTextEx(font,line.c_str(),20,1).x;
                    DrawRectangle(static_cast<int>(screen_width/2-tw/2-14),screen_height-252,static_cast<int>(tw+28),32,Fade(BLACK,.6F));
                    DrawTextEx(font,line.c_str(),{screen_width/2-tw/2,static_cast<float>(screen_height-246)},20,1,owned ? Color{140,220,150,255} : GOLD);
                }
                // La Verda Basballigo's scoreboard at a park with a game on (sitting in the stands: the table too).
                if(game::rules_for(game_mode).civilian_life && !league.fixtures().empty()) {
                    using namespace game::sports;
                    const double now=island_life.clock().minutes;
                    for(int p=0;p<5;++p) {
                        const auto& site=parks[static_cast<std::size_t>(p)];
                        if(site.team<0 || std::hypot(loot_feet.x-site.origin.x,loot_feet.z-site.origin.z)>170) continue;
                        const auto live=league.live(p,now);
                        const bool riot=league.rioting(p,now);
                        if(live.phase==Phase::Quiet && !riot) continue;
                        const int fixture=live.fixture>=0 ? live.fixture : static_cast<int>(league.fixtures().size())-1;
                        const auto& g=league.fixtures()[static_cast<std::size_t>(fixture)];
                        const auto& home=teams[static_cast<std::size_t>(g.home)];
                        const auto& away=teams[static_cast<std::size_t>(g.away)];
                        const auto score=league.live_fixture(fixture,now);
                        const Font& font=sign_renderer.font();
                        const float x=static_cast<float>(screen_width-400),y=250;
                        const bool table=stand_seat.on;
                        DrawRectangle(static_cast<int>(x),static_cast<int>(y),380,table ? 236 : 96,Fade(BLACK,.7F));
                        DrawTextEx(font,g.final ? "BASBALLIGO - LA FINALO" : "BASBALLIGO",{x+12,y+8},16,1,GOLD);
                        DrawRectangle(static_cast<int>(x+12),static_cast<int>(y+32),10,22,away.primary);
                        // The city, then its runs in a column of their own (the names are long).
                        DrawTextEx(font,away.nick,{x+28,y+32},22,1,RAYWHITE);
                        DrawTextEx(font,TextFormat("%d",score.away_runs),{x+190,y+32},22,1,GOLD);
                        DrawRectangle(static_cast<int>(x+12),static_cast<int>(y+60),10,22,home.primary);
                        DrawTextEx(font,home.nick,{x+28,y+60},22,1,RAYWHITE);
                        DrawTextEx(font,TextFormat("%d",score.home_runs),{x+190,y+60},22,1,GOLD);
                        const int pitch_minute=static_cast<int>(League::first_pitch);
                        const char* state=riot ? "RIOT!" : live.phase==Phase::Pregame ? TextFormat("%02d:%02d start",pitch_minute/60,pitch_minute%60) :
                            live.phase==Phase::Over ? "FINAL" : TextFormat("%s %d",live.top ? "TOP" : "BOT",live.inning);
                        DrawTextEx(font,state,{x+232,y+34},20,1,riot ? RED : GOLD);
                        if(live.phase==Phase::Playing && !riot) {
                            // The outs, and a little diamond with the runners on.
                            DrawTextEx(font,TextFormat("%d OUT",std::min(live.outs,2)),{x+232,y+62},18,1,RAYWHITE);
                            // Who's up, the count, and who's pitching (and how many he's thrown).
                            if(live.play>=0) {
                                const auto line=league.linescore(live.fixture);
                                const auto& pl=line.plays[static_cast<std::size_t>(live.play)];
                                const auto& batting=league.roster(pl.home ? g.home : g.away);
                                const auto& pitching=league.roster(pl.home ? g.away : g.home);
                                int thrown=0;
                                for(const auto& q:line.plays) if(q.home==pl.home && q.pitcher==pl.pitcher && q.at<=pl.at) thrown+=q.pitches;
                                const float yb=y+(table ? 240.0F : 100.0F);
                                DrawRectangle(static_cast<int>(x),static_cast<int>(yb),380,46,Fade(BLACK,.7F));
                                DrawTextEx(font,TextFormat("AB  %s  %d-%d",batting.lineup[static_cast<std::size_t>(pl.batter)].name.c_str(),pl.balls,pl.strikes),{x+12,yb+4},16,1,RAYWHITE);
                                DrawTextEx(font,TextFormat("P   %s  (%d)",pitching.staff[static_cast<std::size_t>(pl.pitcher)].name.c_str(),thrown),{x+12,yb+24},16,1,Fade(RAYWHITE,.8F));
                            }
                            const Vector2 c{x+345,y+62};
                            const std::array<Vector2,3> bag{{{c.x+11,c.y},{c.x,c.y-11},{c.x-11,c.y}}};
                            for(int k=0;k<3;++k) {
                                const bool on=(live.bases>>k)&1;
                                DrawPoly(bag[static_cast<std::size_t>(k)],4,6,45,on ? GOLD : Fade(RAYWHITE,.25F));
                            }
                        }
                        if(table) {
                            DrawTextEx(font,TextFormat("Season %d table",league.season()),{x+12,y+96},16,1,GOLD);
                            int row=0;
                            for(const auto& s:league.standings(now)) {
                                const auto& team=teams[static_cast<std::size_t>(s.team)];
                                DrawRectangle(static_cast<int>(x+12),static_cast<int>(y+122+row*22),8,16,team.primary);
                                DrawTextEx(font,TextFormat("%d. %s  %d-%d",row+1,team.name,s.wins,s.losses),{x+26,y+120+row*22.0F},17,1,RAYWHITE);
                                ++row;
                            }
                        }
                        break;
                    }
                }
                if(garage_here && !vehicles.driver() && garage_site.in_mansion(loot_feet)) {
                    const bool owned=game::economy::SkyMansion::owned(loot_session.economy());
                    const auto bed=garage_site.to_world(world::garage::bed);
                    const bool by_bed=std::hypot(loot_feet.x-bed.x,loot_feet.z-bed.z)<2.4F && std::abs(loot_feet.y-bed.y)<1.5F;
                    const char* line=!owned ? TextFormat("MANSION FOR SALE - %lld kronoj (with its own parking bay)   E / USE: BUY",static_cast<long long>(game::economy::SkyMansion::price)) :
                        by_bed ? "E / USE: SLEEP UNTIL MORNING" : nullptr;
                    if(line) {
                        const int tw=MeasureText(line,22);
                        DrawRectangle(screen_width/2-tw/2-14,screen_height-252,tw+28,34,Fade(BLACK,.6F));
                        DrawText(line,screen_width/2-tw/2,screen_height-246,22,GOLD);
                    }
                }
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
        if(store_open) store_ui.draw(wardrobe_catalogue,loot_session.economy(),mafio.player_gang()==0,store_status,screen_width,screen_height);
        if(motor_menu.is_open()) motor_menu.draw(static_cast<long long>(loot_session.economy().money),screen_width,screen_height);
        if(gear_menu.is_open()) gear_menu.draw(static_cast<long long>(loot_session.economy().money),screen_width,screen_height);
        else if(!store_prompt.empty() && !inventory_open && !(shop_open||poker_open)) {
            const Font& font=sign_renderer.font();
            const float size=20,width=MeasureTextEx(font,store_prompt.c_str(),size,1).x;
            DrawRectangle(static_cast<int>(screen_width/2-width/2-10),screen_height-134,static_cast<int>(width+20),30,Fade(BLACK,.55F));
            DrawTextEx(font,store_prompt.c_str(),{screen_width/2-width/2,static_cast<float>(screen_height-129)},size,1,YELLOW);
        }
        if(shop_open && current_shop) play_ui.draw_shop(economy,*current_shop,loot_session.economy(),item_registry,loot_session.inventory(),shop_status,screen_width,screen_height);
        // Squad revive: down (the bleed and the pick-up), or picking a teammate up.
        if(squad_play() && !mode_session.terminal()) {
            const auto bar=[&](const char* title,float fill,Color colour,int y) {
                const int w=320,x=screen_width/2-w/2,tw=MeasureText(title,20),box=std::max(w,tw);
                DrawRectangle(screen_width/2-box/2-12,y-30,box+24,52,Fade(BLACK,.6F));
                DrawText(title,screen_width/2-tw/2,y-25,20,colour);
                DrawRectangle(x,y,w,10,Fade(WHITE,.15F));
                DrawRectangle(x,y,static_cast<int>(w*std::clamp(fill,0.0F,1.0F)),10,colour);
            };
            const int y=screen_height/2+90;
            if(player_health.downed()) {
                if(squad_revive.progress()>0) bar("BEING PICKED UP - HOLD ON",squad_revive.progress(),Color{120,235,140,255},y);
                else bar("YOU'RE DOWN - CRAWL TO COVER FOR A TEAMMATE",squad_revive.bleed(),Color{235,80,60,255},y);
            } else if(const auto helping=squad_revive.helping();helping!=game::lan::SquadRevive::nobody)
                if(const auto* who=lan_play.find(helping))
                    bar(TextFormat("PICKING UP %s",who->name.c_str()),static_cast<float>(who->state.revive)/255.0F,Color{120,235,140,255},y);
        }
        // Out, with the side fighting on: follow them (the result comes when the side wins or falls).
        if(mode_session.squad_fighting()) {
            const auto* who=lan_play.find(static_cast<std::uint8_t>(std::max(0,spectate_slot)));
            const std::string line=TextFormat("YOU'RE OUT  ·  YOUR %s FIGHTS ON  ·  WATCHING %s",lan_sides==net::Sides::Squad ? "SQUAD" : "TEAM",
                                              who ? who->name.c_str() : "-");
            const char* keys=touch_ui ? "TAP TO WATCH THE NEXT  ·  HOME / H" : "TAB: WATCH THE NEXT  ·  H: HOME";
            const int lw=MeasureText(line.c_str(),22),kw=MeasureText(keys,16);
            DrawRectangle(screen_width/2-lw/2-16,screen_height-128,lw+32,64,Fade(BLACK,.6F));
            DrawText(line.c_str(),screen_width/2-lw/2,screen_height-120,22,Color{120,230,140,255});
            DrawText(keys,screen_width/2-kw/2,screen_height-90,16,LIGHTGRAY);
        } else if(mode_session.terminal()) play_ui.draw_result(mode_session,screen_width,screen_height);
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
        for(const int id:retired_bots) npcs.set_bot_hidden(id,true);
        // The Green Hand's GT is theirs: an outsider at the wheel has the gang after them.
        if(const auto* car=vehicles.driver();car && game_mode==game::GameMode::Explore && mafio.active()) {
            const auto* a=vehicles.asset(verda_region,car->id);
            if(a && a->vehicle.definition==game::mafia::green_hand_car && player_gang()!=0 && !gt_stolen) {
                gt_stolen=true;
                mafio.provoke_gang(0,match_time,"You took La Verda Mano's GT - the Green Hand wants it back");
            } else if(a && a->vehicle.definition==game::mafia::green_hand_car && player_gang()==0 && !gt_stolen) {
                gt_stolen=true;interaction_message="The crew's Verda Mano GT - bring it back in one piece";interaction_remaining=3;
            }
        } else gt_stolen=false;
        // The Ŝosea Patrolo grows with the law: an outpost more for every quarter of the towns the
        // Policio has cleared (all five once Verda is clean), each with its cruiser and two troopers.
        if(game::rules_for(game_mode).civilian_life && services.active() && !territory.turfs().empty() && !patrol_posts.empty()) {
            const float law=territory.balance(-1).law;
            int want=std::min(static_cast<int>(patrol_posts.size()),
                              game::services::outposts_open(law,law>=.999F)+career.outposts_built());   // the law's reach, and what an officer's authority built
#ifdef OUTLAND_DEV_TOOLS
            // OUTLAND_DEV_PATROL=N: open N outposts at once.
            if(const char* dev=std::getenv("OUTLAND_DEV_PATROL")) want=std::clamp(std::atoi(dev),want,static_cast<int>(patrol_posts.size()));
#endif
            if(patrol_open<want) {
                const auto& post=patrol_posts[static_cast<std::size_t>(patrol_open)];
                std::vector<std::string> before;for(const auto& v:vehicles.vehicles()) before.push_back(v.id);
                if(vehicles.spawn(verda_region,post.parking,post.parking_yaw,"highway_patrol"))
                    for(const auto& v:vehicles.vehicles())
                        if(std::find(before.begin(),before.end(),v.id)==before.end()) {services.add_highway(v.id,v.position,v.yaw);break;}
                // The outpost itself: the city kit's small modular building, in the nearest town's assets.
                if(auto& towns=verda_region.runtime_settlements();!towns.empty()) {
                    std::size_t near=0;
                    for(std::size_t t=1;t<towns.size();++t)
                        if(Vector3Distance(towns[t].center,post.at)<Vector3Distance(towns[near].center,post.at)) near=t;
                    world::WorldAsset asset;
                    asset.id="patrol_outpost_"+std::to_string(patrol_open+1);
                    asset.type=world::AssetType::House;
                    asset.purpose=world::BuildingPurpose::Police;
                    asset.model_path=game::services::outpost_model;
                    asset.rotation_y=post.yaw+180;
                    {
                        const float r=asset.rotation_y*DEG2RAD;
                        const Vector2 c=game::services::outpost_model_centre;
                        asset.position={post.at.x+c.x*std::cos(r)+c.y*std::sin(r),post.at.y,post.at.z-c.x*std::sin(r)+c.y*std::cos(r)};
                    }
                    asset.size={14,3.8F,6};
                    asset.collision=true;
                    towns[near].assets.push_back(std::move(asset));
                    world::physics::MeshCollisionLibrary::preload(verda_region);
                    world::physics::WorldCollision::invalidate_index();
                    static_world.invalidate();
                }
                services.post_troopers(post.at,post.yaw,npcs);
                if(patrol_open>0) {interaction_message="The Ŝosea Patrolo opens another outpost on the ring road";interaction_remaining=4;}
                TraceLog(LOG_INFO,"OUTLAND Ŝosea Patrolo outpost %d at %.0f %.0f",patrol_open+1,post.at.x,post.at.z);
                ++patrol_open;
            }
            // Ranchers in trouble call the patrol: the nearest cruiser rides out, the law is told.
            if(ranches.active())
                for(std::size_t r=0;r<ranches.ranches().size() && r<ranch_alarm.size();++r) {
                    const auto& ranch=ranches.ranches()[r];
                    const bool trouble=ranch.provoked_until>match_time;
                    if(trouble && !ranch_alarm[r] && ranch.spec) {
                        const Vector3 gate=ranch.frame.at(0,110);
                        services.guard(gate,vehicles);
                        game::law::JusticeFrame call;call.now=match_time;call.player={player.position.x,player.position.y-1,player.position.z};
                        justice.tip_off(1,call.player,call,island_life,"The "+std::string(ranch.spec->family)+" ranch called the Ŝosea Patrolo");
                    }
                    ranch_alarm[r]=trouble;
                }
        }
        // The mural: a side wall of the Verda City hideout (or its back), the one with the most
        // open ground in front of it.
        if(game::rules_for(game_mode).civilian_life && mafio.active() && !green_hand_mural.placed && mural_attempts<30 &&
           (mural_wait-=GetFrameTime())<=0) {
            mural_wait=2;++mural_attempts;
            if(const auto* home=mafio.casino()) {
                const game::life::Place* place=nullptr;
                for(const auto& p:island_life.places()) if(p.building_id==home->building) {place=&p;break;}
                Vector3 out=Vector3Subtract(home->door,home->interior);out.y=0;
                if(place && Vector3Length(out)>.1F) {
                    out=Vector3Normalize(out);
                    const float yaw=place->building_yaw*DEG2RAD;
                    const Vector3 lx{std::cos(yaw),0,-std::sin(yaw)},lz{std::sin(yaw),0,std::cos(yaw)};
                    const Vector3 centre{place->interior.x,home->interior.y,place->interior.z};
                    const float tall=std::max(3.0F,place->building_size.y);
                    float best=0;
                    // Walls, seen through the street trees.
                    const auto solid=[&](Vector3 a,Vector3 b) {
                        auto hit=combat_world.trace_segment(a,b,false,false,false);
                        for(int k=0;k<4 && hit.kind==game::combat::HitKind::Tree;++k) {
                            a=Vector3Add(hit.position,Vector3Scale(Vector3Normalize(Vector3Subtract(b,a)),.8F));
                            hit=combat_world.trace_segment(a,b,false,false,false);
                        }
                        return hit;
                    };
                    for(const Vector3 want:{Vector3{-out.z,0,out.x},Vector3{out.z,0,-out.x},Vector3Negate(out)}) {
                        // Square to the building: the face whose axis is nearest.
                        const bool along_x=std::abs(Vector3DotProduct(want,lx))>std::abs(Vector3DotProduct(want,lz));
                        const Vector3 axis=Vector3Scale(along_x ? lx : lz,Vector3DotProduct(want,along_x ? lx : lz)<0 ? -1.0F : 1.0F);
                        const float reach=(along_x ? place->building_size.x : place->building_size.z)*.5F;
                        const float length=along_x ? place->building_size.z : place->building_size.x;
                        float width=std::min(length*.85F,11.0F),height=width*game::mafia::GangLogos::mural_height/game::mafia::GangLogos::mural_width;
                        if(height>tall*.8F) {height=tall*.8F;width=height*game::mafia::GangLogos::mural_width/game::mafia::GangLogos::mural_height;}
                        if(width<3) continue;
                        const float y=centre.y+std::min(.8F+height*.5F,tall-.25F-height*.5F);
                        const Vector3 face{centre.x+axis.x*reach,y,centre.z+axis.z*reach};
                        const auto wall=solid(Vector3Add(face,Vector3Scale(axis,6)),Vector3Subtract(face,Vector3Scale(axis,3)));
                        if((wall.kind!=game::combat::HitKind::Building && wall.kind!=game::combat::HitKind::Structure) || std::abs(wall.normal.y)>.3F) continue;
                        // Both ends on the same wall, and open ground in front.
                        const Vector3 run{-axis.z,0,axis.x};
                        bool flat=true;
                        for(const float s:{-.45F,.45F}) {
                            const Vector3 end=Vector3Add(wall.position,Vector3Scale(run,s*width));
                            const auto edge=solid(Vector3Add(end,Vector3Scale(axis,2)),Vector3Subtract(end,Vector3Scale(axis,1)));
                            flat=flat && (edge.kind==game::combat::HitKind::Building || edge.kind==game::combat::HitKind::Structure) && std::abs(Vector3DotProduct(Vector3Subtract(edge.position,wall.position),axis))<.4F;
                        }
                        if(!flat) continue;
                        // Outdoors: open sky in front of it (a wall shared with the next building is not).
                        bool outdoors=true;
                        for(const float s:{-.3F,.3F}) {
                            const Vector3 from=Vector3Add(Vector3Add(wall.position,Vector3Scale(axis,2.5F)),Vector3Scale(run,s*width));
                            const auto roof=combat_world.trace_segment(from,Vector3Add(from,{0,30,0}),false,false,false);
                            outdoors=outdoors && (!roof.hit() || roof.kind==game::combat::HitKind::Tree);
                        }
                        if(!outdoors) continue;
                        float open=0;
                        for(const float s:{-.35F,0.0F,.35F}) {
                            const Vector3 from=Vector3Add(Vector3Add(wall.position,Vector3Scale(axis,.3F)),Vector3Scale(run,s*width));
                            const auto view=combat_world.trace_segment(from,Vector3Add(from,Vector3Scale(axis,14)),false,false,false);
                            open+=view.hit() ? Vector3Distance(from,view.position) : 14.0F;
                        }
                        if(open>best+1) {best=open;green_hand_mural={wall.position,axis,width,true};}
                    }
                    if(green_hand_mural.placed) banner_key.clear();   // the walls are in: hang the banners on them
                    if(green_hand_mural.placed)
                        TraceLog(LOG_INFO,"OUTLAND Green Hand mural at %.1f %.1f %.1f facing %.2f %.2f, %.1f m wide",green_hand_mural.centre.x,green_hand_mural.centre.y,
                                 green_hand_mural.centre.z,green_hand_mural.normal.x,green_hand_mural.normal.z,green_hand_mural.width);
                }
            }
        }
        // La Verda Mano's banners: over each hideout it holds (on a staff off the wall above the
        // door) and on a pole beside the flag in the square of every town it holds.
        if(game::rules_for(game_mode).civilian_life && mafio.active()) {
            std::string key;
            for(const auto& crew:mafio.crews()) key+=static_cast<char>('0'+crew.gang);
            if(key!=banner_key) {
                banner_key=key;
                std::vector<game::culture::FlagPole> banners;
                for(const auto& crew:mafio.crews()) {
                    if(crew.gang!=0) continue;
                    Vector3 out=Vector3Subtract(crew.door,crew.interior);out.y=0;
                    if(Vector3Length(out)<.1F) continue;
                    out=Vector3Normalize(out);
                    const Vector3 side{-out.z,0,out.x};
                    game::culture::FlagPole pole;
                    pole.place="verda_mano:"+crew.building;pole.design=game::culture::FlagDesign::VerdaMano;
                    const Vector3 from=Vector3Add(crew.door,Vector3Add(Vector3Scale(out,1.2F),{0,3.5F,0}));
                    const auto wall=combat_world.trace_segment(from,Vector3Subtract(from,Vector3Scale(out,4.5F)),false,false,false);
                    if((wall.kind==game::combat::HitKind::Building || wall.kind==game::combat::HitKind::Structure) && std::abs(wall.normal.y)<.4F) {
                        pole.base=Vector3Add(wall.position,Vector3Scale(out,.05F));
                        pole.tip=Vector3Add(pole.base,Vector3Scale(Vector3Normalize(Vector3Add(Vector3Scale(out,.75F),{0,.66F,0})),3.2F));
                        pole.width=2.3F;
                    } else {
                        const Vector3 foot=Vector3Add(crew.door,Vector3Add(Vector3Scale(out,2.2F),Vector3Scale(side,2.6F)));
                        pole.base={foot.x,world::terrain::TerrainHeight::sample(foot.x,foot.z),foot.z};
                        pole.tip=Vector3Add(pole.base,{0,6.5F,0});
                        pole.width=1.8F;
                    }
                    pole.fly=side;
                    banners.push_back(pole);
                }
                for(const auto& flag:sign_renderer.signage().flags) {
                    const auto colon=flag.place.rfind(":square");
                    if(colon==std::string::npos) continue;
                    const std::string town=flag.place.substr(0,colon);
                    const auto held=std::find_if(territory.turfs().begin(),territory.turfs().end(),
                        [&](const game::mafia::Turf& t){return t.id==town && t.ranch<0 && t.owner==0;});
                    if(held==territory.turfs().end()) continue;
                    const Vector3 across=Vector3Normalize(Vector3{-flag.fly.z,0,flag.fly.x});
                    game::culture::FlagPole pole;
                    pole.place="verda_mano:"+flag.place;pole.design=game::culture::FlagDesign::VerdaMano;
                    const Vector3 foot=Vector3Add(flag.base,Vector3Scale(across,2.8F));
                    pole.base={foot.x,world::terrain::TerrainHeight::sample(foot.x,foot.z),foot.z};
                    pole.tip=Vector3Add(pole.base,{0,std::max(5.0F,flag.tip.y-flag.base.y-1.2F),0});
                    pole.fly=flag.fly;pole.width=std::max(2.0F,flag.width*.85F);
                    banners.push_back(pole);
                }
                // La Verda Mano's own chapel flies its banner by the door, whoever holds the town.
                for(const auto& church:churches) {
                    if(!church.green_hand) continue;
                    const float a=church.yaw*DEG2RAD;
                    const Vector3 out{-std::sin(a),0,-std::cos(a)},side{std::cos(a),0,-std::sin(a)};
                    const Vector3 foot=Vector3Add(church.door,Vector3Add(Vector3Scale(side,-4.6F),Vector3Scale(out,-.5F)));
                    game::culture::FlagPole pole;
                    pole.place="verda_mano:chapel";pole.design=game::culture::FlagDesign::VerdaMano;
                    pole.base={foot.x,world::terrain::TerrainHeight::sample(foot.x,foot.z),foot.z};
                    pole.tip=Vector3Add(pole.base,{0,7.5F,0});
                    pole.fly=side;pole.width=2.2F;
                    banners.push_back(pole);
                }
                sign_renderer.set_banners(std::move(banners));
            }
        }
        // The compass's marks, to lead the player: the job or orders and the waypoint set on the map
        // (always on the strip, with how far), the safe circle in Battle Royale, and the places that
        // matter close by - your crew, rival hideouts, the police, the hospital, shops, the bus.
        std::vector<game::hud::CompassMark> compass_marks;
        {
            using game::hud::Glyph;
            const Vector3 me=player.position;
            const auto flat=[&](Vector3 at){return std::hypot(at.x-me.x,at.z-me.z);};
            if(const auto& pin=map_screen.pin();pin) {
                if(flat(*pin)<18 && !map_screen.is_open()) {
                    map_screen.clear_pin();interaction_message="Waypoint reached";interaction_remaining=2.5F;
                } else compass_marks.push_back({*pin,Glyph::Waypoint,Color{255,110,210,255},"WAYPOINT",true});
            }
            if(career.job().kind!=game::career::JobKind::None && flat(career.job().target)>14)
                compass_marks.push_back({career.job().target,Glyph::Objective,career.on_duty() ? Color{90,150,255,255} : Color{255,196,70,255},career.job().title,true});
            if(outbreak.active()) {
                if(const auto goal=outbreak.goal();goal && flat(*goal)>4)
                    compass_marks.push_back({*goal,Glyph::Objective,Color{255,214,120,255},
                        outbreak.story().stage()==game::outbreak::Stage::Drive || outbreak.story().stage()==game::outbreak::Stage::Clear ? "THE BARN" : "UNCLE RON",true});
                if(outbreak.has_hideout()) compass_marks.push_back({outbreak.hideout(),Glyph::Hideout,Color{46,204,96,255},"HIDEOUT",false});
                for(std::size_t k=0;k<outbreak.holdouts().size();++k) if(outbreak.found(static_cast<int>(k))) {
                    const auto& h=outbreak.holdouts()[k];
                    const auto& spec=game::outbreak::holdouts[static_cast<std::size_t>(h.spec)];
                    if(flat(h.centre)<1800) compass_marks.push_back({h.centre,Glyph::Camp,spec.color,std::string(spec.faction),false});
                }
                if(outbreak.tower().found && outbreak.state().tower_seen && flat(outbreak.tower().centre)<1200)
                    compass_marks.push_back({outbreak.tower().centre,Glyph::Zone,Color{200,40,40,255},"???",false});
            }
            if(!campaign_path.empty() && !story.objective().empty() && uncle_ron.placed && flat(uncle_ron.at)>4)
                compass_marks.push_back({uncle_ron.at,Glyph::Objective,Color{255,214,120,255},"UNCLE RON",true});
            // Uncle Dario at the Green Hand's bar: on the strip for a nephew with no road yet (close by for anyone else outside the law).
            if(!campaign_path.empty() && !story.intro() && career.state().side==game::career::Side::Civilian)
                if(const auto* bar=mafio.casino();bar && bar->gang==0 && !bar->disbanded && !bar->members.empty()) {
                    const auto* boss=br_bots.find(bar->members.front());
                    const Vector3 at=boss && boss->alive() ? boss->position() : bar->door;
                    const bool lead=!trucking.hired() && !mafio.provoked(0,match_time);
                    if(flat(at)>3.5F && (lead || flat(at)<300))
                        compass_marks.push_back({at,Glyph::Hideout,game::mafia::gangs[0].color,"UNCLE DARIO",lead});
                }
            if(trucking.hired() && trucking.job().stage!=game::freight::HaulStage::None && flat(trucking.target())>game::freight::Trucking::stop_radius)
                compass_marks.push_back({trucking.target(),Glyph::Objective,Color{90,220,140,255},trucking.title(),true});
            if(game_mode==game::GameMode::BattleRoyale && !mode_session.terminal()) {
                const auto zone=mode_session.zone();
                const bool next=zone.next_radius<zone.radius-1;
                const Vector2 centre=next ? zone.next_center : zone.center;
                const float radius=next ? zone.next_radius : zone.radius;
                const float out=std::hypot(me.x-centre.x,me.z-centre.y)-radius;
                if(out>-15) compass_marks.push_back({{centre.x,0,centre.y},Glyph::Zone,Color{120,190,255,255},"SAFE ZONE",true});
            }
            // The player's side (LAN): each teammate on the strip, by name.
            for(const auto& r:lan_play.remotes())
                if(r.known && (r.state.flags&net::StateAlive) && teammate(r.slot) && (flat(r.shown)>6 || down(r)))
                    compass_marks.push_back({r.shown,Glyph::Objective,down(r) ? Color{255,150,70,255} : Color{120,235,140,255},
                                             down(r) ? r.name+" - DOWN" : r.name,false});
            if(game::rules_for(game_mode).civilian_life) {
                // The nearest of each, if close enough to matter.
                const auto nearest=[&](Glyph glyph,Color colour,const std::string& label,float range,auto&& points) {
                    const Vector3* best=nullptr;float d=range;
                    for(const Vector3& at:points) if(const float f=flat(at);f<d) {d=f;best=&at;}
                    if(best) compass_marks.push_back({*best,glyph,colour,label,false});
                };
                if(const auto* site=services.nearest_medical(player.position);site && flat(site->door)<2500)
                    compass_marks.push_back({site->door,Glyph::Hospital,Color{220,50,50,255},site->hospital ? "HOSPITAL" : "CLINIC",false});
                if(prison_until>=0) {
                    if(const auto* mark=prison_yard.snitch()>=0 && career.state().side==game::career::Side::Gang && !in_shu ? prison_yard.find(prison_yard.snitch()) : nullptr)
                        compass_marks.push_back({prison_yard.world(*mark),Glyph::Objective,Color{255,90,80,255},"SNITCH",true});
                    compass_marks.push_back({prison_bunk(in_shu),Glyph::Home,Color{230,180,90,255},"BUNK",false});
                }
                if(career.custody()>0) compass_marks.push_back({prison_intake,Glyph::Objective,Color{90,150,255,255},"PRISON",true});
                else if(!callouts.escorts().empty()) compass_marks.push_back({prison_intake,Glyph::Police,Color{90,150,255,255},"PRISON",false});
                else if(flat(prison_intake)<900) compass_marks.push_back({prison_intake,Glyph::Police,Color{170,170,176,255},"PRISON",false});
                {
                    std::vector<Vector3> stations;
                    for(const auto& unit:services.units()) if(unit.kind==game::services::UnitKind::Police) stations.push_back(unit.home);
                    nearest(Glyph::Police,Color{70,130,255,255},"POLICE",career.on_duty() ? 2500.0F : 700.0F,stations);
                }
                {   // Marnafto: the nearest filling station - from far off when the tank runs low.
                    std::vector<Vector3> pumps;
                    for(const auto& site:motor_sites) if(site.kind==world::motor::Kind::Gas) pumps.push_back(site.origin);
                    float range=500;
                    if(const auto* v=vehicles.driver()) if(const auto* a=vehicles.asset(verda_region,v->id);a && a->vehicle.fuel<.3F) range=4000;
                    nearest(Glyph::Shop,Color{238,122,26,255},"MARNAFTO",range,pumps);
                    // At a rig's wheel: the nearest diesel lane (from anywhere once the tank is under 35%).
                    if(const auto* v=vehicles.driver())
                        if(const auto* a=vehicles.asset(verda_region,v->id);a && a->model_path.find("semi_body")!=std::string::npos)
                            nearest(Glyph::Shop,Color{238,122,26,255},"DIZELO",a->vehicle.fuel<.35F ? 1e9F : 900.0F,diesel_lanes);
                }
                {   // Armilejo: the nearest gun shop, from further off.
                    std::vector<Vector3> doors;
                    for(const auto& shop:economy.shops()) if(shop.kind=="guns") doors.push_back(shop.door);
                    nearest(Glyph::Shop,Color{196,160,70,255},"ARMILEJO",900.0F,doors);
                }
                for(const char* kind:{"general","medical","hardware","pub"}) {
                    std::vector<Vector3> doors;
                    for(const auto& shop:economy.shops()) if(shop.kind==kind) doors.push_back(shop.door);
                    const std::string k=kind;
                    nearest(Glyph::Shop,k=="medical" ? Color{240,120,140,255} : k=="hardware" ? Color{200,160,90,255} : k=="pub" ? Color{230,170,60,255} : Color{120,210,130,255},
                            k=="general" ? "GENERAL STORE" : k=="medical" ? "PHARMACY" : k=="hardware" ? "HARDWARE" : "PUB",450.0F,doors);
                }
                {
                    std::vector<Vector3> shelters;
                    for(const auto& stop:bus_line.stops()) shelters.push_back(stop.shelter);
                    nearest(Glyph::Bus,Color{250,205,40,255},"BUS STOP",260.0F,shelters);
                }
                if(clothing_store && flat(clothing_store->front)<900)
                    compass_marks.push_back({clothing_store->front,Glyph::Shop,Color{60,200,130,255},"VESTAĴEJO",false});
                if(penthouse.found() && game::economy::Penthouse::owned(loot_session.economy()))
                    compass_marks.push_back({penthouse.to_world({0,0,0}),Glyph::Home,Color{255,214,90,255},"HOME",false});
                if(garage_here && game::economy::SkyMansion::owned(loot_session.economy()))
                    compass_marks.push_back({garage_site.to_world(world::garage::door),Glyph::Home,Color{255,214,90,255},"HOME",false});
                // A ball game on (or about to be) within a drive: its park on the compass.
                if(!league.fixtures().empty())
                    for(int p=0;p<5;++p) {
                        const auto& site=parks[static_cast<std::size_t>(p)];
                        if(site.team<0 || flat(site.origin)>1400) continue;
                        const auto live=league.live(p,island_life.clock().minutes);
                        if(live.phase==game::sports::Phase::Pregame || live.phase==game::sports::Phase::Playing)
                            compass_marks.push_back({site.to_world(world::ballpark::gate),Glyph::Shop,game::sports::teams[static_cast<std::size_t>(p)].secondary,"PLAY BALL",false});
                    }
                // Hideouts: your own crew's (the nearest, anywhere) and any rival's close by.
                const int mine=player_gang();
                std::vector<Vector3> own;
                for(const auto& crew:mafio.crews()) {
                    const auto& gang=game::mafia::gangs[static_cast<std::size_t>(crew.gang)];
                    if(crew.gang==mine) own.push_back(crew.door);
                    else if((mine>=0 || career.on_duty()) && flat(crew.door)<400.0F)   // a rival's, close by (not a civilian's business)
                        compass_marks.push_back({crew.door,Glyph::Hideout,gang.color,std::string(gang.name)+" - "+crew.town,false});
                }
                if(mine>=0) {
                    nearest(Glyph::Hideout,game::mafia::gangs[static_cast<std::size_t>(mine)].color,"YOUR CREW",1e9F,own);
                    std::vector<Vector3> docks;
                    for(const auto& dock:smuggling_docks) docks.push_back(dock.point);
                    nearest(Glyph::Dock,Color{90,210,230,255},"DOCKS",700.0F,docks);
                }
                for(const auto& t:territory.turfs()) if(t.ranch>=0 && flat(t.centre)<600) {
                    const auto& family=territory.families()[static_cast<std::size_t>(t.ranch)];
                    compass_marks.push_back({t.centre,Glyph::Ranch,Color{176,128,74,255},family.name.empty() ? t.name : family.name+" ranch",false});
                }
                for(int i=0;career.on_duty() && i<patrol_open && i<static_cast<int>(patrol_posts.size());++i)
                    if(flat(patrol_posts[static_cast<std::size_t>(i)].at)<1400)
                        compass_marks.push_back({patrol_posts[static_cast<std::size_t>(i)].at,Glyph::Police,Color{214,170,100,255},"ŜOSEA PATROLO",false});
                if(career.on_duty() && police_camp.placed())
                    compass_marks.push_back({police_camp.line(),Glyph::Camp,Color{215,60,50,255},"TRAINING CAMP",false});
            }
        }
        // The compass, in every circumstance: where the camera looks.
        if(!map_screen.is_open() && !poker_open)
            game::hud::draw_compass(game::hud::bearing(Vector3Subtract(camera.target,camera.position)),screen_width,32,camera.position,compass_marks);
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
                centred(TextFormat("HELICOPTER %d OF %d: %s  |  %d ABOARD",drop_ride+1,static_cast<int>(drop_fleet.size()),game::aircraft::heading_name(ride.heading),jumpers),140,18,Color{255,220,120,255});
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
            // The outbreak: red but where the living are; the holdouts found, the hideout, the tower.
            if(outbreak.active()) {
                view.danger=true;
                for(const auto& c:outbreak.clearings({player.position.x,player.position.y-1,player.position.z})) view.clear.push_back({c.centre,c.radius});
                for(const auto& town:verda_region.settlements()) if(town.id=="capital_verda") view.ground_zero=town.center;
                for(std::size_t k=0;k<outbreak.holdouts().size();++k) if(outbreak.found(static_cast<int>(k))) {
                    const auto& h=outbreak.holdouts()[k];
                    const auto& spec=game::outbreak::holdouts[static_cast<std::size_t>(h.spec)];
                    view.bases.push_back({h.centre,spec.color,std::string(spec.faction),true});
                }
                if(outbreak.has_hideout()) view.bases.push_back({outbreak.hideout(),Color{46,204,96,255},"YOUR HIDEOUT",true});
                if(outbreak.tower().found && outbreak.state().tower_seen) view.bases.push_back({outbreak.tower().centre,Color{200,40,40,255},"???",true});
            }
            for(const auto& stop:bus_line.stops()) view.stops.push_back(stop.shelter);
            if(bus_line.running()) for(const auto& bus:bus_line.buses()) if(bus.running) view.buses.push_back({bus.position,bus.yaw});
            // The underworld only on a map that is its business: a cop knows every hideout and what
            // the Policio cleared; a gangster their own gang's; nobody else any of it.
            for(const auto& crew:mafio.crews()) {
                const auto& gang=game::mafia::gangs[static_cast<std::size_t>(crew.gang)];
                const bool known=career.on_duty() || crew.gang==player_gang();
                if(!known) continue;
                if(crew.disbanded) {view.cleared.push_back(crew.door);continue;}
                view.turf.push_back({crew.door,gang.color});
                if(crew.base) view.bases.push_back({crew.door,gang.color,std::string(gang.name)});
            }
            // The truckers' brotherhood's home: La Ringo Okcidento (a brother's map).
            if(!outbreak.active() && game::rules_for(game_mode).civilian_life && career.trucker())
                view.bases.push_back({world::highways::brotherhood_home().front_door(0),Color{232,140,40,255},"LA FRATECO - LA RINGO",true});
            // A gangster's map: who holds every town and ranch, the tags, the standings.
            if(const int mine=player_gang();mine>=0 && !territory.turfs().empty()) {
                view.gang_map=true;
                int yours=0;
                for(std::size_t i=0;i<territory.turfs().size();++i) {
                    const auto& t=territory.turfs()[i];
                    const bool held=territory.controls(static_cast<int>(i),mine);
                    yours+=held;
                    if(t.ranch>=0) {
                        const auto& family=territory.families()[static_cast<std::size_t>(t.ranch)];
                        const bool tied=family.stance!=game::mafia::Stance::Independent && family.gang>=0;
                        const std::string holder=family.name+(family.stance==game::mafia::Stance::Allied ? " - with "+gang_name(family.gang) :
                            family.stance==game::mafia::Stance::Extorted ? " - pays "+gang_name(family.gang) : family.fierce ? " - fiercely independent" : " - independent");
                        view.territory.push_back({t.centre,t.radius,tied ? faction_color(family.gang) : faction_color(t.owner),t.name,holder,100,held,true});
                    } else if(t.cleared) view.territory.push_back({t.centre,t.radius,Color{90,150,255,255},t.name,"cleared by the Policio",0,false,false});
                    else view.territory.push_back({t.centre,t.radius,faction_color(t.owner),t.name,gang_name(t.owner),t.hold,held,false});
                }
                for(const auto& tag:territory.tags()) view.tags.push_back({tag.position,faction_color(tag.gang)});
                for(const int f:territory.ranking()) {
                    std::string line=faction_name(f);
                    if(f<game::mafia::gang_count) line+=TextFormat("  -  %d towns, %d ranches",territory.towns_of(f),territory.ranches_with(f));
                    else line+=territory.families()[static_cast<std::size_t>(f-game::mafia::gang_count)].stance==game::mafia::Stance::Independent ? "  -  their own ranch" : "";
                    if(f==mine) line+="  (YOU)";
                    view.standings.push_back({line,faction_color(f)});
                }
                view.gang_goal=island_ruled ? "YOUR GANG RULES VERDA" :
                    TextFormat("Take the island: %d of %d turfs are yours",yours,static_cast<int>(territory.turfs().size()));
            }
            if(game::rules_for(game_mode).civilian_life && penthouse.found()) {
                view.home=penthouse.to_world({0,0,0});
                view.home_owned=game::economy::Penthouse::owned(loot_session.economy());
            }
            view.marks=compass_marks;
            // The ballparks on the map, each in its club's colour.
            if(game::rules_for(game_mode).civilian_life)
                for(const auto& site:parks)
                    if(site.team>=0)
                        view.marks.push_back({site.to_world(world::ballpark::gate),game::hud::Glyph::Shop,game::sports::teams[static_cast<std::size_t>(site.team)].primary,
                            TextFormat("%s BALLPARK",game::sports::teams[static_cast<std::size_t>(site.team)].nick),false});
            if(game::rules_for(game_mode).civilian_life && garage_here && !game::economy::SkyMansion::owned(loot_session.economy()))
                view.marks.push_back({garage_site.to_world(world::garage::door),game::hud::Glyph::Home,Color{230,90,90,255},
                    TextFormat("MANSION FOR SALE %lld kr",static_cast<long long>(game::economy::SkyMansion::price)),false});
            for(const auto& home:homes_for_sale) {
                const bool mine=game::economy::owns(loot_session.economy(),home);
                view.marks.push_back({home.board,game::hud::Glyph::Home,mine ? Color{255,214,90,255} : Color{230,90,90,255},
                    mine ? "HOME" : TextFormat("FOR SALE %lld kr",static_cast<long long>(home.price)),false});
            }
            if(clothing_store && std::hypot(clothing_store->front.x-player.position.x,clothing_store->front.z-player.position.z)>=900)
                view.marks.push_back({clothing_store->front,game::hud::Glyph::Shop,Color{60,200,130,255},"VESTAĴEJO",false});
            view.marks.push_back({prison_intake,game::hud::Glyph::Police,Color{170,170,176,255},"ŜTATA MALLIBEREJO",false});
            for(int i=0;career.on_duty() && i<patrol_open && i<static_cast<int>(patrol_posts.size());++i) {   // a cop's map: every outpost
                const auto& post=patrol_posts[static_cast<std::size_t>(i)];
                if(std::hypot(post.at.x-player.position.x,post.at.z-player.position.z)>=1400)
                    view.marks.push_back({post.at,game::hud::Glyph::Police,Color{214,170,100,255},"ŜOSEA PATROLO",false});
            }
            if(game::rules_for(game_mode).civilian_life && !territory.turfs().empty() && underworld()) {
                const auto balance=territory.balance(player_gang());
                view.law=balance.law;view.crime=balance.crime;
                if(player_gang()>=0) view.crime_color=game::mafia::gangs[static_cast<std::size_t>(player_gang())].color;
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
        // The drive home with Uncle Ron: the screen goes dark, and comes back at his house.
        if(outbreak.active() && outbreak.dark()>0) {
            DrawRectangle(0,0,screen_width,screen_height,Fade(BLACK,outbreak.dark()));
            if(outbreak.dark()>.5F) {
                const char* caption="Somebody is shaking you awake...";
                const Font& f=sign_renderer.font();
                const float cw=MeasureTextEx(f,caption,26,1).x;
                DrawTextEx(f,caption,{screen_width/2-cw/2,screen_height/2-13.0F},26,1,Fade(Color{236,240,226,255},(outbreak.dark()-.5F)/.5F));
            }
        }
        if(!campaign_path.empty() && story.fade()>0) {
            DrawRectangle(0,0,screen_width,screen_height,Fade(BLACK,story.fade()));
            if(story.fade()>.6F) {
                const char* caption=story.caption();
                const Font& f=sign_renderer.font();
                const float cw=MeasureTextEx(f,caption,26,1).x;
                DrawTextEx(f,caption,{screen_width/2-cw/2,screen_height/2-13.0F},26,1,Fade(Color{236,240,226,255},(story.fade()-.6F)/.4F));
            }
        }
        if(touch_editor.is_open()) touch_editor.draw(settings,screen_width,screen_height,&sign_renderer.font());
        else pause_menu.draw(settings,screen_width,screen_height,&sign_renderer.font(),can_quit_game);
#ifdef OUTLAND_DEV_TOOLS
        DrawFPS(
            screen_width - 90,
            10
        );
#endif

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
    render::shading_shutdown();
    CloseWindow();

    initialized_ = false;
}

bool Renderer::initialized() const {
    return initialized_;
}

}
