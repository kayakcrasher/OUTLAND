// Parkumejo Centra on the published map: cars ride its decks and ramps (VehicleSystem::bind_surface,
// as the game binds it), the cars parked on its levels stand on their decks, a car driven in at the
// entrance climbs the first ramp to deck 1, one left in the mansion's bay stands on the roof; and a
// rig drives into Verda Transporto's shed through its door (the shed is no longer one solid box).
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/economy/SkyMansion.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/MotorTrade.hpp"
#include "outland/world/ParkingGarage.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <raymath.h>

using namespace outland;
namespace garage = world::garage;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    garage::Site site;
    check(garage::find(region, site), "Parkumejo Centra is on the map");
    check(std::abs(site.origin.x + 129) < 2 && std::abs(site.origin.z + 126) < 2, "in downtown's south-west corner block");
    std::string error;
    game::vehicles::VehicleRegistry types;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem cars(types);
    cars.set_training_structure(false);
    cars.bind_surface([&](Vector3 p, float from) {
        if (!site.over(p, 1.5F)) return std::numeric_limits<float>::quiet_NaN();
        return world::physics::WorldCollision::ground_height({p.x, from, p.z}, from, region, 1.0F);
    });
    cars.reconcile(region);
    const auto level = [&](float y) { return y - site.origin.y; };

    // The cars parked on its levels stand on their decks.
    int parked = 0;
    for (const auto& v : cars.vehicles()) {
        const auto* a = cars.asset(region, v.id);
        if (!a || a->vehicle.marker.find("_garage_") == std::string::npos) continue;
        const int deck = a->vehicle.marker[a->vehicle.marker.find("_garage_") + 8] - '0';
        check(std::abs(level(a->position.y) - garage::decks[static_cast<std::size_t>(deck)]) < .15F,
              a->vehicle.marker + " on deck " + std::to_string(deck) + " (at " + std::to_string(level(a->position.y)) + ")");
        ++parked;
    }
    check(parked >= 6, "cars parked on every level");

    // In at the entrance, straight up the first ramp to deck 1 (the ramp climbs local +z in lane A).
    check(cars.spawn(region, site.to_world({15.2F, 0, -21}), site.yaw, "hatchback"), "a car at the entrance");
    const auto car = cars.last_spawned();
    check(cars.place(region, car, site.to_world({15.2F, 0, -21}), site.yaw), "placed at the entrance");
    game::vehicles::VehicleInput drive;
    drive.throttle = .55F;
    cars.set_autopilot(car, drive);
    float top = -100, furthest = -100;
    for (int i = 0; i < 600; ++i) {
        cars.update(1.0F / 60, {}, region, site.origin);
        const auto& v = cars.vehicles()[static_cast<std::size_t>(cars.find(car))];
        const auto local = site.to_local(v.position);
        top = std::max(top, level(v.position.y));
        furthest = std::max(furthest, local.z);
        if (local.z > 11) break;
    }
    const auto& climbed = cars.vehicles()[static_cast<std::size_t>(cars.find(car))];
    std::cout << "up the ramp: reached local z " << furthest << ", height " << top << " m\n";
    check(furthest > 9.5F, "through the entrance and up the ramp");
    check(std::abs(level(climbed.position.y) - garage::decks[1]) < .2F, "on deck 1 at the top (" + std::to_string(level(climbed.position.y)) + ")");
    cars.clear_autopilot(car);

    // All the way up: a car and a pickup, driven at a parking pace, climb lane A to deck 1, swing
    // round the north aisle into lane B, climb to deck 2, round the south aisle and up lane A to
    // the roof - no lamp, pylon or open ramp edge in the way. (A pickup swings wider.)
    for (const auto& [kind, radius] : {std::pair{"hatchback", 3.75F}, std::pair{"truck", 4.6F}}) {
        check(cars.spawn(region, site.to_world({15.2F, 0, -21}), site.yaw, kind), std::string("a ") + kind + " at the entrance");
        const auto who = cars.last_spawned();
        check(cars.place(region, who, site.to_world({15.2F, 0, -21}), site.yaw), "placed");
        const float cx = 11.7F;
        std::vector<Vector3> route{{15.2F, 0, -8}, {cx + radius, 0, 6.5F}};
        for (int a = 1; a <= 8; ++a) route.push_back({cx + radius * std::cos(a * PI / 8), 0, 6.5F + radius * std::sin(a * PI / 8)});
        route.push_back({cx - radius, 0, -6.5F});
        for (int a = 1; a <= 8; ++a) route.push_back({cx - radius * std::cos(a * PI / 8), 0, -6.5F - radius * std::sin(a * PI / 8)});
        route.push_back({15.2F, 0, 8.5F});
        std::size_t next = 0;
        float stalled = 0;
        int backing = 0;
        for (int f = 0; f < 60 * 120 && next < route.size(); ++f) {
            const auto& v = cars.vehicles()[static_cast<std::size_t>(cars.find(who))];
            const auto local = site.to_local(v.position);
            const float dx = route[next].x - local.x, dz = route[next].z - local.z;
            if (std::hypot(dx, dz) < 1.5F) { ++next; continue; }
            const float err = std::remainder(std::atan2(dx, dz) - (v.yaw - site.yaw) * DEG2RAD, 2 * PI);
            game::vehicles::VehicleInput in;
            in.steer = std::clamp(err / .6F, -1.0F, 1.0F);
            const bool turning = (next >= 2 && next <= 10) || (next >= 12 && next <= 19) || std::abs(err) > .35F;
            in.throttle = std::clamp(((turning ? 2.5F : 6.0F) - v.speed) * .6F, v.speed < .5F ? 0.0F : -1.0F, 1.0F);
            if (backing > 0) { in.throttle = -.5F; in.steer = -in.steer; --backing; }
            else if ((stalled = std::abs(v.speed) < .3F ? stalled + 1.0F / 60 : 0) > 1) { stalled = 0; backing = 70; }
            cars.set_autopilot(who, in);
            cars.update(1.0F / 60, {}, region, site.origin);
        }
        const auto& up = cars.vehicles()[static_cast<std::size_t>(cars.find(who))];
        std::cout << kind << ": " << next << " of " << route.size() << " waypoints, at height " << level(up.position.y) << " m\n";
        check(next == route.size() && std::abs(level(up.position.y) - garage::roof) < .2F, std::string("a ") + kind + " drives all the way up to the roof");
        cars.clear_autopilot(who);
        check(cars.remove(region, who), "and goes");
    }

    // Left in the mansion's bay: on the roof (a car that was saved up there comes back up there).
    const Vector3 bay = site.to_world({(garage::bay_x0 + garage::bay_x1) * .5F, garage::roof, (garage::bay_z0 + garage::bay_z1) * .5F});
    check(cars.place(region, car, bay, site.yaw + 180), "parked in the mansion's bay");
    const auto& home = cars.vehicles()[static_cast<std::size_t>(cars.find(car))];
    check(std::abs(level(home.position.y) - garage::roof) < .15F && site.in_bay(home.position), "on the roof, in the bay");
    for (int i = 0; i < 60; ++i) cars.update(1.0F / 60, {}, region, site.origin);
    check(std::abs(level(home.position.y) - garage::roof) < .15F, "and it stays up there");
    // Outside the garage, nothing changes: the terrain.
    const Vector3 street{site.origin.x, 0, site.origin.z + 60};
    check(std::abs(cars.ground(street, 50) - cars.ground(street, 0)) < .01F, "elsewhere the surface is the terrain");

    // The mansion and its bay: for sale, then the owner's.
    check(site.in_mansion(site.to_world(garage::wake)) && site.in_mansion(site.to_world({-10, garage::roof, 2})), "the mansion's floors");
    check(!site.in_mansion(site.to_world({0, garage::roof, -10})), "the roof outside isn't the mansion");
    game::economy::EconomyState wallet;
    wallet.money = 1000;
    check(game::economy::SkyMansion::buy(wallet).find("costs") != std::string::npos && !game::economy::SkyMansion::owned(wallet), "too dear");
    wallet.money = game::economy::SkyMansion::price + 5;
    game::economy::SkyMansion::buy(wallet);
    check(game::economy::SkyMansion::owned(wallet) && wallet.money == 5, "bought");

    // Verda Transporto's shed: a rig drives in through a door to its service bay.
    world::motor::Site depot;
    bool found = false;
    for (const auto& s : world::motor::sites(region)) if (s.kind == world::motor::Kind::Depot) { depot = s; found = true; }
    check(found, "the depot");
    const float door_x = world::motor::depot_bays[0];
    check(cars.spawn(region, depot.to_world({door_x, 0, -22}), depot.yaw, "semi"), "a rig at the depot");
    const auto rig = cars.last_spawned();
    check(cars.place(region, rig, depot.to_world({door_x, 0, -22}), depot.yaw), "lined up with the door");
    drive.throttle = .5F;
    cars.set_autopilot(rig, drive);
    for (int i = 0; i < 900; ++i) {
        cars.update(1.0F / 60, {}, region, depot.origin);
        if (world::motor::bay_at(depot, cars.vehicles()[static_cast<std::size_t>(cars.find(rig))].position) >= 0) break;
    }
    const auto in = depot.to_local(cars.vehicles()[static_cast<std::size_t>(cars.find(rig))].position);
    std::cout << "the rig drove to local z " << in.z << " in the shed\n";
    check(world::motor::bay_at(depot, cars.vehicles()[static_cast<std::size_t>(cars.find(rig))].position) >= 0, "into the depot's service bay");
    std::cout << "parking garage tests passed\n";
    return 0;
}
