#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/game/combat/CombatWorld.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
using namespace outland;
using world::physics::WorldCollision;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::size_t count_assets(const world::VerdaRegion& region) {
    std::size_t n = 0;
    for (const auto& s : region.settlements()) n += s.assets.size();
    return n;
}
Vector3 world_of(const dev::KitBuilding& b, float x, float z) {
    const float a = b.yaw * DEG2RAD;
    return {b.origin.x + x * std::cos(a) + z * std::sin(a), b.origin.y, b.origin.z - x * std::sin(a) + z * std::cos(a)};
}
// Walk a body in small steps toward a building-local point; returns the final world position.
Vector3 walk(const world::VerdaRegion& region, const dev::KitBuilding& b, Vector3 position, float x, float z) {
    const auto target = world_of(b, x, z);
    for (int i = 0; i < 400; ++i) {
        const float dx = target.x - position.x, dz = target.z - position.z, length = std::sqrt(dx * dx + dz * dz);
        if (length < .05F) break;
        const float step = std::min(.05F, length);
        const Vector3 next{position.x + dx / length * step, position.y, position.z + dz / length * step};
        position = WorldCollision::resolve_body_movement(position, next, position.y, region, .45F);
        position.y = WorldCollision::ground_height(position, position.y, region);
    }
    return position;
}
bool near(Vector3 p, Vector3 q, float tolerance) { return std::hypot(p.x - q.x, p.z - q.z) < tolerance; }
}
int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    const creator::CreatorAssetRegistry catalog;
    const auto make = [&](world::VerdaRegion& region) {
        check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_loot_defaults.map"), "load defaults");
        return dev::build_verda_towns(region, catalog);
    };
    world::VerdaRegion region(true);
    const auto before = count_assets(region);
    const auto report = make(region);
    std::cout << report.buildings << " buildings, " << report.assets << " pieces, " << report.skipped_lots << " skipped lots\n";
    for (const auto& p : report.skipped) std::cout << "  skipped lot " << p.x << ", " << p.z << '\n';
    check(report.missing.empty(), "every kit model is in the catalog");
    check(report.buildings >= 15 && report.assets > 400, "towns dressed");
    check(count_assets(region) == before + static_cast<std::size_t>(report.assets), "every piece is a world asset");
    world::VerdaRegion again(true);
    const auto repeat = make(again);
    check(repeat.assets == report.assets && count_assets(again) == count_assets(region), "deterministic");

    // Save/load round trip through the normal Creator map format.
    const auto path = std::filesystem::temp_directory_path() / "outland_world_kit_test.map";
    check(creator::CreatorMapIO::save(region, path.string()), "save");
    world::VerdaRegion loaded(false);
    check(creator::CreatorMapIO::load(loaded, path.string()) && count_assets(loaded) == count_assets(region), "map round trip");

    // Every building can be entered through its doorway, and the doorway is not a wall.
    game::combat::CombatWorld combat(region);
    int two_storey = 0;
    for (const auto& b : report.enterable) {
        auto start = world_of(b, b.door_x, -b.depth * .5F - 2.5F);
        start.y = WorldCollision::ground_height(start, b.origin.y, region);
        const auto inside = walk(region, b, start, b.door_x, -b.depth * .5F + 2.5F);
        check(near(inside, world_of(b, b.door_x, -b.depth * .5F + 2.5F), .2F), "walk in through the front door");
        check(std::abs(inside.y - (b.origin.y + .1F)) < .05F, "stand on the ground-floor slab");
        // Bullets fly in through the doorway at chest height.
        const auto shot = combat.trace_segment(Vector3Add(world_of(b, b.door_x, -b.depth * .5F - 3), {0, 1.4F, 0}),
                                               Vector3Add(world_of(b, b.door_x, -b.depth * .5F + 1), {0, 1.4F, 0}), false, false, false);
        check(!shot.hit() || shot.kind == game::combat::HitKind::Ground, "doorway is open to bullets");
        if (b.storeys < 2) continue;
        ++two_storey;
        // Upstairs: walk up the flight along +X and step off onto the upper floor.
        auto foot = world_of(b, b.stairs_start.x, b.stairs_start.z);
        foot.y = b.origin.y + .1F;
        foot = walk(region, b, foot, b.stairs_start.x + 6.0F, b.stairs_start.z);
        check(foot.y > b.origin.y + 3.0F, "climb to the upper floor: " + std::to_string(foot.y - b.origin.y));
        foot = walk(region, b, foot, b.stairs_start.x + 6.0F, b.stairs_start.z - 4);
        check(std::abs(foot.y - (b.origin.y + 3.1F)) < .05F, "walk around upstairs");
    }
    check(two_storey >= 5, "several two-storey buildings");
    std::cout << "world kit tests passed\n";
}
