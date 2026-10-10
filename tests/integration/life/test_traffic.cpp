#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/law/Justice.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/navigation/NavGrid.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <iostream>
using namespace outland;
using namespace outland::game::life;
namespace roads = outland::world::roads;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float to_road(const world::VerdaRegion& region, Vector3 p) {
    float best = 1e9F;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
        const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
        const float t = l2 < 1e-6F ? 0 : std::clamp(((p.x - road.start.x) * dx + (p.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
        best = std::min(best, std::hypot(p.x - road.start.x - dx * t, p.z - road.start.z - dz * t) - road.width * .5F);
    }
    return best;
}
struct World {
    world::VerdaRegion& region;
    characters::NpcSystem& npcs;
    game::vehicles::VehicleSystem& vehicles;
    LifeSimulation& life;
    // Steps everything like the game loop; returns true as soon as `until` holds.
    bool run(Vector3 player, float seconds, const std::function<bool()>& until = {}, const std::function<void()>& each = {}) {
        characters::NpcContext context; context.player_position = player;
        for (float t = 0; t < seconds; t += .05F) {
            npcs.update(.05F, context, region);
            vehicles.update(.05F, {}, region, player);
            life.update(.05F, player, npcs);
            if (each) each();
            if (until && until()) return true;
        }
        return false;
    }
};
}

int main() {
    using game::law::CrimeKind;
    check(game::law::crime_severity(CrimeKind::CarTheft) == 1 && std::string(game::law::crime_name(CrimeKind::CarTheft)) == "car theft", "car theft is a one-star crime");

    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);

    // The road network joins every town, crossings included, and routes keep to it.
    roads::RoadGraph graph(region);
    check(graph.nodes().size() > 60 && graph.edges().size() > 70, "road graph: " + std::to_string(graph.nodes().size()) + " nodes");
    for (const auto& town : region.settlements()) {
        const auto route = graph.route({3, 0, -30}, town.center);
        check(route.complete, "a road from downtown to " + town.name);
        check(route.length < flat({3, 0, -30}, town.center) * 1.7F + 60, "the route to " + town.name + " is direct enough: " + std::to_string(route.length));
        for (std::size_t i = 1; i + 1 < route.points.size(); ++i) check(to_road(region, route.points[i]) < .6F, "route stays on roads to " + town.name);
    }
    check(graph.route({-1900, 0, 612}, {0, 0, 0}).complete, "the trailer park's lane joins the island's roads");
    // Downtown: across the grid, turning at a crossing, keeping right.
    const auto across = graph.route({-120, 0, -86}, {86, 0, 40});
    check(across.complete && across.length < 345, "downtown route: " + std::to_string(across.length));
    bool corner = false;
    for (const auto& p : across.points) corner = corner || (std::fmod(std::abs(p.x) + .5F, 80.0F) < 1 && std::fmod(std::abs(p.z) + .5F, 80.0F) < 1);
    check(corner, "turns at a grid crossing");
    {
        // Eastbound on a z = const street, the right-hand lane is on the +z side.
        const auto east = graph.route({-150, 0, -80}, {-10, 0, -80});
        float yaw = 0;
        const auto mid = roads::route_point(east, east.length * .5F, &yaw);
        check(std::abs(yaw - 90) < 1 && mid.z > -80 + 1 && mid.z < -80 + 3, "drives on the right: z " + std::to_string(mid.z));
        check(std::abs(roads::route_progress(east, mid, east.length * .4F) - east.length * .5F) < 1, "progress along a route");
    }

