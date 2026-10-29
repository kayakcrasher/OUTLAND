// The ring road's interchanges: where each highway crosses the ring, the ring rides over a
// roundabout on a bridge (it never stops); the highway runs into the roundabout from both sides, and
// slip roads leave the ring before the bridge and come back on after it. Checked on the world map:
// the layout, the bridge deck and the roundabout's level ground, and that traffic finds its way
// through them the right way round.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/roads/HighwayNetwork.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <string>

using namespace outland;
namespace hw = world::highways;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 offset(Vector3 p, Vector3 d, float metres) { return {p.x + d.x * metres, 0, p.z + d.z * metres}; }
// A point on the ring `s` metres from an interchange the way the ring's points run, `lateral` out
// from its centre line (positive: outward).
Vector3 on_ring(const hw::Interchange& x, float s, float lateral) {
    const float radius = std::hypot(x.at.x, x.at.z), bearing = std::atan2(x.at.z, x.at.x);
    const float sense = x.ring_forward.x * -std::sin(bearing) + x.ring_forward.z * std::cos(bearing) > 0 ? 1.0F : -1.0F;
    const float b = bearing + sense * s / radius, r = radius + lateral;
    return {r * std::cos(b), 0, r * std::sin(b)};
}
}

int main() {
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    const auto& all = hw::interchanges();
    check(all.size() == 4, "an interchange where each highway crosses the ring, both ways: " + std::to_string(all.size()));
    const auto& network = hw::network(region);
    const world::roads::RoadGraph graph(region);
    for (const auto& x : all) {
        const std::string name = "exit " + std::to_string(x.number) + " (" + std::string(x.town) + ")";
        check(x.number >= 1 && x.number <= 4 && !x.town.empty(), name + ": numbered and named");
        check(std::abs(std::hypot(x.at.x, x.at.z) - 1500) < 2, name + ": on the ring");
        // Four slip roads: off and on, either side; each from the ring's centre line to the roundabout's lane.
        check(x.slips.size() == 4, name + ": four slip roads");
        for (const auto& slip : x.slips) {
            const Vector3 ring_end = slip.off ? slip.points.front() : slip.points.back();
            const Vector3 round_end = slip.off ? slip.points.back() : slip.points.front();
            check(std::abs(std::hypot(ring_end.x, ring_end.z) - 1500) < 1, name + ": a slip road leaves the ring's centre line");
            check(std::abs(flat(round_end, x.at) - hw::roundabout_radius) < .5F, name + ": and meets the roundabout's lane");
        }
        // The bridge: the ring's roadway rides over the roundabout's level ground.
        float deck = 0;
        const float ground = world::terrain::TerrainHeight::sample(x.at.x, x.at.z);
        check(hw::deck(x.at.x, x.at.z, deck, &x.ring_forward), name + ": a deck over the crossing, for the ring");
        check(std::abs(deck - ground - hw::overpass_rise) < .3F, name + ": the deck " + std::to_string(deck - ground) + " m up");
        check(!hw::deck(x.at.x, x.at.z, deck, &x.outward), name + ": not for the highway beneath");
        check(!hw::deck(offset(x.at, x.ring_forward, 160).x, offset(x.at, x.ring_forward, 160).z, deck), name + ": the bridge ends");
        for (const float s : {-30.0F, 0.0F, 30.0F}) {
            const Vector3 p = offset(x.at, x.ring_forward, s);
            check(std::abs(world::terrain::TerrainHeight::sample(p.x, p.z) - ground) < .2F, name + ": level ground under the bridge");
        }
        // The roundabout's island has a kerb (at its level), the deck has parapets (at its own).
        const Vector3 kerb = offset(x.at, x.outward, hw::roundabout_radius - hw::roundabout_width * .5F - .4F);
        check(network.barrier_hit({kerb.x, ground, kerb.z}, .5F), name + ": the island's kerb stops a car");
        check(!network.barrier_hit({kerb.x, ground + hw::overpass_rise, kerb.z}, .5F), name + ": not one on the bridge above it");
        const Vector3 edge = offset(x.at, x.outward, hw::parapet);
        hw::deck(x.at.x, x.at.z, deck, &x.ring_forward);
        check(network.barrier_hit({edge.x, deck, edge.z}, .5F), name + ": a parapet along the deck");
        check(!network.barrier_hit({x.at.x + x.outward.x * 6.25F, ground, x.at.z + x.outward.z * 6.25F}, 1.0F), name + ": nothing of the bridge's on the roundabout");

        // Along the highway, through the roundabout: round it the right way, never onto the ring.
        const Vector3 inner = offset(x.at, x.outward, -120), outer = offset(x.at, x.outward, 120);
        const auto across = graph.route(inner, outer);
        check(across.complete, name + ": a way across");
        bool on_ring = false, through_roundabout = false;
        for (float s = 0; s < across.length; s += 2) {
            const Vector3 p = world::roads::route_point(across, s, nullptr, false);
            on_ring |= std::abs(std::hypot(p.x, p.z) - 1500) < 3 && flat(p, x.at) > hw::roundabout_pad;
            through_roundabout |= std::abs(flat(p, x.at) - hw::roundabout_radius) < 2;
        }
        check(through_roundabout && !on_ring && across.length < 330, name + ": straight across through the roundabout (" + std::to_string(across.length) + " m)");
        // Round it anticlockwise on the map (traffic keeps right): the angle about the centre falls.
        {
            float turned = 0, last = 0;
            bool started = false;
            for (float s = 0; s < across.length; s += 1) {
                const Vector3 p = world::roads::route_point(across, s, nullptr, false);
                if (std::abs(flat(p, x.at) - hw::roundabout_radius) > .8F) continue;
                const float a = std::atan2(p.z - x.at.z, p.x - x.at.x);
                if (started) turned += std::remainder(a - last, 2 * PI);
                last = a; started = true;
            }
            check(turned < 0, name + ": round the roundabout the right way (" + std::to_string(turned * RAD2DEG) + " degrees)");
        }
        // Off the ring and into the town beyond: by a slip road (the way it runs) and the roundabout.
        for (const float way : {-1.0F, 1.0F}) {
            const Vector3 ring_point = offset(x.at, x.ring_forward, way * 500);
            const auto off = graph.route(ring_point, outer);
            check(off.complete, name + ": off the ring");
            bool slipped = false;
            for (float s = 0; s < off.length; s += 2) {
                const Vector3 p = world::roads::route_point(off, s, nullptr, false);
                const float lateral = std::hypot(p.x, p.z) - 1500;
                slipped |= std::abs(std::abs(lateral) - hw::slip_offset) < 1.5F && flat(p, x.at) > hw::roundabout_pad + 10;
            }
            check(slipped && off.length < 1100, name + ": by a slip road (" + std::to_string(off.length) + " m)");
            // And back on: the other way.
            const auto on = graph.route(outer, ring_point);
            check(on.complete && on.length < 1100, name + ": back onto the ring (" + std::to_string(on.length) + " m)");
        }
    }
    // A car drives along the ring over each bridge, in either carriageway, and up on the deck the
    // whole way across.
    {
        world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
        game::vehicles::VehicleRegistry types;
        std::string error;
        check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
        game::vehicles::VehicleSystem cars(types);
        std::string test_car;
        for (const auto& x : all)
            for (const float way : {1.0F, -1.0F}) {
                const std::string name = "exit " + std::to_string(x.number) + (way > 0 ? " forward" : " back");
                // On the right: forward traffic is inside the ring, the other way outside it.
                const Vector3 dir{x.ring_forward.x * way, 0, x.ring_forward.z * way};
                const float lane = way > 0 ? -hw::slow_lane : hw::slow_lane;
                const Vector3 from = on_ring(x, -way * 260, lane);
                // (One car for every run: the world's own parked cars fill the system up.)
                Vector3 start = from;
                if (test_car.empty()) {
                    check(cars.spawn(region, from, std::atan2(dir.x, dir.z) * RAD2DEG, "hatchback"), name + ": a car");
                    test_car = cars.last_spawned();
                }
                const auto id = test_car;
                cars.place(region, id, start, std::atan2(dir.x, dir.z) * RAD2DEG);
                const int index = cars.find(id);
                Vector3 feet = start;
                check(cars.enter(region, index, feet), name + ": at the wheel");
                float highest = -1e9F;
                const auto route = graph.route(from, on_ring(x, way * 320, lane));
                float progress = 0;
                for (int f = 0; f < 60 * 30; ++f) {
                    const auto* car = cars.asset(region, id);
                    progress = world::roads::route_progress(route, car->position, progress);
                    const Vector3 target = world::roads::route_point(route, progress + 14);
                    const float want = std::atan2(target.x - car->position.x, target.z - car->position.z) * RAD2DEG;
                    const float error = std::remainder(want - car->rotation_y, 360.0F);
                    const float speed = cars.driver() ? cars.driver()->speed : 0;
                    cars.update(1 / 60.0F, {speed < 20 ? 1.0F : 0.0F, std::clamp(error / 25.0F, -1.0F, 1.0F)}, region, car->position);
                    const float along = (car->position.x - x.at.x) * dir.x + (car->position.z - x.at.z) * dir.z;
                    if (std::abs(along) < 20) highest = std::max(highest, car->position.y);
                    if (along > 120) break;
                    if (std::getenv("DEBUG_DRIVE") && std::abs(along) < 70 && f % 6 == 0) {float dk = 0; const bool on = hw::deck(car->position.x, car->position.z, dk); std::cout << name << " along " << along << " y " << car->position.y << " deck " << on << " " << dk << " terrain " << world::terrain::TerrainHeight::sample(car->position.x, car->position.z) << " lat " << std::hypot(car->position.x, car->position.z) - 1500 << '\n';}
                    if (std::getenv("DEBUG_DRIVE") && f % 60 == 0) std::cout << name << " t" << f / 60 << " at " << car->position.x << "," << car->position.y << "," << car->position.z << " speed " << speed << " along " << along << " route " << route.length << " target " << target.x << "," << target.z << '\n';
                }
                const auto* car = cars.asset(region, id);
                const float along = (car->position.x - x.at.x) * dir.x + (car->position.z - x.at.z) * dir.z;
                float deck = 0;
                hw::deck(x.at.x, x.at.z, deck, &x.ring_forward);
                check(along > 100, name + ": over the bridge and on (" + std::to_string(along) + " m past)");
                check(std::abs(highest - deck) < .6F, name + ": up on the deck (" + std::to_string(highest) + " at " + std::to_string(deck) + ")");
                Vector3 out;
                cars.exit(region, out, true);
            }
    }
    // One-way roads are only driven their way.
    for (const auto& x : all)
        for (const auto& slip : x.slips) {
            const Vector3 from = slip.points[1], to = slip.points[slip.points.size() - 2];
            const auto route = graph.route(from, to);
            check(route.complete && route.length < 400, "a slip road is driven its way: " + std::to_string(route.length) + " m");
            const auto back = graph.route(to, from);
            check(back.length > route.length + 50, "and not back against it: " + std::to_string(back.length) + " m");
        }
    if (failures) return 1;
    std::cout << "[PASS] ring interchanges\n";
    return 0;
}
