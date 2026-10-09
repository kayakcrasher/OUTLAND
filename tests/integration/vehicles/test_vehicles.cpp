#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/combat/WeaponSystem.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <raymath.h>
using namespace outland;
using namespace game::vehicles;
using namespace game::combat;
namespace fs = std::filesystem;
int main() {
    const std::string root = OUTLAND_SOURCE_DIR;
    VehicleRegistry registry;
    std::string error;
    assert(registry.load(root + "/assets/verda/vehicles/vehicle_manifest.tsv", error));
    const auto invalid = fs::temp_directory_path() / "outland_bad_vehicle.tsv";
    {
        std::ifstream in(root + "/assets/verda/vehicles/vehicle_manifest.tsv");
        std::string data((std::istreambuf_iterator<char>(in)), {});
        auto at = data.find("\t4.2\t");
        assert(at != std::string::npos);
        data.replace(at, 5, "\tnan\t");
        std::ofstream out(invalid);
        out << data;
    }
    assert(!registry.load(invalid.string(), error));
    fs::remove(invalid);
    const auto *def = registry.find("hatchback");
    assert(def && !def->wheel.empty() && def->length == 4.2F && !registry.find("missing"));
    const auto anchors = def->anchors();
    assert(anchors[0].x < 0 && anchors[1].x > 0 && anchors[0].z > anchors[2].z &&
           anchors[0].y == def->wheel_radius);
    // Normalization uses real CPU mesh bounds, without loading a GPU model.
    float vertices[]{-1, 0, -4, 1, 2, 4};
    Mesh mesh{};
    mesh.vertexCount = 2;
    mesh.vertices = vertices;
    Model model{};
    model.transform = MatrixIdentity();
    model.meshCount = 1;
    model.meshes = &mesh;
    assert(VehicleRegistry::normalize(model, 4.2F));
    auto bounds = GetModelBoundingBox(model);
    assert(std::abs(bounds.max.z - bounds.min.z - 4.2F) < .001F);
    assert(!VehicleRegistry::normalize(model, 0));
    auto region = world::VerdaRegion{};
    VehicleSystem cars(registry);
    assert(cars.reconcile(region));
    assert(cars.vehicles().size() == 1);
    assert(!cars.reconcile(region));
    auto *car = cars.asset(region, cars.vehicles()[0].id);
    assert(car && car->vehicle.marker == "vehicle_spawn_hatchback_capital");
    // Move onto the open flat capital pad, retaining the marker's authored home.
    car->position = {25, 0, 25};
    car->rotation_y = 0;
    cars.reconcile(region);
    assert(cars.nearest(region, {25, 0, 27}) == 0);
    assert(cars.nearest(region, {100, 0, 100}) == -1);
    assert(!cars.enter(region, -1, {25, 0, 27}));
    assert(!cars.enter(region, 0, {100, 0, 100}));
    assert(cars.enter(region, 0, {25, 0, 27}));
    assert(!cars.enter(region, 0, {25, 0, 27}));
    assert(cars.events().started && cars.events().door);
    assert(std::abs(cars.seat(region).y - def->seat.y) < .001F);
    for (int i = 0; i < 50; ++i) {
        cars.begin_frame();
        cars.update(.02F, {1, 0}, region, {25, 0, 25});
    }
    assert(cars.driver()->speed > 4 && car->position.z > 27 &&
           std::abs(cars.driver()->wheel_angle) > 1);
    Vector3 exit{};
    assert(!cars.exit(region, exit));
    float speed = cars.driver()->speed;
    for (int i = 0; i < 10; ++i)
        cars.update(.02F, {0, 0, true}, region, car->position);
    assert(cars.driver()->speed < speed);
    for (int i = 0; i < 50; ++i)
        cars.update(.02F, {-1, .6F}, region, car->position);
    assert(cars.driver()->speed < 0 && cars.driver()->steering > 0 &&
           std::abs(cars.driver()->yaw) > .1F);
    for (int i = 0; i < 50; ++i)
        cars.update(.02F, {0, 0, true}, region, car->position);
    assert(std::abs(cars.driver()->speed) < .01F);
    assert(cars.exit(region, exit));
    assert(!cars.driver());
    assert(!world::physics::WorldCollision::blocked(exit, region, .45F));
    cars.update(.02F, {}, region, {1500, 0, 1500});
    assert(cars.vehicles()[0].sleeping);
    // Body blocks bullets; logical side glass allows a continued shot into the driver.
    car->position = {25, 0, 25};
    car->rotation_y = 0;
    cars.reconcile(region);
    assert(cars.enter(region, 0, {25, 0, 27}));
    auto hit = cars.trace(region, {22, .6F, 25}, {28, .6F, 25});
    assert(hit.kind == HitKind::Vehicle && hit.penetration == 0 &&
           hit.zone == static_cast<int>(VehicleZone::Body));
    hit = cars.trace(region, {22, 1.1F, 25.1F}, {28, 1.1F, 25.1F});
    assert(hit.kind == HitKind::VehicleWindow && hit.penetration > .5F);
    auto second = cars.trace(region, Vector3Add(hit.position, {.04F, 0, 0}), {28, 1.1F, 25.1F});
    assert(second.kind == HitKind::VehicleOccupant);
    float occupant_damage = 0;
    cars.bind_occupant_damage([&](float amount) { occupant_damage += amount; });
    CombatWorld combat(region);
    combat.bind_vehicles(
        [&](Vector3 a, Vector3 b, bool occupants) { return cars.trace(region, a, b, occupants); },
        [&](const BulletHit &h, float amount) { cars.damage(region, h, amount); });
    WeaponSystem weapons;
    weapons.reset(true);
    weapons.update(.02F, {.fire = true}, {{22, 1.1F, 25.1F}, {1, 0, 0}}, combat);
    assert(occupant_damage > 0 && occupant_damage < weapons.weapon().damage);
    assert(!combat.trace_segment({22, 1.1F, 25.1F}, {28, 1.1F, 25.1F}, false, false, false).hit());
    auto engine = cars.trace(region, {25, .7F, 29}, {25, .7F, 26});
    assert(engine.zone == static_cast<int>(VehicleZone::Engine));
    cars.damage(region, engine, 20);
    assert(car->vehicle.engine == 70 && car->vehicle.health == 80);
    auto tire = cars.trace(region, {23, .2F, 25 + def->front_z}, {25, .2F, 25 + def->front_z});
    assert(tire.zone == static_cast<int>(VehicleZone::Tire0));
    cars.damage(region, tire, 100);
    assert(car->vehicle.tires[0] == 0);
    cars.bind_fuel([](const std::string &, float) { return .1F; });
    cars.update(.02F, {1, 0}, region, car->position);
    assert(car->vehicle.fuel < 1);
    car->vehicle.fuel = 0;
    for (int i = 0; i < 20; ++i)
        cars.update(.02F, {1, 0}, region, car->position);
    assert(cars.driver()->speed == 0);
    cars.damage(region, engine, 1000);
    assert(car->vehicle.destroyed && car->vehicle.engine == 0);
    cars.leave_session();
    assert(!cars.enter(region, 0, car->position));
    // Companion runtime saves are transactional and stale authored anchors cannot be overridden.
    const auto dir = fs::temp_directory_path() / "outland_vehicle_runtime_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const auto state = (dir / "map.vehicles").string();
    car->position.x = 33;
    assert(cars.save_state(region, state, error));
    car->position.x = 25;
    car->vehicle.health = 100;
    assert(cars.load_state(region, state, error));
    assert(car->position.x == 33 && car->vehicle.destroyed);
    car->vehicle.home.x += 1;
    for (auto &site : region.runtime_settlements())
        for (auto &marker : site.gameplay_markers)
            if (marker.id == car->vehicle.marker)
                marker.position.x = car->vehicle.home.x;
    car->position.x = 44;
    assert(cars.load_state(region, state, error));
    assert(car->position.x == 44);
    {
        std::ofstream out(dir / "bad");
        out << "OUTLAND_VEHICLES 1\nVEHICLE truncated\n";
    }
    assert(!cars.load_state(region, (dir / "bad").string(), error));
    assert(car->position.x == 44);
    assert(cars.blocked(region, {2700, -40, 0}, 0, *def, 1));
    assert(cars.blocked(region, {0, 0, 0}, 0, *def, 1));
    // Deleting a marker-derived car disables its source instead of respawning it.
    const auto id = car->id;
#ifdef OUTLAND_DEV_TOOLS
    assert(region.delete_world_asset(id));
#else
    for (auto &site : region.runtime_settlements())
        for (auto &marker : site.gameplay_markers)
            marker.enabled = false;
    for (auto &site : region.runtime_settlements())
        std::erase_if(site.assets, [&](const auto &a) { return a.id == id; });
#endif
    cars.reconcile(region);
    assert(cars.vehicles().empty());
    assert(!cars.reconcile(region));
    assert(cars.spawn(region, {25, 0, 25}, 0));
    assert(cars.vehicles().size() == 1);
    assert(cars.asset(region, cars.vehicles()[0].id)->vehicle.marker.empty());
#ifdef OUTLAND_DEV_TOOLS
    const auto map = (dir / "world.map").string();
    auto *placed = cars.asset(region, cars.vehicles()[0].id);
    placed->vehicle.health = 62;
    placed->vehicle.tires[2] = 33;
    assert(creator::CreatorMapIO::save(region, map));
    world::VerdaRegion restored;
    assert(creator::CreatorMapIO::load(restored, map));
    VehicleSystem saved(registry);
    saved.reconcile(restored);
    assert(saved.vehicles().size() == 1);
    const auto *roundtrip = saved.asset(restored, saved.vehicles()[0].id);
    assert(roundtrip->vehicle.health == 62 && roundtrip->vehicle.tires[2] == 33 &&
           roundtrip->vehicle.definition == "hatchback");
#endif
    world::VerdaRegion bounded;
    for (auto &site : bounded.runtime_settlements())
        site.gameplay_markers.clear();
    for (int i = 0; i < 140; ++i)
        bounded.runtime_settlements().front().gameplay_markers.push_back(
            {"vehicle_spawn_hatchback_" + std::to_string(i),
             world::GameplayMarkerType::VehicleSpawn,
             {25.0F + i * 5, 0, 25},
             {3, 1.8F, 5},
             0,
             true});
    VehicleSystem fleet(registry);
    fleet.reconcile(bounded);
    assert(fleet.vehicles().size() == 128);
    assert(!fleet.spawn(bounded, {25, 0, 25}, 0));
    assert(!fleet.reconcile(bounded));
    fs::remove_all(dir);
    std::cout << "[PASS] Vehicle definitions, scale, anchors, marker reconciliation, driving, "
                 "exit, sleep, cover, glass penetration, damage, fuel hook and persistence\n";
}