    // Households own the cars parked beside them.
    characters::CharacterRegistry characters; std::string error;
    check(characters.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/characters/character_manifest.tsv", error), error);
    game::vehicles::VehicleRegistry catalog;
    check(catalog.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem vehicles(catalog);
    vehicles.set_training_structure(false);
    vehicles.reconcile(region);
    characters::NpcSystem npcs;
    world::navigation::NavGrid nav(region);
    nav.set_max_new_cells(250);
    nav.set_max_expansions(8000);
    npcs.set_path_finder([&](Vector3 from, Vector3 to, world::navigation::NavPath& path) {return nav.find_path(from, to, path);});
    LifeSimulation life;
    life.build(region, &characters);
    const auto& cars = life.island().cars;
    std::cout << cars.size() << " resident cars of " << vehicles.vehicles().size() << " vehicles\n";
    check(cars.size() >= 20, "most cars have owners");
    for (std::size_t i = 0; i < cars.size(); ++i) {
        const auto& owner = life.residents()[static_cast<std::size_t>(cars[i].owner)];
        check(owner.car == static_cast<int>(i) && owner.home == cars[i].home, "owner and car agree");
        check(flat(life.places()[static_cast<std::size_t>(cars[i].home)].door, cars[i].spot) < 70, "parked near home");
        check(vehicles.find(cars[i].vehicle) >= 0, "a real vehicle: " + cars[i].vehicle);
    }
    life.bind_vehicles(&vehicles, &region);
    World world{region, npcs, vehicles, life};

    // Off-screen morning: commuters drive to work on the roads and park there.
    const Vector3 far_away{0, 0, 9000};
    life.reset(npcs, WorldClock::at(0, 5, 30));
    std::vector<bool> drove(life.residents().size(), false);
    float worst_off_road = 0;
    world.run(far_away, 540, {}, [&] {
        for (const auto& r : life.residents()) {
            if (r.trip != Trip::Driving) continue;
            drove[static_cast<std::size_t>(r.id)] = true;
            const auto& car = life.cars()[static_cast<std::size_t>(r.car)];
            const float ends = std::min(car.progress, car.route.length - car.progress);
            if (ends > 25 && to_road(region, car.position) > worst_off_road) {
                worst_off_road = to_road(region, car.position);
                if (std::getenv("OUTLAND_TRAFFIC_DEBUG") && worst_off_road > 1) {
                    std::cout << "  off road " << worst_off_road << " at " << car.position.x << "," << car.position.z << " progress " << car.progress << "/" << car.route.length << " route:";
                    for (const auto& p : car.route.points) std::cout << " (" << p.x << "," << p.z << ")";
                    std::cout << '\n';
                }
            }
        }
    });
    const auto drivers = std::count(drove.begin(), drove.end(), true);
    if (std::getenv("OUTLAND_TRAFFIC_DEBUG"))
        for (std::size_t i = 0; i < cars.size(); ++i) {
            const auto& r = life.residents()[static_cast<std::size_t>(cars[i].owner)];
            std::cout << "  " << cars[i].vehicle << " " << r.full_name() << " " << occupation_name(r.occupation) << " work "
                      << (r.work >= 0 ? flat(life.places()[static_cast<std::size_t>(r.work)].door, cars[i].spot) : -1) << " m, "
                      << activity_name(r.activity) << " drove " << drove[static_cast<std::size_t>(r.id)] << " lost " << life.cars()[i].lost << '\n';
        }
    std::cout << "morning (" << life.clock().label() << "): " << drivers << " residents drove\n";
    check(drivers >= 6, "people drive to work");
    check(worst_off_road < 2, "cars keep to the roads: " + std::to_string(worst_off_road));
    int commuters = 0, parked_at_work = 0;
    for (std::size_t i = 0; i < cars.size(); ++i) {
        const auto& r = life.residents()[static_cast<std::size_t>(cars[i].owner)];
        if (r.work < 0 || r.activity != Activity::Work || life.cars()[i].lost) continue;
        const auto& work = life.places()[static_cast<std::size_t>(r.work)];
        if (flat(work.door, cars[i].spot) < 400) continue;
        ++commuters;
        const int v = vehicles.find(cars[i].vehicle);
        const Vector3 at = vehicles.vehicles()[static_cast<std::size_t>(v)].position;
        parked_at_work += flat(at, work.door) < 60 && flat(at, life.cars()[i].position) < .5F;
    }
    std::cout << parked_at_work << " of " << commuters << " long-distance commuters parked at work\n";
    check(commuters >= 3 && parked_at_work * 10 >= commuters * 8, "commuters' cars are at work");

    // On screen: the real car drives off under autopilot, stops for someone in the road, and
    // goes back to the island clock once out of sight.
    life.reset(npcs, WorldClock::at(0, 5, 0));
    int pick = -1;
    for (std::size_t i = 0; i < cars.size() && pick < 0; ++i) {
        const auto& r = life.residents()[static_cast<std::size_t>(cars[i].owner)];
        if (r.work >= 0 && flat(life.places()[static_cast<std::size_t>(r.work)].door, cars[i].spot) > 600 &&
            graph.nearest(cars[i].spot).distance < 40 && cars[i].vehicle.find("_drive_") != std::string::npos) pick = static_cast<int>(i);
    }
    check(pick >= 0, "a commuter with a car on the drive");
    const auto& chosen = cars[static_cast<std::size_t>(pick)];
    const Vector3 watcher{chosen.spot.x + 12, 0, chosen.spot.z + 12};
    const auto& state = life.cars()[static_cast<std::size_t>(pick)];
    {
        float clock = 0;
        const auto& who = life.residents()[static_cast<std::size_t>(chosen.owner)];
        const bool off = world.run(watcher, 900, [&] {return state.driving && state.physical;}, [&] {
            if (!std::getenv("OUTLAND_TRAFFIC_DEBUG") || (clock += .05F) < 10) return;
            clock = 0;
            std::cout << "  " << life.clock().label() << " " << life.describe(who) << " trip " << static_cast<int>(who.trip) << " physical " << who.physical
                      << " at " << life.position(who).x << "," << life.position(who).z << " car " << state.position.x << "," << state.position.z
                      << " driving " << state.driving;
            if (const int b = npcs.find_resident(who.id); b >= 0) {
                const auto& a = npcs.actors()[static_cast<std::size_t>(b)];
                std::cout << " body state " << static_cast<int>(a.state) << " anchor " << a.spawn_position.x << "," << a.spawn_position.z
                          << " waypoint " << a.waypoint.x << "," << a.waypoint.z << " active " << a.active << " directed " << a.directed
                          << " routing " << a.follower.routing() << " following " << a.follower.following() << " path " << a.follower.path().points.size()
                          << " travelling " << a.travelling << " hurry " << a.hurry;
            }
            std::cout << '\n';
        });
        check(off, "the owner sets off in view");
    }
    check(vehicles.autopiloted(chosen.vehicle), "under autopilot");
    const int v = vehicles.find(chosen.vehicle);
    const auto& runtime = vehicles.vehicles()[static_cast<std::size_t>(v)];
    {
        float clock = 0;
        if (std::getenv("OUTLAND_TRAFFIC_DEBUG")) {
            std::cout << "  route:";
            for (const auto& p : state.route.points) std::cout << " (" << p.x << "," << p.z << ")";
            std::cout << " park " << state.park.x << "," << state.park.z << '\n';
        }
        const bool away = world.run(watcher, 40, [&] {return runtime.speed > 4 && flat(runtime.position, chosen.spot) > 25;}, [&] {
            if (!std::getenv("OUTLAND_TRAFFIC_DEBUG") || (clock += .05F) < 1) return;
            clock = 0;
            std::cout << "  car at " << runtime.position.x << "," << runtime.position.z << " yaw " << runtime.yaw << " speed " << runtime.speed
                      << " progress " << state.progress << "/" << state.route.length << " stuck " << state.stuck << " reverse " << state.reverse
                      << " attempts " << state.attempts << " waiting " << state.waiting << " physical " << state.physical << " driving " << state.driving << '\n';
        });
        check(away, "it pulls away and gets up to speed");
    }
    // Someone steps out in front.
    const Vector3 ahead = roads::route_point(state.route, state.progress + 9);
    world.run(ahead, 5);
    check(std::abs(runtime.speed) < .6F && flat(runtime.position, ahead) > 2, "it stops for a pedestrian");
    // Out of sight it rejoins the island clock and reaches work.
    const auto& owner = life.residents()[static_cast<std::size_t>(chosen.owner)];
    check(world.run(far_away, 120, [&] {return !state.driving;}), "the trip finishes off-screen");
    check(!vehicles.autopiloted(chosen.vehicle) && owner.trip == Trip::Walk, "parked, driver walking in");
    check(flat(vehicles.vehicles()[static_cast<std::size_t>(v)].position, life.places()[static_cast<std::size_t>(owner.work)].door) < 60, "parked by work");

    // Carjacking: pulled out mid-drive, the driver is frightened and walks; the car leaves island life.
    life.reset(npcs, WorldClock::at(0, 5, 0));
    check(world.run(watcher, 900, [&] {return state.driving && state.physical;}), "sets off again");
    const auto taken = life.take_car(chosen.vehicle);
    check(taken.occupied && taken.owner == chosen.owner, "the driver was inside");
    check(state.lost && !vehicles.autopiloted(chosen.vehicle) && owner.trip == Trip::Walk && owner.fear > 0, "driver out and frightened");
    check(life.take_car(chosen.vehicle).owner < 0, "a taken car is nobody's now");
    // Next session the car is home again.
    life.reset(npcs, WorldClock::at(1, 5, 0));
    check(!state.lost && flat(vehicles.vehicles()[static_cast<std::size_t>(v)].position, chosen.spot) < .5F, "cars start each session at home");

    // A parked car stolen: nobody inside, the owner walks.
    const auto stolen = life.take_car(chosen.vehicle);
    check(stolen.owner == chosen.owner && !stolen.occupied, "stealing a parked car");
    life.bind_vehicles(nullptr, nullptr);
    check(!vehicles.autopiloted(chosen.vehicle), "unbinding releases every car");
    std::cout << "traffic tests passed\n";
    return 0;
}
