#include "outland/game/combat/CombatWorld.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
using namespace outland;
using world::physics::CollisionMesh;
using world::physics::MeshCollisionLibrary;
using world::physics::WorldCollision;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
const std::string parts = "assets/verda/urban/Building Parts/";
}
int main() {
    MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);

    // Every placeable non-character model has real-shape collision.
    int models = 0, failed = 0;
    const auto start = std::chrono::steady_clock::now();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda")) {
        const auto extension = entry.path().extension();
        if ((extension != ".glb" && extension != ".gltf") || entry.path().string().find("/characters/") != std::string::npos) continue;
        CollisionMesh mesh; std::string error; ++models;
        if (!world::physics::load_collision_mesh(entry.path().string(), mesh, error)) { ++failed; std::cerr << entry.path() << ": " << error << '\n'; }
    }
    std::cout << models << " models in " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() << " ms\n";
    check(models > 550 && failed == 0, "all building/prop models load");

    // Windows are knocked out: atlas-painted panels in the modular kit, glass + fake interiors downtown.
    for (const char* name : {"brick_wall_window_1", "brick_wall_window_2", "stuco_wall_window_1", "fancy_brick_wall_window_2"}) {
        const auto* mesh = MeshCollisionLibrary::get(parts + name + ".glb");
        check(mesh && mesh->knocked_out == 2 && !mesh->mesh_knocked_out[0] && mesh->mesh_knocked_triangles[0].size() == 2, std::string("window panel removed: ") + name);
    }
    for (const char* name : {"brick_wall", "brick_wall_door", "brick_wall_door_frame", "brick_foot_wall"})
        check(MeshCollisionLibrary::get(parts + name + ".glb")->knocked_out == 0, std::string("solid piece untouched: ") + name);
    const auto* downtown = MeshCollisionLibrary::get("assets/verda/creator/downtown/parts/Brick_Window_Square_Single.gltf");
    check(downtown && downtown->knocked_out > 0, "downtown glass removed");
    check(MeshCollisionLibrary::get("assets/verda/creator/downtown/buildings/Building_Small_1.gltf")->knocked_out == 0,
        "hollow prebuilt facades keep their windows");
    check(MeshCollisionLibrary::get("assets/verda/survival/runtime/Glass.glb") == nullptr ||
          MeshCollisionLibrary::get("assets/verda/survival/runtime/Glass.glb")->knocked_out == 0, "small glass props stay");
    check(MeshCollisionLibrary::get("assets/verda/characters/runtime/missing.glb") == nullptr, "characters keep box collision");

    // A little test site: window wall, doorway wall, closed-door wall, low wall, stairs, hollow shack.
    world::VerdaRegion region(true);
    auto& settlements = region.runtime_settlements();
    settlements.emplace_back();
    auto& site = settlements.back();
    site.id = "mesh_test";
    const auto place = [&](const std::string& path, float x, float z, float yaw) {
        world::WorldAsset asset; asset.id = path; asset.model_path = path;
        asset.position = {x, world::terrain::TerrainHeight::sample(x, z), z}; asset.size = {3, 3, 3}; asset.rotation_y = yaw;
        site.assets.push_back(asset);
        return asset.position;
    };
    // Wall pieces are thin in X and run along Z; the window/door gap is local z -0.5..0.5.
    const auto window_wall = place(parts + "brick_wall_window_1.glb", 400, 400, 0);
    const auto door_wall = place(parts + "brick_wall_door_frame.glb", 420, 400, 0);
    const auto closed_wall = place(parts + "brick_wall_door.glb", 440, 400, 0);
    const auto low_wall = place(parts + "brick_foot_wall.glb", 460, 400, 0);
    const auto turned = place(parts + "brick_wall_door_frame.glb", 480, 400, 90);
    const auto walk = [&](Vector3 from, Vector3 to) {
        Vector3 p = from;
        for (int i = 0; i < 200; ++i) {
            const auto next = Vector3{p.x + (to.x - from.x) / 100, p.y, p.z + (to.z - from.z) / 100};
            const float feet = WorldCollision::ground_height(p, p.y, region);
            p = WorldCollision::resolve_body_movement(p, next, feet, region, .45F);
            p.y = WorldCollision::ground_height(p, feet, region);
        }
        return p;
    };
    const auto through = [&](Vector3 base, float z) {
        const auto end = walk({base.x - 1.5F, base.y, base.z + z}, {base.x + 1.5F, base.y, base.z + z});
        return end.x > base.x + 1;
    };
    check(through(window_wall, 0), "walk through a knocked-out window opening (low sill)");
    check(!through(window_wall, 1.0F), "solid wall beside the window blocks");
    check(through(door_wall, 0), "walk through a doorway");
    check(!through(closed_wall, 0), "closed door blocks");
    check(!through(low_wall, 0), "waist-high wall blocks walking");
    {
        const auto end = walk({turned.x, turned.y, turned.z - 1.5F}, {turned.x, turned.y, turned.z + 1.5F});
        check(end.z > turned.z + 1, "rotated doorway matches drawn rotation");
    }
    // Window vault over the waist-high wall; nothing to vault in the open.
    Vector3 landing{};
    check(WorldCollision::mesh_vault_target({low_wall.x - .7F, low_wall.y + 1, low_wall.z}, low_wall.y, {1, 0, 0}, region, landing) &&
          landing.x > low_wall.x + .4F, "vault the low wall");
    check(!WorldCollision::mesh_vault_target({low_wall.x - .7F, low_wall.y + 1, low_wall.z + 5}, low_wall.y, {1, 0, 0}, region, landing), "no vault without an obstacle");

    // Bullets and sight pass through knocked-out windows and doorways, not walls.
    game::combat::CombatWorld combat(region);
    const auto shot = [&](Vector3 base, float z, float y) {
        return combat.trace_segment({base.x - 3, base.y + y, base.z + z}, {base.x + 3, base.y + y, base.z + z}, false, false, false);
    };
    check(shot(window_wall, 0, 1.5F).kind != game::combat::HitKind::Structure, "shoot through the window");
    check(shot(window_wall, 1.0F, 1.5F).kind == game::combat::HitKind::Structure, "wall stops bullets");
    check(shot(window_wall, 0, 2.7F).kind == game::combat::HitKind::Structure, "lintel above the window stops bullets");
    check(shot(door_wall, 0, 1.2F).kind != game::combat::HitKind::Structure, "shoot through the doorway");
    check(shot(closed_wall, 0, 1.2F).kind == game::combat::HitKind::Structure, "closed door stops bullets");
    // Downtown window: some straight shots through the facade now pass where glass used to be.
    {
        const auto glass_wall = place("assets/verda/creator/downtown/parts/Brick_Window_Square_Single.gltf", 500, 400, 0);
        int open = 0, solid = 0;
        for (float x = -.9F; x <= .9F; x += .1F) for (float y = .2F; y <= 2.8F; y += .2F) {
            const auto hit = combat.trace_segment({glass_wall.x + x, glass_wall.y + y, glass_wall.z - 2}, {glass_wall.x + x, glass_wall.y + y, glass_wall.z + 2}, false, false, false);
            (hit.kind == game::combat::HitKind::Structure ? solid : open) += 1;
        }
        check(open > 10 && solid > 10, "downtown window is an opening in a solid wall");
    }

    // Stairs and floors: walking up the industrial stairs raises the feet, walking off drops back.
    {
        const auto stairs = place("assets/verda/industrial/shacks/Stairs.glb", 520, 400, 0);
        float best = 0;
        for (const float direction : {1.0F, -1.0F}) for (const int axis : {0, 1}) {
            Vector3 p{stairs.x - (axis == 0 ? direction * 1.2F : 0), stairs.y, stairs.z - (axis == 1 ? direction * 1.2F : 0)};
            float feet = p.y, highest = p.y;
            for (int i = 0; i < 40; ++i) {
                p.x += axis == 0 ? direction * .05F : 0; p.z += axis == 1 ? direction * .05F : 0;
                feet = WorldCollision::ground_height(p, feet, region);
                highest = std::max(highest, feet);
            }
            best = std::max(best, highest - stairs.y);
        }
        check(best > .8F, "climb the stairs: " + std::to_string(best));
        const auto gantry = place("assets/verda/industrial/shacks/Metal Gantry Floor.glb", 540, 400, 0);
        check(std::abs(WorldCollision::ground_height(gantry, gantry.y + .05F, region) - (gantry.y + .05F)) < .02F, "stand on a floor");
        check(WorldCollision::ground_height(gantry, gantry.y - 2, region) <= gantry.y + .001F, "floors above the head are not ground");
    }
    // Legacy 2D queries (vehicles, foliage, placement) keep the asset's box.
    check(WorldCollision::blocked({door_wall.x, 0, door_wall.z}, region, .2F), "box occupancy for non-walkers");
    std::cout << "mesh collision tests passed\n";
}
