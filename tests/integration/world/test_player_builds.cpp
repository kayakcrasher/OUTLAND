// Explore's BUILD mode: a player lays a street off a highway, builds a cottage and a bus stop on
// it; the town joins the road network and Linio 1, newcomers move in, and it all saves.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/build/PlayerBuilds.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/transit/BusLine.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace outland;
using game::build::Piece;
using game::build::Placed;
using game::build::PlayerBuilds;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::size_t roads(const world::VerdaRegion& region) {
    std::size_t n = 0;
    for (const auto& town : region.settlements()) n += town.roads.size();
    return n;
}
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    const std::size_t roads_before = roads(region);

    // Find open country beside a long highway: a 70 m street off it at right angles must fit,
    // with room for a cottage beside the far end.
    PlayerBuilds builds;
    Placed street, cottage, stop;
    bool found = false;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
        if (found || Vector3Distance(road.start, road.end) < 150 || road.width < 7) continue;
        const Vector3 along = Vector3Normalize(Vector3Subtract(road.end, road.start));
        const Vector3 side{-along.z, 0, along.x};
        for (float t = .3F; t < .7F && !found; t += .1F) for (const float s : {-1.0F, 1.0F}) {
            if (found) break;
            const Vector3 on = Vector3Lerp(road.start, road.end, t);
            street = {}; street.piece = Piece::Road;
            street.position = on;
            street.end = Vector3Add(on, Vector3Scale(side, s * 70));
            cottage = {}; cottage.piece = Piece::Cottage;
            // Beside the street's far half, its door toward the street.
            const Vector3 lot = Vector3Add(Vector3Add(on, Vector3Scale(side, s * 52)), Vector3Scale(along, 14));
            cottage.position = {lot.x, world::terrain::TerrainHeight::sample(lot.x, lot.z), lot.z};
            const Vector3 to_street = Vector3Normalize(Vector3Subtract(Vector3Add(on, Vector3Scale(side, s * 52)), lot));
            cottage.yaw = std::atan2(-to_street.x, -to_street.z) * RAD2DEG;
            if (!builds.check(region, street).empty() || !builds.check(region, cottage).empty()) continue;
            // Lay the street, then see that the cottage still fits beside it.
            builds.add(street);
            builds.apply(region);
            if (builds.check(region, cottage).empty()) found = true;
            else { builds.remove(street.end, 2); builds.apply(region); }
        }
    }
    check(found, "open country for a new street and a cottage");
    check(roads(region) == roads_before + 1, "the street is laid into the region");
    std::cout << "street from " << street.position.x << "," << street.position.z << " to " << street.end.x << "," << street.end.z << '\n';
    cottage.position.y = 0;
    const Placed& home = builds.add(cottage);
    check(home.home() && home.residents == 0 && home.id().starts_with("player_home_cottage_"), "a player home, empty");
    builds.apply(region);
    world::physics::MeshCollisionLibrary::preload(region);
    // Nothing else can go on top of it.
    Placed shed = cottage; shed.piece = Piece::House;
    check(!builds.check(region, shed).empty(), "a house cannot be built on top of the cottage");
    // A bus stop on the new street, snapped to its verge; none where there is no road.
    Vector3 shelter{};
    float yaw = 0;
    // Somewhere out in the country, well away from every road.
    Vector3 lonely = Vector3Add(street.end, {300, 0, 300});
    for (float r = 300; r < 1500; r += 50) {
        bool clear = true;
        lonely = Vector3Add(street.end, {r, 0, r * .4F});
        for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
            const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
            const float t = l2 > 0 ? std::clamp(((lonely.x - road.start.x) * dx + (lonely.z - road.start.z) * dz) / l2, 0.0F, 1.0F) : 0;
            if (std::hypot(lonely.x - road.start.x - dx * t, lonely.z - road.start.z - dz * t) < 80) clear = false;
        }
        if (clear) break;
    }
    check(!PlayerBuilds::snap_bus_stop(region, lonely, shelter, yaw), "no bus stop away from roads");
    check(PlayerBuilds::snap_bus_stop(region, Vector3Lerp(street.position, street.end, .45F), shelter, yaw), "a bus stop snaps to the new street");
    stop.piece = Piece::BusStop; stop.position = shelter; stop.yaw = yaw;
    check(builds.check(region, stop).empty(), "the bus stop fits: " + builds.check(region, stop));
    builds.add(stop);
    builds.apply(region);
    world::physics::MeshCollisionLibrary::preload(region);
    check(roads(region) == roads_before + 1, "re-applying does not duplicate the street");

    // The street joins the island's roads: from the cottage's door to the capital.
    world::roads::RoadGraph graph(region);
    const auto route = graph.route(cottage.position, {3, 0, -30});
    check(route.complete, "a road from the new cottage to the capital");

    // Linio 1 now calls at the player's stop.
    game::vehicles::VehicleRegistry types;
    std::string error;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem vehicles(types);
    vehicles.reconcile(region);
    game::transit::BusLine line;
    check(line.build(region, vehicles, nullptr, 7), "the bus line builds");
    bool calls = false;
    for (const auto& s : line.stops()) calls |= s.asset.starts_with("player_stop_");
    check(calls, "Linio 1 calls at the player's stop");
    // Run the line until a bus pulls in at the player's stop (the game moves a family in then).
    line.start(region, vehicles);
    std::vector<float> dwell(line.buses().size(), 0);
    bool arrived = false;
    float waited = 0;
    for (; waited < 2400 && !arrived; waited += .1F) {
        line.update(.1F, region, vehicles, {0, 0, 9000}, nullptr); // the player well out of the way
        for (std::size_t i = 0; i < line.buses().size(); ++i) {
            const auto& bus = line.buses()[i];
            if (bus.dwell > 0 && dwell[i] <= 0)
                for (const auto& s : line.stops())
                    if (s.asset.starts_with("player_stop_") && Vector3Distance(s.kerb, bus.position) < 30) arrived = true;
            dwell[i] = bus.dwell;
        }
    }
    std::cout << "a bus reached the player's stop after " << waited << " s\n";
    check(arrived, "a bus calls at the player's stop");
    line.stop(region, vehicles);

    // Island life: the new home stands empty until newcomers arrive, then they live there.
    game::life::LifeSimulation life;
    life.build(region, nullptr);
    const auto population = life.residents().size();
    for (const auto& place : life.places()) check(!place.building_id.starts_with("player_"), "player homes start empty");
    check(builds.vacant_home_near(shelter, 600) == 1, "the empty cottage is found from the stop");
    const auto ids = life.move_in(builds.building(builds.placed()[1]), 4, 99, &shelter);
    check(ids.size() == 4 && life.residents().size() == population + 4, "a family of four moves in");
    const auto& newcomer = life.residents()[static_cast<std::size_t>(ids.front())];
    const auto& their_home = life.places()[static_cast<std::size_t>(newcomer.home)];
    check(their_home.building_id == builds.placed()[1].id() && their_home.home_capacity == 4, "they live in the cottage");
    check(!newcomer.days[0].empty() && newcomer.household.size() == 4, "they have a household and a day plan");
    check(Vector3Distance(life.position(newcomer), shelter) < 3, "they step off at the bus stop");
    builds.find(builds.placed()[1].id())->residents = 4;
    check(builds.vacant_home_near(shelter, 600) < 0 && builds.empty_homes() == 0, "no empty homes left");
    check(builds.remove(cottage.position, 3) == PlayerBuilds::Removal::Occupied, "an occupied home cannot be removed");

    // Save and load.
    const std::string path = (std::filesystem::temp_directory_path() / "outland_test_profile" / "explore.builds").string();
    check(builds.save(path, error), "save: " + error);
    PlayerBuilds loaded;
    check(loaded.load(path, error) && loaded.placed().size() == 3, "load: " + error);
    check(loaded.placed()[1].residents == 4 && loaded.placed()[2].piece == Piece::BusStop, "residents and pieces survive a save");
    Placed later; later.piece = Piece::Tree; later.position = Vector3Add(cottage.position, {0, 0, 20});
    const int last = loaded.placed().back().serial;
    check(loaded.add(later).serial == last + 1, "serials carry on after a load");
    check(PlayerBuilds::path_beside("/x/profiles/explore.loot") == "/x/profiles/explore.builds", "builds sit beside the loot profile");

    // Another mode: the player's town is taken away again.
    builds.clear();
    builds.apply(region);
    for (const auto& town : region.settlements()) {
        for (const auto& b : town.buildings) check(!b.id.starts_with("player_"), "no player buildings left");
        for (const auto& a : town.assets) check(!a.id.starts_with("player_"), "no player assets left");
    }
    check(roads(region) == roads_before, "the street is gone");
    std::filesystem::remove_all(std::filesystem::path(path).parent_path());
    std::cout << "player build tests passed\n";
    return 0;
}
