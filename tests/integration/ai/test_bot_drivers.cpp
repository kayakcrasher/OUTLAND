// Battle Royale bots on the road: some take parked cars at the start of a match, drive the roads
// in toward the zone, and get out when the player is close and in sight.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/ZoneWall.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace outland;

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
    std::string error;
    game::vehicles::VehicleRegistry types;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem cars(types);
    cars.reconcile(region);

    game::ai::BattleRoyaleBots bots;
    const Vector3 far_away{0, 0, 9000};
    bots.start(region, game::ai::BotLevel::Medium, 23, 41, far_away);
    game::ai::BotDrivers drivers;
    const int driving = drivers.start(bots, cars, region, 41, 2450);
    std::cout << driving << " of " << bots.bots().size() << " bots took a car\n";
    check(driving >= 3, "several bots drive");
    for (const auto& ride : drivers.rides()) {
        check(bots.mounted(ride.bot) && drivers.driver_of(ride.vehicle) == ride.bot, "a driver is mounted in its car");
        check(ride.route.complete && ride.route.length > 80, "a route in toward the zone");
    }
    // The goal is well inside the circle on the line to the centre.
    const Vector3 goal = game::ai::BotDrivers::zone_goal({2000, 0, 0}, 2450);
    check(std::abs(goal.z) < 1 && goal.x > 0 && goal.x < 2450 * .4F, "the goal is inside the zone");
    check(game::zone_wall_segments(2450) == 1282 && game::zone_wall_segments(25) == 64, "zone wall detail");

    // A minute on the road: the cars move along their routes and carry their drivers.
    std::vector<Vector3> start;
    for (const auto& ride : drivers.rides()) start.push_back(cars.vehicles()[static_cast<std::size_t>(cars.find(ride.vehicle))].position);
    const auto sight = [](Vector3, Vector3) { return true; };
    for (float t = 0; t < 60; t += .05F) {
        drivers.update(.05F, bots, cars, region, far_away, false, 2450, sight);
        cars.update(.05F, {}, region, far_away, false);
    }
    float furthest = 0;
    for (std::size_t i = 0; i < drivers.rides().size(); ++i) {
        const auto& ride = drivers.rides()[i];
        const auto& car = cars.vehicles()[static_cast<std::size_t>(cars.find(ride.vehicle))];
        furthest = std::max(furthest, Vector3Distance(start[i], car.position));
        if (ride.active) {
            const auto* bot = bots.find(ride.bot);
            check(bot && Vector3Distance(bot->position(), car.position) < 2, "the driver rides with the car"); // one physics step behind
        }
    }
    std::cout << "furthest drive in a minute: " << furthest << " m, still driving: " << drivers.driving() << '\n';
    check(furthest > 150, "bots drive the roads: " + std::to_string(furthest));

    // The player steps out beside a driving car: its driver gets out to fight.
    for (const auto& ride : drivers.rides()) {
        if (!ride.active) continue;
        const auto& car = cars.vehicles()[static_cast<std::size_t>(cars.find(ride.vehicle))];
        const Vector3 player{car.position.x + 25, car.position.y, car.position.z};
        const int bot = ride.bot;
        drivers.update(.05F, bots, cars, region, player, true, 2450, sight);
        check(!bots.mounted(bot) && drivers.driver_of(ride.vehicle) < 0, "the driver gets out when the player is close");
        const auto* out = bots.find(bot);
        check(out && out->alive() && Vector3Distance(out->position(), car.position) < 5, "out beside the car");
        break;
    }
    drivers.clear(bots, cars);
    check(drivers.driving() == 0, "everyone out at the end of the match");
    std::cout << "bot driver tests passed\n";
    return 0;
}
