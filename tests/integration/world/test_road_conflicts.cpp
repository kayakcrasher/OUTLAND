// Where the roads meet the rest of the world, on the baked map: nothing standing on a carriageway
// (solid or not - a house, a tree, a fence, a stall), no two roads crossing with no junction
// between them (one running through the other), and nothing near a road left hanging over the
// land or sunk into it where the land was cut and filled to the roads.
// OUTLAND_CONFLICT_LOG=1 prints every one.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/roads/HighwayNetwork.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace outland;
namespace hw = world::highways;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float to_segment(Vector3 p, Vector3 a, Vector3 b, float* along = nullptr) {
    const float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
    const float t = l2 > 0 ? std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / l2, 0.0F, 1.0F) : 0;
    if (along) *along = t;
    return std::hypot(p.x - a.x - dx * t, p.z - a.z - dz * t);
}
// Do segments ab and cd cross (properly, away from their ends)?
bool cross(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3& at) {
    const float r1 = b.x - a.x, r2 = b.z - a.z, s1 = d.x - c.x, s2 = d.z - c.z;
    const float den = r1 * s2 - r2 * s1;
    if (std::abs(den) < 1e-6F) return false;
    const float t = ((c.x - a.x) * s2 - (c.z - a.z) * s1) / den, u = ((c.x - a.x) * r2 - (c.z - a.z) * r1) / den;
    if (t <= .02F || t >= .98F || u <= .02F || u >= .98F) return false;
    at = {a.x + r1 * t, 0, a.z + r2 * t};
    return true;
}
bool log() { return std::getenv("OUTLAND_CONFLICT_LOG") != nullptr; }
// What belongs on or over a road: its paving, its furniture (signals, signs and gantries over it,
// the lamps down a boulevard's median), the cars on it.
bool of_the_road(const world::WorldAsset& a) {
    if (!a.vehicle.definition.empty() || a.model_path.empty()) return a.model_path.empty() && a.type != world::AssetType::Tree;
    for (const char* kind : {"/city/ground/", "Traffic lights", "signal", "gantry", "sign_", "_sign", "Sign", "road_marking", "crosswalk", "Crosswalk", "manhole", "loading_chute", "harbour_crane", "quay_apron", "north/l_deck"})
        if (a.model_path.find(kind) != std::string::npos) return true;
    return false;
}
} // namespace

