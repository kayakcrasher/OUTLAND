// Verda's highways: the network laid in the world map, the graded roadbed, the median barrier
// (cars stop at it, people climb over), and traffic keeping to its lane clear of the barrier.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/Ranches.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/roads/HighwayNetwork.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
namespace hw = world::highways;
namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::terrain::TerrainHeight::set_coastal_layout(true);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");

    // The network: a closed ring, the north-south highway right through Verda City, and the east
    // and west spokes; each a run of short, gently turning pieces.
    const auto& lines = hw::lines();
    check(lines.size() == 4, "the ring, the north-south highway and two spokes");
    {
        bool through = false;
        for (const auto& line : lines)
            for (const auto& p : line.points) through |= std::abs(p.x) < .5F && std::abs(p.z) < 31;
        check(through, "the main highway runs through downtown");
    }
    for (const auto& line : lines) {
        const auto n = line.points.size();
        for (std::size_t i = 0; i + (line.loop ? 0 : 1) < n; ++i) {
            const Vector3 a = line.points[i], b = line.points[(i + 1) % n];
            check(flat(a, b) <= 66, std::string(line.name) + ": pieces at most 65 m");
            if (i + (line.loop ? 0 : 2) < n) {
                const Vector3 c = line.points[(i + 2) % n];
                const float turn = std::acos(std::clamp(((b.x - a.x) * (c.x - b.x) + (b.z - a.z) * (c.z - b.z)) / (flat(a, b) * flat(b, c)), -1.0F, 1.0F)) * RAD2DEG;
                check(turn < 3.6F, std::string(line.name) + ": no sharp kinks (" + std::to_string(turn) + " degrees)");
            }
        }
    }
    int highway_roads = 0;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads)
        if (road.type == world::RoadType::Highway) {
            ++highway_roads;
            check(road.width == hw::width, "a highway is " + std::to_string(hw::width) + " m wide");
        }
    check(highway_roads > 250, "the highways are in the world map: " + std::to_string(highway_roads) + " pieces");

    // The roadbed: level across, eased along.
    float steepest = 0, tilt = 0;
    for (const auto& line : lines) {
        const auto n = line.points.size();
        for (std::size_t i = 0; i + (line.loop ? 0 : 1) < n; ++i) {
            const Vector3 a = line.points[i], b = line.points[(i + 1) % n];
            const float l = flat(a, b);
            const Vector3 d{(b.x - a.x) / l, 0, (b.z - a.z) / l}, right{-d.z, 0, d.x};
            for (float s = 0; s < l; s += 4) {
                const Vector3 p{a.x + d.x * s, 0, a.z + d.z * s}, q{p.x + d.x * 4, 0, p.z + d.z * 4};
                const float h = world::terrain::TerrainHeight::sample(p.x, p.z);
                const float grade = std::abs(world::terrain::TerrainHeight::sample(q.x, q.z) - h) / 4;
                if (std::getenv("OUTLAND_HIGHWAY_PROFILE") && grade > .1F) std::cout << "  steep " << grade << " at " << p.x << "," << p.z << '\n';
                steepest = std::max(steepest, grade);
                const float left = world::terrain::TerrainHeight::sample(p.x - right.x * hw::paved_half, p.z - right.z * hw::paved_half);
                const float across = world::terrain::TerrainHeight::sample(p.x + right.x * hw::paved_half, p.z + right.z * hw::paved_half);
                if (std::getenv("OUTLAND_HIGHWAY_PROFILE") && std::abs(across - left) / hw::width > .03F) std::cout << "  tilted " << std::abs(across - left) / hw::width << " at " << p.x << "," << p.z << '\n';
                tilt = std::max(tilt, std::abs(across - left) / hw::width);
            }
        }
    }
    std::cout << "steepest grade " << steepest * 100 << "%, most tilt across " << tilt * 100 << "%\n";
    check(steepest < .12F, "highway grades stay gentle: " + std::to_string(steepest * 100) + "%");
    check(tilt < .04F, "highways are level across: " + std::to_string(tilt * 100) + "%");
    // Ranch yards keep their level pads beside the road.
    for (const auto& ranch : world::ranches::ranches) {
        const world::ranches::Frame f(ranch.gate, ranch.up_track);
        const Vector3 yard = f.at(world::ranches::yard);
        check(flat(yard, f.at(0, 0)) > 50, std::string(ranch.name) + " stands back from the highway");
    }

    // The median: barriers along the network, opened at junctions and farm tracks.
    const auto& network = hw::network(region);
    std::cout << network.barrier_runs() << " barrier runs, " << network.junctions() << " junctions\n";
    check(network.barrier_runs() > 200 && network.junctions() > 20, "barrier runs and junctions");
    const Vector3 median{0, 0, -1000};   // the north spoke, out in the country
    check(network.barrier_hit(median, .5F), "a barrier down the north spoke's median");
    check(world::physics::WorldCollision::vehicle_blocked(median, region, .3F), "a car stops at the barrier");
    check(!world::physics::WorldCollision::blocked({0, 0, -1000}, region, .3F), "someone on foot climbs over it");
    check(!network.barrier_hit({hw::slow_lane, 0, -1000}, 1.2F) && !network.barrier_hit({-hw::fast_lane, 0, -1000}, 1.2F), "the lanes are clear of it");
    // Crossovers: the median opens every few hundred metres along the open road (the north spoke
    // from the city's edge to the ring has three, and the farm track's opening besides).
    {
        int openings = 0;
        bool closed = true;
        for (float z = -300; z > -1440; z -= 1) {
            const bool hit = network.barrier_hit({0, 0, z}, .1F);
            if (closed && !hit) ++openings;
            closed = hit;
        }
        std::cout << openings << " median openings on the north spoke\n";
        check(openings >= 3, "crossovers in the median: " + std::to_string(openings));
    }
    // Into a town the highway tapers to a little more than a street's width, with no barrier.
    for (std::size_t s = 0; s < region.settlements().size(); ++s)
        for (std::size_t r = 0; r < region.settlements()[s].roads.size(); ++r) {
            const auto* info = network.info(s, r);
            if (!info) continue;
            check(info->start_half >= 6.5F && info->end_half >= 6.5F && info->start_half <= hw::paved_half + .01F && info->end_half <= hw::paved_half + .01F, "paved half-widths in range");
        }
    check(!network.barrier_hit({0, 0, -175}, 1.0F) && !network.barrier_hit({0, 0, -1670}, 1.0F), "no barrier where the north spoke meets the towns");
    for (const auto& ranch : world::ranches::ranches) {
        // Where the farm track meets the highway, cars can turn across.
        Vector3 best{};
        float nearest = 1e9F;
        for (const auto& line : lines) for (std::size_t i = 0; i < line.points.size(); ++i) {
            const Vector3 a = line.points[i], b = line.points[(i + 1) % line.points.size()];
            if (!line.loop && i + 1 == line.points.size()) break;
            const float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
            const float t = std::clamp(((ranch.gate.x - a.x) * dx + (ranch.gate.y - a.z) * dz) / l2, 0.0F, 1.0F);
            const Vector3 p{a.x + dx * t, 0, a.z + dz * t};
            const float d = std::hypot(p.x - ranch.gate.x, p.z - ranch.gate.y);
            if (d < nearest) {nearest = d; best = p;}
        }
        check(!network.barrier_hit(best, 1.5F), std::string(ranch.name) + ": an opening in the median at the farm track");
    }

    // Traffic: routes between the towns keep to their lane, never into the barrier.
    world::roads::RoadGraph graph(region);
    int checked = 0;
    const std::string towns[]{"capital_verda", "village_espera", "port_luma", "south_haven", "west_roka"};
    for (const auto& from : towns) for (const auto& to : towns) {
        if (from == to) continue;
        const world::Settlement *a = nullptr, *b = nullptr;
        for (const auto& s : region.settlements()) { if (s.id == from) a = &s; if (s.id == to) b = &s; }
        if (!a || !b) continue;
        const auto route = graph.route(a->center, b->center);
        check(route.complete, "a route " + from + " to " + to);
        int hits = 0;
        for (float s = 0; s < route.length; s += 3) {
            const Vector3 p = world::roads::route_point(route, s);
            hits += network.barrier_hit(p, 1.0F);
            ++checked;
        }
        check(hits == 0, from + " to " + to + ": the car stays clear of the median (" + std::to_string(hits) + " hits)");
    }
    std::cout << checked << " route points checked\n";
    if (failures) return 1;
    std::cout << "[PASS] highways\n";
    return 0;
}
