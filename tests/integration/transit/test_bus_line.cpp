#include "outland/characters/CharacterRegistry.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/transit/BusLine.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
using namespace outland;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float to_road(const world::VerdaRegion& region, Vector3 p) {
    float best = 1e9F;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
        const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
        const float t = l2 < 1e-6F ? 0 : std::clamp(((p.x - road.start.x) * dx + (p.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
        best = std::min(best, std::hypot(p.x - road.start.x - dx * t, p.z - road.start.z - dz * t) - road.width * .5F);
    }
    return best;
}
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    std::string error;
    game::vehicles::VehicleRegistry types;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    const auto* bus_type = types.find("bus");
    check(bus_type && bus_type->length > 11 && bus_type->width > 2.4F, "the bus is a vehicle type");
    characters::CharacterRegistry characters;
    check(characters.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/characters/character_manifest.tsv", error), error);
    game::vehicles::VehicleSystem vehicles(types);
    vehicles.reconcile(region);

    game::transit::BusLine line;
    check(line.build(region, vehicles, &characters, 3), "build the line");
    std::cout << line.stops().size() << " stops, " << line.buses().size() << " buses, " << line.turnarounds() << " turnarounds, loop " << line.loop().length / 1000 << " km:";
    for (const auto& stop : line.stops()) std::cout << ' ' << stop.town;
    std::cout << '\n';
    // Every town has a stop but the backcountry (Sovaĝejo) and the small highway hamlets.
    std::size_t served = 0;
    for (const auto& town : region.settlements())
        served += town.id != "ferarbaro_sovagejo" && !town.id.starts_with("ranch_") && (!town.id.starts_with("hamlet_") || town.id == "hamlet_ruldomaro");
    check(line.stops().size() == served, "a stop in every town");
    for (const auto& stop : line.stops()) check(stop.town != "Sovaĝejo", "no bus out to Sovaĝejo");
    std::set<std::string> towns;
    for (const auto& stop : line.stops()) towns.insert(stop.town);
    check(towns.size() == line.stops().size() && line.stops().front().town == "Verda", "every town once, starting at the capital");
    check(line.buses().size() == 3, "three buses");
    for (const auto& bus : line.buses()) check(!bus.driver.empty(), "a driver at every wheel");
    check(line.loop().length > 8000 && line.loop().complete, "a loop round the island");
    // Every shelter is on the kerb the bus pulls into, the side traffic keeps to.
    for (const auto& stop : line.stops()) {
        float heading = 0;
        world::roads::route_point(line.loop(), std::fmod(stop.at + 3, line.loop().length), &heading);
        const Vector3 d{std::sin(heading * DEG2RAD), 0, std::cos(heading * DEG2RAD)}, kerb_side{-d.z, 0, d.x};
        check((stop.shelter.x - stop.kerb.x) * kerb_side.x + (stop.shelter.z - stop.kerb.z) * kerb_side.z > 2,
              "the shelter at " + stop.town + " is on the bus's kerb side");
    }

    line.start(region, vehicles);
    check(line.running(), "running");
    for (const auto& bus : line.buses()) {
        check(bus.running && vehicles.kinematic(bus.vehicle), "buses on the road");
        const int index = vehicles.find(bus.vehicle);
        check(vehicles.nearest(region, bus.position, 8) != index, "nobody climbs into a running bus");
    }

    // Forty minutes of service: every bus calls at every stop, keeps to the road, waits at stops.
    const Vector3 far_away{0, 0, 9000};
    std::vector<std::set<int>> called(line.buses().size());
    std::vector<int> calls(line.buses().size(), 0);
    float worst_road = 0, fastest = 0;
    for (float t = 0; t < 2400; t += .1F) {
        line.update(.1F, region, vehicles, far_away, nullptr);
        for (std::size_t b = 0; b < line.buses().size(); ++b) {
            const auto& bus = line.buses()[b];
            fastest = std::max(fastest, bus.speed);
            worst_road = std::max(worst_road, to_road(region, bus.position));
            if (bus.dwell > line.config().dwell - .15F) {
                const auto& stop = line.stops()[static_cast<std::size_t>(bus.next_stop)];
                check(bus.speed < .5F && Vector3Distance({bus.position.x, 0, bus.position.z}, {stop.kerb.x, 0, stop.kerb.z}) < 9,
                      "pulls up at " + stop.town);
                called[b].insert(bus.next_stop);
                ++calls[b];
            }
        }
    }
    for (std::size_t b = 0; b < called.size(); ++b) {
        std::cout << "bus " << b + 1 << ": called " << calls[b] << " times at " << called[b].size() << " stops:";
        for (const int s : called[b]) std::cout << ' ' << line.stops()[static_cast<std::size_t>(s)].town;
        std::cout << '\n';
        check(called[b].size() == line.stops().size(), "every bus calls at every stop");
    }
    std::cout << "fastest " << fastest * 3.6F << " km/h, furthest off the road " << worst_road << " m\n";
    check(fastest <= line.config().cruise + .01F, "within the speed limit");
    // The middle of a 12 m bus cuts inside a tight corner by a couple of metres, as real ones do.
    check(worst_road < 2.5F, "keeps to the road");

    // Someone standing in the lane: the bus stops short of them and waits.
    {
        auto& bus = const_cast<game::transit::Bus&>(line.buses().front());
        bus.dwell = 0;
        for (int i = 0; i < 400 && bus.speed < 8; ++i) line.update(.1F, region, vehicles, far_away, nullptr);
        for (int i = 0; i < 2000 && (bus.speed < 8 || bus.dwell > 0); ++i) line.update(.1F, region, vehicles, far_away, nullptr);
        check(bus.speed >= 8, "under way");
        // Someone stands in the bus's lane 40 m on.
        const Vector3 person = world::roads::route_point(line.loop(), std::fmod(bus.progress + 40, line.loop().length));
        for (int i = 0; i < 300; ++i) {
            // The person stays put in the lane ahead.
            line.update(.1F, region, vehicles, person, nullptr);
        }
        const float gap = Vector3Distance({bus.position.x, 0, bus.position.z}, {person.x, 0, person.z});
        std::cout << "stopped " << gap << " m short of the person\n";
        check(bus.speed < .05F && gap > 5.5F, "stops for someone in the lane");
        for (int i = 0; i < 50; ++i) line.update(.1F, region, vehicles, far_away, nullptr);
        check(bus.speed > .5F, "and goes on when they move");
    }

    // A wrecked bus leaves the line; stopping the line parks the rest at the depot.
    {
        const auto id = line.buses()[1].vehicle;
        vehicles.asset(region, id)->vehicle.destroyed = true;
        line.update(.1F, region, vehicles, far_away, nullptr);
        check(!line.buses()[1].running && !vehicles.kinematic(id), "a wreck is off the line");
        line.stop(region, vehicles);
        for (const auto& bus : line.buses()) {
            const auto* asset = vehicles.asset(region, bus.vehicle);
            check(!vehicles.kinematic(bus.vehicle), "released");
            if (!asset->vehicle.destroyed) check(Vector3Distance({asset->position.x, 0, asset->position.z}, {asset->vehicle.home.x, 0, asset->vehicle.home.z}) < .1F, "parked at the depot");
        }
    }
    std::cout << "bus line tests passed\n";
    return 0;
}
