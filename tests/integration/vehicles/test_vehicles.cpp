#include <cstdio>
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/combat/WeaponSystem.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <algorithm>
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
    // Reach is measured to the car body: standing at the bumper 1.5 m away is close enough.
    car->position = {25, 0, 25};
    car->rotation_y = 0;
    cars.reconcile(region);
    assert(cars.nearest(region, {25, 0, 25 + def->length * .5F + 1.5F}) == 0);
    assert(cars.nearest(region, {25, 0, 25 + def->length * .5F + 3}) == -1);
    // A car placed overlapping something (a post landed on its spawn) can still drive out.
    {
        world::WorldAsset post;
        post.id = "test_post"; post.model_path = "assets/missing_post.glb"; post.collision = true;
        post.position = {25, 0, 25.3F}; post.size = {.6F, 3, .6F};
        region.runtime_settlements().front().assets.push_back(post);
        assert(cars.enter(region, 0, {23.5F, 0, 25}));
        assert(world::physics::WorldCollision::blocked(car->position, region, .2F, car->id));
        const float start = car->position.z;
        for (int i = 0; i < 60; ++i) cars.update(.02F, {-1, 0}, region, car->position);
        assert(car->position.z < start - 1);
        for (int i = 0; i < 60; ++i) cars.update(.02F, {0, 0, true}, region, car->position);
        assert(cars.exit(region, exit));
        assert(car->vehicle.health == 100); // escaping is not a crash
        auto& assets = region.runtime_settlements().front().assets;
        assets.erase(std::remove_if(assets.begin(), assets.end(), [](const auto& a) { return a.id == "test_post"; }), assets.end());
    }
    // Battle-royale handling: the handbrake throws the tail out (sideways slip), boost pulls
    // harder, and a crash costs body health in proportion to the impact.
    {
        const auto run = [&](bool boost, bool handbrake) {
            cars.leave_session();
            car->position = {25, 0, 25}; car->rotation_y = 0; car->vehicle.health = 100;
            cars.reconcile(region);
            assert(cars.enter(region, 0, {25, 0, 27}));
            for (int i = 0; i < 100; ++i) cars.update(.02F, {1, 0, false, false, false, boost}, region, car->position);
            const float speed = cars.driver()->speed;
            float slip = 0;
            for (int i = 0; i < 20; ++i) {
                cars.update(.02F, {0, 1, handbrake}, region, car->position);
                slip = std::max(slip, std::abs(cars.driver()->slip));
            }
            return std::pair{speed, slip};
        };
        const auto plain = run(false, false), boosted = run(true, false), drift = run(false, true);
        std::fprintf(stderr, "handling: plain %.2f/%.2f boosted %.2f drift %.2f/%.2f\n", plain.first, plain.second, boosted.first, drift.first, drift.second);
        assert(boosted.first > plain.first * 1.2F);
        assert(drift.second > plain.second * 2 && drift.second > 1);
        cars.leave_session();
        world::WorldAsset wall;
        wall.id = "test_wall"; wall.model_path = "assets/missing_wall.glb"; wall.collision = true;
        wall.position = {25, 0, 45}; wall.size = {8, 3, 1};
        region.runtime_settlements().front().assets.push_back(wall);
        car->position = {25, 0, 25}; car->rotation_y = 0; car->vehicle.health = 100;
        cars.reconcile(region);
        assert(cars.enter(region, 0, {25, 0, 27}));
        for (int i = 0; i < 200 && car->vehicle.health == 100; ++i) cars.update(.02F, {1, 0}, region, car->position);
        assert(car->vehicle.health < 100 && cars.driver()->speed <= 0 && car->position.z < 45);
        cars.leave_session();
        // A parking bump (well under 25 km/h) costs nothing, and the car is not left pinned.
        car->position = {25, 0, 40}; car->rotation_y = 0; car->vehicle.health = 100;
        cars.reconcile(region);
        assert(cars.enter(region, 0, {25, 0, 42}));
        for (int i = 0; i < 60; ++i) cars.update(.02F, {.5F, 0}, region, car->position);
        assert(car->vehicle.health == 100 && car->position.z < 45);
        for (int i = 0; i < 60; ++i) cars.update(.02F, {-1, 0}, region, car->position);
        assert(car->position.z < 39.5F); // backs straight off the wall
        cars.leave_session();
        auto& assets = region.runtime_settlements().front().assets;
        assets.erase(std::remove_if(assets.begin(), assets.end(), [](const auto& a) { return a.id == "test_wall"; }), assets.end());
        car->vehicle.health = 100;
        // Light street furniture (a picket fence) is brushed aside, not crashed into.
        world::WorldAsset fence;
        fence.id = "test_fence"; fence.model_path = "assets/verda/urban/Walls & Fences/white_picket_fence/white_picket_fence_closed_left.glb";
        fence.collision = true; fence.position = {25, 0, 35}; fence.size = {2.4F, 1.37F, .26F};
        assets.push_back(fence);
        assert(world::physics::WorldCollision::light_prop(fence));
        car->position = {25, 0, 25}; car->rotation_y = 0;
        cars.reconcile(region);
        assert(cars.enter(region, 0, {25, 0, 27}));
        for (int i = 0; i < 150; ++i) cars.update(.02F, {1, 0}, region, car->position);
        assert(car->position.z > 40 && car->vehicle.health == 100);
        cars.leave_session();
        assets.erase(std::remove_if(assets.begin(), assets.end(), [](const auto& a) { return a.id == "test_fence"; }), assets.end());
    }
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
    // Rounds mostly pass through the bodywork: 20 damage takes 7% of a 100-health hatchback.
    assert(car->vehicle.engine == 70 && std::abs(car->vehicle.health - 93) < .01F);
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
    // A wreck is towed home and repaired once the player has been far away for a while.
    {
        const Vector3 parked = car->position;
        car->vehicle.destroyed = true;
        car->vehicle.health = car->vehicle.engine = 0;
        const Vector3 far{car->position.x + 600, 0, car->position.z};
        for (int i = 0; i < 850; ++i) cars.update(.1F, {}, region, far); // 85 s
        assert(car->vehicle.destroyed);
        for (int i = 0; i < 60; ++i) cars.update(.1F, {}, region, far);
        assert(!car->vehicle.destroyed && car->vehicle.health == 100 && car->vehicle.engine == 100);
        assert(car->position.x == car->vehicle.home.x && car->position.z == car->vehicle.home.z);
        // Not while the player stands by it.
        car->vehicle.destroyed = true;
        for (int i = 0; i < 1200; ++i) cars.update(.1F, {}, region, car->position);
        assert(car->vehicle.destroyed);
        car->vehicle.destroyed = false;
        car->vehicle.health = car->vehicle.engine = 100;
        car->position = parked;
    }
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
    // Battle Royale uses healthy vehicles without changing Explore/Creator placement or damage.
    auto* persistent=cars.asset(region,cars.vehicles()[0].id);
    persistent->vehicle.health=42;persistent->vehicle.engine=31;persistent->vehicle.tires[0]=20;
    persistent->vehicle.destroyed=true;persistent->position.x+=5;
    cars.reconcile(region);
    const auto checkpoint=cars.checkpoint(region);
    const auto vehicle_id=persistent->id;
    const auto position=persistent->position;
    cars.reset_match_fleet(region);
    const auto* match_car=cars.asset(region,vehicle_id);
    assert(match_car->vehicle.health==100 && match_car->vehicle.engine==100 && !match_car->vehicle.destroyed);
    assert(match_car->vehicle.tires[0]==100);
    cars.restore_checkpoint(region,checkpoint);
    const auto* original=cars.asset(region,vehicle_id);
    assert(original->vehicle.health==42 && original->vehicle.engine==31 && original->vehicle.destroyed);
    assert(original->vehicle.tires[0]==20 && original->position.x==position.x);
    assert(cars.vehicles()[0].position.x==position.x && !cars.driver());
    world::VerdaRegion bounded;
    for (auto &site : bounded.runtime_settlements())
        site.gameplay_markers.clear();
    for (int i = 0; i < static_cast<int>(VehicleSystem::max_vehicles) + 16; ++i)
        bounded.runtime_settlements().front().gameplay_markers.push_back(
            {"vehicle_spawn_hatchback_" + std::to_string(i),
             world::GameplayMarkerType::VehicleSpawn,
             {25.0F + i * 5, 0, 25},
             {3, 1.8F, 5},
             0,
             true});
    VehicleSystem fleet(registry);
    fleet.reconcile(bounded);
    assert(fleet.vehicles().size() == VehicleSystem::max_vehicles);
    assert(!fleet.spawn(bounded, {25, 0, 25}, 0));
    assert(!fleet.reconcile(bounded));
    fs::remove_all(dir);
    std::cout << "[PASS] Vehicle definitions, scale, anchors, marker reconciliation, driving, "
                 "exit, sleep, cover, glass penetration, damage, fuel hook and persistence\n";
}
