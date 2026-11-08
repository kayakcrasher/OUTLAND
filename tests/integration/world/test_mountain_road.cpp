// Sovaĝa Monto and its road (world/MountainRoad.hpp): the island's highest ground, and a dirt road
// from Sovaĝejo's trailer park over the mountain's shoulder to the ring road that the land meets
// exactly - level across, never steeper than its grade - with level pullouts and Supra Sovaĝejo's
// level pad; then the world map's pieces of it.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/MountainRoad.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
namespace mtn = outland::world::mountain;
using world::terrain::TerrainHeight;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

int main() {
    TerrainHeight::set_coastal_layout(true);
    const auto& road = mtn::road();
    // The road: out of the trailer park's north gate, about 3 km over the mountain to the ring road's verge.
    check(road.length() > 2800, "a road of some kilometres: " + std::to_string(road.length()));
    check(flat(road.points.front(), {world::layout::sovagejo_center.x, 0, 40}) < .5F, "it leaves the trailer park's north gate");
    const Vector3 end = road.points.back();
    const float ring = std::hypot(end.x, end.z) - 1500;
    check(ring > world::highways::paved_half && ring < world::highways::paved_half + 4, "it ends at the ring road's verge: " + std::to_string(ring));
    // The summit: the island's highest ground, near the road.
    const Vector3 top = mtn::summit();
    check(top.y > 250 && std::abs(TerrainHeight::sample(top.x, top.z) - top.y) < .5F, "Sovaĝa Monto stands high: " + std::to_string(top.y));
    float elsewhere = -1e9F, near_road = 1e9F, highest_road = -1e9F;
    for (float x = -2400; x <= 2400; x += 40)
        for (float z = -2400; z <= 2400; z += 40)
            if (std::hypot(x - top.x, z - top.z) > 400) elsewhere = std::max(elsewhere, TerrainHeight::sample(x, z));
    for (const auto& p : road.points) {near_road = std::min(near_road, flat(p, top)); highest_road = std::max(highest_road, p.y);}
    check(elsewhere < top.y - 60, "the highest mountain: nothing else comes near it (" + std::to_string(elsewhere) + ")");
    check(near_road < 260 && highest_road > 80, "the road climbs high, the summit close above it");
    // The land meets the road: on its profile, level across the 6 m roadbed, never steeper than its grade.
    float off = 0, across = 0, grade = 0;
    for (float s = 2; s < road.length() - 60; s += 2) {
        const Vector3 p = road.at(s), h = road.heading(s), left{-h.z, 0, h.x};
        const float y = TerrainHeight::sample(p.x, p.z);
        off = std::max(off, std::abs(y - p.y));
        across = std::max(across, std::abs(TerrainHeight::sample(p.x + left.x * 3, p.z + left.z * 3) - TerrainHeight::sample(p.x - left.x * 3, p.z - left.z * 3)));
        grade = std::max(grade, std::abs(TerrainHeight::sample(p.x + h.x * 2, p.z + h.z * 2) - y) / 2);
    }
    check(off < .05F, "the ground is the road's profile: " + std::to_string(off));
    check(across < .12F, "level across the road: " + std::to_string(across));
    check(grade < mtn::max_grade + .01F, "never steeper than its grade: " + std::to_string(grade));
    // The range keeps off the ring road and La Ringo's pad.
    for (float b = 175; b <= 255; b += .5F) {
        const float a = b * 3.14159265F / 180;
        check(mtn::massif(1540 * std::cos(a), 1540 * std::sin(a)) == 0, "no mountain under the ring road");
    }
    const auto& ringo = world::highways::brotherhood_home().centre;
    check(mtn::massif(ringo.x, ringo.z) == 0, "nor under La Ringo Okcidento");
    // The pullouts: each a level bench beside the road, at the road's height.
    check(mtn::pullouts().size() >= 5, "pullouts every few hundred metres");
    int lookouts = 0;
    for (const auto& p : mtn::pullouts()) {
        lookouts += p.lookout;
        for (const float a : {-18.0F, 0.0F, 18.0F}) for (const float l : {8.0F, 14.0F, 20.0F}) {
            const Vector3 q = p.at(a, l);
            check(std::abs(TerrainHeight::sample(q.x, q.z) - p.centre.y) < .05F, "a pullout's bench is level with the road at " + std::to_string(p.s));
        }
    }
    check(lookouts == 1, "one is the lookout under the summit");
    // Supra Sovaĝejo: a level pad off the road, reached by a level lane from the fork.
    const auto& up = mtn::hamlet();
    for (float a = -60; a <= 30; a += 6) {
        const Vector3 q = up.at(a, 0);
        check(std::abs(TerrainHeight::sample(q.x, q.z) - up.centre.y) < .05F, "Supra Sovaĝejo's lane and pad are level");
    }
    check(flat(up.fork, road.at(up.fork_s)) < .01F && flat(up.centre, world::layout::sovagejo_center) < 900, "the fork leaves the road, the hamlet is Sovaĝejo's");

    // The world map: the road's pieces (dirt) in Sovaĝejo, joined to the trailer park's lane and the
    // ring road; the lay-bys and the lane (gravel); six trailers in Supra Sovaĝejo, one at each pullout.
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    const world::Settlement* town = nullptr;
    for (const auto& s : region.settlements()) if (s.id == "ferarbaro_sovagejo") town = &s;
    check(town, "Sovaĝejo is on the map");
    bool gate = false, ringed = false;
    float covered = 0;
    for (const auto& r : town->roads) {
        if (r.type != world::RoadType::Dirt || r.width != mtn::road_width) continue;
        covered += flat(r.start, r.end);
        gate = gate || flat(r.start, road.points.front()) < .5F;
        ringed = ringed || std::abs(std::hypot(r.end.x, r.end.z) - 1500) < 1;
    }
    check(gate && ringed && covered > road.length() - 20, "the road's pieces, from the gate to the ring's centre line: " + std::to_string(covered));
    int upper = 0, pullout = 0;
    for (const auto& s : region.settlements()) for (const auto& asset : s.assets) {
        if (asset.model_path.find("/residential/trailers/") == std::string::npos) continue;
        upper += flat(asset.position, up.centre) < mtn::hamlet_flat;
        for (const auto& p : mtn::pullouts()) pullout += flat(asset.position, p.centre) < 30;
    }
    check(upper == 6, "six trailers in Supra Sovaĝejo: " + std::to_string(upper));
    check(pullout == static_cast<int>(mtn::pullouts().size()), "a trailer at every pullout: " + std::to_string(pullout));
    // The trailer park's north fence is open where the road leaves it.
    for (const auto& s : region.settlements()) for (const auto& asset : s.assets)
        check(!(asset.model_path.ends_with("Chainlink Fence.glb") && std::abs(asset.position.z - 38) < .6F &&
                std::abs(asset.position.x - world::layout::sovagejo_center.x) < 4), "the north gate is open");
    std::cout << "Sovaĝa Monto " << top.y << " m; road " << road.length() << " m up to " << highest_road << " m, " << mtn::pullouts().size()
              << " pullouts, Supra Sovaĝejo at " << up.centre.y << " m: GREEN\n";
}
