#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/game/combat/CombatWorld.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/characters/NpcSystem.hpp"
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
    check(report.buildings >= 40 && report.assets > 400, "towns dressed: " + std::to_string(report.buildings));
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
        // 1.2 m out: on downtown sidewalks the street trees stand 2.4 m from the shopfronts.
        auto start = world_of(b, b.door_x, -b.depth * .5F - 1.2F);
        start.y = WorldCollision::ground_height(start, b.origin.y, region);
        const auto inside = walk(region, b, start, b.door_x, -b.depth * .5F + 2.5F);
        check(near(inside, world_of(b, b.door_x, -b.depth * .5F + 2.5F), .2F), "walk in through the front door");
        check(std::abs(inside.y - (b.origin.y + b.ground_floor)) < .05F, "stand on the ground-floor slab");
        // Bullets fly in through the doorway at chest height.
        const auto shot = combat.trace_segment(Vector3Add(world_of(b, b.door_x, -b.depth * .5F - 3), {0, 1.4F, 0}),
                                               Vector3Add(world_of(b, b.door_x, -b.depth * .5F + 1), {0, 1.4F, 0}), false, false, false);
        check(!shot.hit() || shot.kind == game::combat::HitKind::Ground, "doorway is open to bullets");
        if (b.storeys < 2) continue;
        ++two_storey;
        // Upstairs: walk up the flight and step off onto the upper floor, then across it.
        auto foot = world_of(b, b.stairs_start.x, b.stairs_start.z);
        foot.y = b.origin.y + b.ground_floor;
        const auto top = Vector3Add(b.stairs_start, Vector3Scale(b.stairs_direction, b.stairs_length));
        foot = walk(region, b, foot, top.x, top.z);
        check(std::abs(foot.y - (b.origin.y + b.upper_floor)) < .05F, "climb to the upper floor: " + std::to_string(foot.y - b.origin.y));
        foot = walk(region, b, foot, top.x + b.upstairs_walk.x, top.z + b.upstairs_walk.z);
        check(std::abs(foot.y - (b.origin.y + b.upper_floor)) < .05F, "walk around upstairs");
    }
    check(two_storey >= 30, "many multi-storey buildings: " + std::to_string(two_storey));

    // Roka's trailer park: twelve mobile homes, every one walkable from the lane through its door.
    {
        check(report.parks == 1, "trailer park built");
        int trailers = 0, entered = 0;
        for (const auto& settlement : region.settlements()) for (const auto& asset : settlement.assets) {
            const auto at = asset.model_path.find("residential/trailers/trailer_0");
            if (at == std::string::npos) continue;
            ++trailers;
            const int model = asset.model_path[at + 30] - '0';
            const float a = asset.rotation_y * DEG2RAD;
            const auto local = [&](float x, float z) {
                return Vector3{asset.position.x + x * std::cos(a) + z * std::sin(a), asset.position.y, asset.position.z - x * std::sin(a) + z * std::cos(a)};
            };
            const auto step_to = [&](Vector3 p, Vector3 target) {
                for (int i = 0; i < 600; ++i) {
                    const float dx = target.x - p.x, dz = target.z - p.z, l = std::sqrt(dx * dx + dz * dz);
                    if (l < .05F) break;
                    const float st = std::min(.05F, l);
                    p = WorldCollision::resolve_body_movement(p, {p.x + dx / l * st, p.y, p.z + dz / l * st}, p.y, region, .35F);
                    p.y = WorldCollision::ground_height(p, p.y, region);
                }
                return p;
            };
            // From the lane end of the lot: up the side steps (models 1-4) or the front steps (5-6).
            auto p = local(model <= 4 ? -1.5F : -4.0F, 8.0F);
            p.y = WorldCollision::ground_height(p, asset.position.y + .5F, region);
            if (model <= 4) p = step_to(step_to(p, local(-1.5F, 1.6F)), local(-1.5F, .56F));
            else p = step_to(p, local(-4.0F, .56F));
            p = step_to(p, local(.6F, .56F));
            const auto inside = local(.6F, .56F);
            if (std::hypot(p.x - inside.x, p.z - inside.z) < .2F && p.y > asset.position.y + .6F) ++entered;
            else std::cerr << "  could not enter trailer " << model << " at " << asset.position.x << "," << asset.position.z << '\n';
        }
        check(trailers == 12, "twelve trailers: " + std::to_string(trailers));
        check(entered == trailers, "walk into every trailer: " + std::to_string(entered) + "/" + std::to_string(trailers));
    }

    // Downtown: the old capital is gone, the grid is in, highways meet the grid edge.
    const world::Settlement* capital = nullptr;
    for (const auto& s : region.settlements()) if (s.id == "capital_verda") capital = &s;
    check(capital && capital->buildings.empty(), "old capital houses wiped");
    int grid_streets = 0, highways = 0;
    for (const auto& road : capital->roads) {
        const bool inside = std::abs(road.start.x) <= 167 && std::abs(road.start.z) <= 167 && std::abs(road.end.x) <= 167 && std::abs(road.end.z) <= 167;
        if (inside) { ++grid_streets; check(road.width == 12 && road.type == world::RoadType::Asphalt, "grid streets are 12 m asphalt"); }
        else if (std::abs(road.start.x) < 167 && std::abs(road.start.z) < 167) {
            ++highways;
            check((road.start.x == 0 && std::abs(road.start.z) == 166) || (road.start.z == 0 && std::abs(road.start.x) == 166), "highway joins a main street at the grid edge");
        }
    }
    check(grid_streets == 10 && highways == 4, "grid and four highways: " + std::to_string(grid_streets) + "/" + std::to_string(highways));
    int towers = 0;
    for (const auto& s : region.settlements()) for (const auto& a : s.assets) if (a.model_path.find("/city/towers/") != std::string::npos) {
        ++towers;
        // Towers are solid cover: walking at one from the street stops at its wall.
        Vector3 p{a.position.x + std::max(a.size.x, a.size.z) * .5F + 3, a.position.y, a.position.z};
        for (int i = 0; i < 400; ++i) {
            p = WorldCollision::resolve_body_movement(p, {p.x - .05F, p.y, p.z}, p.y, region, .45F);
            p.y = WorldCollision::ground_height(p, p.y, region);
        }
        check(p.x > a.position.x + std::min(a.size.x, a.size.z) * .5F - 1, "tower is solid");
    }
    check(towers >= 8, "tower core");

    // Every parked car downtown can be entered and driven along its street.
    game::vehicles::VehicleRegistry vehicle_types; std::string error;
    check(vehicle_types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem cars(vehicle_types);
    cars.set_training_structure(false);
    cars.reconcile(region);
    int downtown_cars = 0;
    for (std::size_t i = 0; i < cars.vehicles().size(); ++i) {
        auto* car = cars.asset(region, cars.vehicles()[i].id);
        if (!car || (car->vehicle.marker.find("downtown") == std::string::npos && car->vehicle.marker.find("capital") == std::string::npos)) continue;
        ++downtown_cars;
        const Vector3 start = car->position;
        const Vector3 beside = Vector3Add(start, Vector3RotateByAxisAngle({2.2F, 0, 0}, {0, 1, 0}, car->rotation_y * DEG2RAD));
        const int found = cars.nearest(region, beside);
        check(found == static_cast<int>(i) && cars.enter(region, found, beside), "get into parked car " + car->vehicle.marker);
        for (int f = 0; f < 120; ++f) { cars.begin_frame(); cars.update(.02F, {1, 0}, region, car->position); }
        check(Vector3Distance(start, car->position) > 8, "drive away: " + car->vehicle.marker + " moved " + std::to_string(Vector3Distance(start, car->position)));
        for (int f = 0; f < 100; ++f) cars.update(.02F, {0, 0, true}, region, car->position);
        Vector3 out{};
        check(cars.exit(region, out), "get out of " + car->vehicle.marker);
    }
    check(downtown_cars >= 12, "parked cars downtown: " + std::to_string(downtown_cars));

    // Explore: downtown is lived in - Main Street homes, shops and pubs, office workers in the towers.
    game::life::LifeSimulation life;
    life.build(region, nullptr);
    // Part-built buildings carry authored purposes (markers inside the front door), so island life
    // houses and employs people in them; the works and harbour offices are offices.
    {
        int kit_places = 0, offices = 0, docks = 0, staffed = 0;
        for (std::size_t i = 0; i < life.places().size(); ++i) {
            const auto& place = life.places()[i];
            if (!place.building_id.starts_with("creator_marker_building_purpose_kit_")) continue;
            ++kit_places;
            offices += place.kind == game::life::PlaceKind::Office;
            docks += place.kind == game::life::PlaceKind::Dock;
            bool used = false;
            for (const auto& r : life.residents()) if (r.work == static_cast<int>(i) || r.home == static_cast<int>(i)) {used = true; break;}
            staffed += used;
            if (!used) std::cerr << "  unused " << place.id << " kind " << game::life::place_name(place.kind) << " town " << place.settlement
                                 << " homes " << place.home_capacity << " jobs " << place.job_capacity << '\n';
        }
        check(report.purposes >= 10 && kit_places == report.purposes, "every part-built building is a place: " + std::to_string(kit_places) + "/" + std::to_string(report.purposes));
        check(offices >= 2 && docks >= 1, "authored offices and dock");
        check(staffed == kit_places, "people live or work in every part-built building: " + std::to_string(staffed));
    }
    int capital_homes = 0, offices = 0, office_workers = 0;
    for (const auto& place : life.places())
        if (life.island().settlement_names[static_cast<std::size_t>(place.settlement)] == "Verda") {
            capital_homes += place.home_capacity > 0;
            offices += place.sealed;
        }
    for (const auto& r : life.residents())
        if (r.work >= 0 && life.places()[static_cast<std::size_t>(r.work)].sealed) ++office_workers;
    check(capital_homes >= 20 && offices >= 8 && office_workers >= 20,
          "downtown residents: homes " + std::to_string(capital_homes) + " offices " + std::to_string(offices) + " office workers " + std::to_string(office_workers));
    // Office workers stay abstract while inside a tower even with the player at the door.
    game::life::LifeConfig config; config.time_scale = 60; life.configure(config);
    characters::NpcSystem npcs;
    life.reset(npcs, game::life::WorldClock::at(0, 10, 0));
    for (int i = 0; i < 40; ++i) life.update(.25F, {0, 0, 9000}, npcs);
    for (const auto& r : life.residents()) {
        if (r.work < 0 || !life.places()[static_cast<std::size_t>(r.work)].sealed || r.place != r.work || !life.indoors(r)) continue;
        life.update(.3F, life.places()[static_cast<std::size_t>(r.work)].door, npcs);
        check(npcs.find_resident(r.id) < 0, "nobody materialises inside a solid tower");
        break;
    }
    std::cout << "world kit tests passed\n";
}