int main() {
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    std::vector<world::Road> roads;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) roads.push_back(road);

    // 1. Nothing standing on a carriageway, solid or not.
    int on_road = 0;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            if (of_the_road(asset)) continue;
            const float a = asset.rotation_y * DEG2RAD, hx = asset.size.x * .4F, hz = asset.size.z * .4F;
            for (const auto& road : roads) {
                bool hit = false;
                for (const float u : {-1.0F, 0.0F, 1.0F})
                    for (const float v : {-1.0F, 0.0F, 1.0F}) {
                        const float x = u * hx, z = v * hz;
                        const Vector3 p{asset.position.x + x * std::cos(a) + z * std::sin(a), 0, asset.position.z - x * std::sin(a) + z * std::cos(a)};
                        float along = 0;
                        const float d = to_segment(p, road.start, road.end, &along);
                        if (along <= 1e-3F || along >= 1 - 1e-3F || d > road.width * .5F - .3F) continue;
                        if (road.type == world::RoadType::Highway && d < hw::median_half + .3F) continue;   // a boulevard's planted median
                        // Under a bridge deck, or on it: different levels.
                        float deck = 0;
                        if (hw::deck(p.x, p.z, deck) && std::abs(asset.position.y - deck) > 3) continue;
                        hit = true;
                    }
                if (!hit) continue;
                ++on_road;
                if (log()) std::cout << "  on the road: " << asset.id << " (" << asset.model_path << ", " << (asset.collision ? "solid" : "not solid") << ") at "
                                     << asset.position.x << "," << asset.position.z << " on a " << road.width << " m road of type " << static_cast<int>(road.type)
                                     << " (" << road.start.x << "," << road.start.z << " -> " << road.end.x << "," << road.end.z << ")\n";
                break;
            }
        }
    // 2. Roads crossing with no junction (neither has a node there): one runs through the other.
    int crossings = 0;
    std::vector<Vector3> seen;
    for (std::size_t i = 0; i < roads.size(); ++i)
        for (std::size_t j = i + 1; j < roads.size(); ++j) {
            Vector3 at{};
            // (The road graph makes a junction of any crossing of streets: a conflict is a slip road
            // or a roundabout's lane cut across, or a divided highway crossed at grade.)
            const auto special = [](const world::Road& r) {
                return r.type == world::RoadType::Ramp || r.type == world::RoadType::Roundabout ||
                       (r.type == world::RoadType::Highway && r.width > hw::boulevard_width + .5F);
            };
            if (!special(roads[i]) && !special(roads[j])) continue;
            if (!cross(roads[i].start, roads[i].end, roads[j].start, roads[j].end, at)) continue;
            // A level crossing at a bridge is no conflict: the ring rides over its roundabout.
            float deck = 0;
            if (hw::deck(at.x, at.z, deck)) continue;
            // A highway crossed at grade is a junction if its median opens there (no barrier across the way).
            if (!hw::network(region).barrier_hit({at.x, world::terrain::TerrainHeight::sample(at.x, at.z), at.z}, 2.0F)) continue;
            // (Joined just beside: the graph welds ends within a road's width.)
            bool joined = false;
            for (const auto& r : roads)
                for (const Vector3 end : {r.start, r.end}) joined |= flat(end, at) < std::max(roads[i].width, roads[j].width) * .5F + 1;
            if (joined) continue;
            bool near = false;
            for (const auto& s : seen) near |= flat(s, at) < 15;
            if (near) continue;
            seen.push_back(at);
            ++crossings;
            if (log()) std::cout << "  crossing with no junction at " << at.x << "," << at.z << ": a " << roads[i].width << " m road of type " << static_cast<int>(roads[i].type)
                                 << " and a " << roads[j].width << " m road of type " << static_cast<int>(roads[j].type) << '\n';
        }
    // 3. Near a road (within 60 m of one), nothing hanging over the land or sunk into it.
    int misplaced = 0;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            if (!asset.vehicle.definition.empty() || asset.model_path.empty() || asset.size.y < .5F) continue;
            if (asset.model_path.find("Stairs") != std::string::npos) continue;   // up to a door
            if (asset.model_path.ends_with("north/l_deck.glb")) continue;   // La Levita's deck, up on its bents
            // An upper storey stands on the one below (a wall piece of the same building under it).
            if (asset.model_path.find("/Building Parts/") != std::string::npos) {
                bool storey = false;
                for (const auto& other : town.assets)
                    storey |= &other != &asset && other.model_path.find("/Building Parts/") != std::string::npos &&
                              flat(other.position, asset.position) < 1.5F && other.position.y < asset.position.y - 1;
                if (storey) continue;
            }
            float nearest = 1e9F;
            for (const auto& road : roads) nearest = std::min(nearest, to_segment(asset.position, road.start, road.end) - road.width * .5F);
            if (nearest > 60) continue;
            // Under its footprint, the land's lowest and highest.
            const float a = asset.rotation_y * DEG2RAD, hx = asset.size.x * .45F, hz = asset.size.z * .45F;
            float low = 1e9F, high = -1e9F;
            for (const float u : {-1.0F, 0.0F, 1.0F})
                for (const float v : {-1.0F, 0.0F, 1.0F}) {
                    const float x = u * hx, z = v * hz;
                    const float h = world::terrain::TerrainHeight::sample(asset.position.x + x * std::cos(a) + z * std::sin(a), asset.position.z - x * std::sin(a) + z * std::cos(a));
                    low = std::min(low, h); high = std::max(high, h);
                }
            float deck = 0;
            if (hw::deck(asset.position.x, asset.position.z, deck) && std::abs(asset.position.y - deck) < 1) continue;   // on the bridge
            // (Quays, jetties and ships stand out over the sea.)
            if (asset.model_path.find("/civic/") != std::string::npos && low < 0.5F) continue;
            const float hanging = asset.position.y - low, sunk = high - asset.position.y;
            // A building a metre clear of the land on a side, or the land a metre and a half up its walls.
            if (hanging < 1.0F && sunk < 1.5F) continue;
            ++misplaced;
            if (log()) std::cout << "  " << (hanging >= 1.0F ? "hanging " : "sunk ") << asset.id << " (" << asset.model_path << ") at " << asset.position.x << ","
                                 << asset.position.y << "," << asset.position.z << ": land from " << low << " to " << high << ", " << nearest << " m from a road\n";
        }
    std::cout << on_road << " things on a road, " << crossings << " crossings with no junction, " << misplaced << " things hanging or sunk by a road\n";
    check(on_road == 0, "nothing stands on a carriageway: " + std::to_string(on_road));
    check(crossings == 0, "no road runs through another: " + std::to_string(crossings));
    check(misplaced == 0, "nothing by a road hangs over the land or sinks into it: " + std::to_string(misplaced));
    if (failures == 0) std::cout << "road conflicts passed\n";
    return failures == 0 ? 0 : 1;
}
