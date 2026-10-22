// The island's roads, audited on the baked map: one connected network (every town reachable
// from the capital), no road end left a few metres short of the road it was meant to meet, and
// nothing solid standing on a carriageway.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
using namespace outland;
namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
// Distance from p to segment ab (flat), and how far along (0..1).
float to_segment(Vector3 p, Vector3 a, Vector3 b, float* along = nullptr) {
    const float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
    const float t = l2 > 0 ? std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / l2, 0.0F, 1.0F) : 0;
    if (along) *along = t;
    return std::hypot(p.x - a.x - dx * t, p.z - a.z - dz * t);
}
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    std::vector<world::Road> roads;
    for (const auto& town : region.settlements())
        for (const auto& road : town.roads) roads.push_back(road);
    std::cout << roads.size() << " road segments\n";

    // 1. One network: the capital reaches every town by road.
    world::roads::RoadGraph graph(region);
    const auto& nodes = graph.nodes();
    std::vector<int> parent(nodes.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto find = [&](int x) { while (parent[static_cast<std::size_t>(x)] != x) x = parent[static_cast<std::size_t>(x)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])]; return x; };
    for (const auto& e : graph.edges()) parent[static_cast<std::size_t>(find(e.a))] = find(e.b);
    std::map<int, float> length;
    for (const auto& e : graph.edges()) length[find(e.a)] += e.length;
    const int capital = find(graph.nearest({0, 0, 0}).edge >= 0 ? graph.edges()[static_cast<std::size_t>(graph.nearest({0, 0, 0}).edge)].a : 0);
    for (const auto& town : region.settlements()) {
        if (town.roads.empty()) continue;
        const auto spot = graph.nearest(town.center);
        if (spot.edge < 0) continue;
        const int c = find(graph.edges()[static_cast<std::size_t>(spot.edge)].a);
        if (c != capital) std::cout << "  cut off: " << town.id << " (" << length[c] << " m of road)\n";
        check(c == capital, "the capital reaches " + town.id + " by road");
    }
    int islands = 0;
    for (const auto& [c, metres] : length)
        if (c != capital) {
            ++islands;
            std::cout << "  island of road: " << metres << " m";
            for (const auto& e : graph.edges()) if (find(e.a) == c) { std::cout << " at " << nodes[static_cast<std::size_t>(e.a)].x << "," << nodes[static_cast<std::size_t>(e.a)].z; break; }
            std::cout << '\n';
        }

    // 2. Road ends that stop just short of another road (a gap a car falls into).
    int gaps = 0;
    for (std::size_t i = 0; i < roads.size(); ++i)
        for (const Vector3 end : {roads[i].start, roads[i].end}) {
            float best = 1e9F;
            bool joined = false;
            for (std::size_t j = 0; j < roads.size(); ++j) {
                if (i == j) continue;
                const float d = to_segment(end, roads[j].start, roads[j].end);
                const float edge = d - roads[j].width * .5F;
                if (d < .6F || edge <= .05F) { joined = true; break; }
                best = std::min(best, edge);
            }
            if (!joined && best < 3.0F) {
                ++gaps;
                std::cout << "  gap " << best << " m at " << end.x << "," << end.z << " (" << roads[i].width << " m road)\n";
            }
        }
    check(gaps == 0, "no road end stops just short of another road: " + std::to_string(gaps));

    // 3. Nothing solid on a carriageway (a lamp, a tree, a wall, a building), but what spans it:
    // the footprint of everything solid, sampled, kept off the lanes.
    int blocked = 0;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            if (!asset.collision || !asset.vehicle.definition.empty()) continue;
            if (asset.model_path.ends_with("loading_chute.glb")) continue;   // a ranch drive ends at it: the trucks back up to it
            if (asset.model_path.find("/city/ground/") != std::string::npos) continue;   // paving and parking lots: driven over
            const float a = asset.rotation_y * DEG2RAD;
            const float hx = asset.size.x * .5F, hz = asset.size.z * .5F;
            std::vector<Vector3> points{asset.position};
            for (const float u : {-1.0F, -.5F, 0.0F, .5F, 1.0F})
                for (const float v : {-1.0F, -.5F, 0.0F, .5F, 1.0F}) {
                    if (std::abs(u) < 1 && std::abs(v) < 1 && (u != 0 || v != 0)) continue;
                    const float x = u * hx * .9F, z = v * hz * .9F;
                    points.push_back({asset.position.x + x * std::cos(a) + z * std::sin(a), 0, asset.position.z - x * std::sin(a) + z * std::cos(a)});
                }
            bool hit = false;
            for (const auto& road : roads) {
                for (const auto& p : points) {
                    float along = 0;
                    const float d = to_segment(p, road.start, road.end, &along);
                    // Beside the segment's span (past its ends the next segment or the junction is measured).
                    if (along <= 1e-3F || along >= 1 - 1e-3F || d > road.width * .5F - .4F) continue;
                    // A divided highway's planted median (trees and lamps down the middle) is meant to be there.
                    if (road.type == world::RoadType::Highway && d < world::highways::median_half + .3F) continue;
                    ++blocked;
                    hit = true;
                    std::cout << "  on the road: " << asset.id << " (" << asset.model_path << ") at " << asset.position.x << "," << asset.position.z
                              << ", reaching " << d << " m from the centre of a " << road.width << " m road (" << road.start.x << "," << road.start.z
                              << " -> " << road.end.x << "," << road.end.z << ", type " << static_cast<int>(road.type) << ")\n";
                    break;
                }
                if (hit) break;
            }
        }
    check(blocked == 0, "nothing solid on a carriageway: " + std::to_string(blocked));
    // 4. Drivable: a car (and a pickup) set down anywhere in either lane or kerbside, facing along
    // the road, runs into nothing - no invisible box (a lamp's arms, a parking lot's slab, a tree on
    // the kerb).
    std::string error;
    game::vehicles::VehicleRegistry catalog;
    check(catalog.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem vehicles(catalog);
    vehicles.set_training_structure(false);
    int stuck = 0;
    std::vector<Vector3> reported;
    for (const char* id : {"hatchback", "truck"}) {
        const auto* car = catalog.find(id);
        if (!car) { check(false, std::string("vehicle ") + id); continue; }
        for (const auto& road : roads) {
            const float length = flat(road.start, road.end);
            if (length < 8) continue;
            const float yaw = std::atan2(road.end.x - road.start.x, road.end.z - road.start.z) * RAD2DEG;
            const Vector3 dir{(road.end.x - road.start.x) / length, 0, (road.end.z - road.start.z) / length}, side{dir.z, 0, -dir.x};
            const float lane = road.type == world::RoadType::Highway ? world::highways::median_half + 2.2F : road.width * .25F;
            // ...and kerbside (a car's width in from the edge), where a stray box would catch a wing.
            const float edge = std::max(lane, road.width * .5F - car->width * .5F - .4F);
            for (float d = 4; d < length - 4; d += 2.5F)
                for (const float o : {-lane, lane, -edge, edge}) {
                    Vector3 p{road.start.x + dir.x * d + side.x * o, 0, road.start.z + dir.z * d + side.z * o};
                    p.y = world::physics::WorldCollision::ground_height({p.x, 400, p.z}, 400, region);
                    if (!vehicles.blocked(region, p, yaw, *car, 1)) continue;
                    // Something solid (not just a steep pitch, which the grading answers for).
                    bool solid = false;
                    for (const float u : {-.5F, 0.0F, .5F}) for (const float v : {-.5F, 0.0F, .5F}) {
                        const Vector3 q{p.x + side.x * u * car->width + dir.x * v * car->length, p.y, p.z + side.z * u * car->width + dir.z * v * car->length};
                        solid |= world::physics::WorldCollision::vehicle_blocked(q, region, .2F);
                    }
                    if (!solid) continue;
                    ++stuck;
                    bool near = false;
                    for (const auto& r : reported) near |= flat(r, p) < 25;
                    if (near) continue;
                    reported.push_back(p);
                    std::cout << "  a " << id << " is stuck at " << p.x << "," << p.y << "," << p.z << " on a " << road.width << " m road (type "
                              << static_cast<int>(road.type) << ", " << road.start.x << "," << road.start.z << " -> " << road.end.x << "," << road.end.z << ")\n";
                }
        }
    }
    check(stuck == 0, "a car is free to drive anywhere on the roads: " + std::to_string(stuck) + " stuck spots");
    check(islands == 0, "no island of road cut off from the rest: " + std::to_string(islands));
    std::cout << islands << " islands of road, " << gaps << " gaps, " << blocked << " obstacles\n";
    if (failures == 0) std::cout << "road audit passed\n";
    return failures == 0 ? 0 : 1;
}
