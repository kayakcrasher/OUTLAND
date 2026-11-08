#include "outland/engine/render/ViewFrustum.hpp"
#include "outland/world/terrain/TerrainWorld.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/MountainRoad.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace outland::world::terrain {
TerrainWorld::TerrainWorld() { build_initial_region(); }
int TerrainWorld::resolution_for(int chunk_x, int chunk_z, int ring) {
    if (ring > kRadius) return kFarResolution;
    if (!TerrainHeight::coastal_layout()) return kResolution;
    // The chunks a highway's roadbed and its banks reach (within 45 m of its line), an
    // interchange's (its roundabout and slip roads) and La Ringo's pads.
    static const std::unordered_set<long long> roads = [] {
        std::unordered_set<long long> out;
        const auto key = [](int x, int z) { return (static_cast<long long>(x) << 32) ^ static_cast<unsigned>(z); };
        const auto mark = [&](float px, float pz, float reach) {
            for (int x = static_cast<int>(std::floor((px - reach) / kChunkSize)); x <= static_cast<int>(std::floor((px + reach) / kChunkSize)); ++x)
                for (int z = static_cast<int>(std::floor((pz - reach) / kChunkSize)); z <= static_cast<int>(std::floor((pz + reach) / kChunkSize)); ++z)
                    out.insert(key(x, z));
        };
        for (const auto& line : highways::lines())
            for (std::size_t i = 0; i < line.points.size(); ++i) {
                if (!line.loop && i + 1 == line.points.size()) {mark(line.points[i].x, line.points[i].z, 45); break;}
                const Vector3 a = line.points[i], b = line.points[(i + 1) % line.points.size()];
                const float length = std::hypot(b.x - a.x, b.z - a.z);
                for (float t = 0; t < std::max(length, .01F); t += 16) mark(a.x + (b.x - a.x) * t / length, a.z + (b.z - a.z) * t / length, 45);
            }
        for (const auto& x : highways::interchanges()) {
            for (const auto& slip : x.slips) for (const auto& p : slip.points) mark(p.x, p.z, 45);
            mark(x.at.x, x.at.z, highways::roundabout_pad + 30);
        }
        for (const auto& rest : highways::rest_areas()) mark(rest.centre.x, rest.centre.z, highways::rest_area_flat + highways::rest_area_blend);
        // Sovaĝa Monto's road, its pullouts and Supra Sovaĝejo: the cuttings and banks beside them.
        for (const auto& p : mountain::road().points) mark(p.x, p.z, mountain::bed_half + mountain::pullout_bench + 30);
        mark(mountain::hamlet().centre.x, mountain::hamlet().centre.z, mountain::hamlet_flat + mountain::hamlet_blend);
        return out;
    }();
    return roads.contains((static_cast<long long>(chunk_x) << 32) ^ static_cast<unsigned>(chunk_z)) ? kRoadResolution : kResolution;
}
void TerrainWorld::build_initial_region() {
    chunks_.clear();
    coastal_layout_ = TerrainHeight::coastal_layout();
    // Startup and teleports use the same bounded nearest-first streaming queue.
    update({0,0,0});
}
void TerrainWorld::update(Vector3 position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.z))
        return;
    const double cx = std::floor(static_cast<double>(position.x) / kChunkSize),
                 cz = std::floor(static_cast<double>(position.z) / kChunkSize);
    constexpr double limit = static_cast<double>(std::numeric_limits<int>::max()) - far_radius - 1;
    if (std::abs(cx) > limit || std::abs(cz) > limit)
        return;
    const int x = static_cast<int>(cx), z = static_cast<int>(cz);
    const bool geography_changed = coastal_layout_ != TerrainHeight::coastal_layout();
    const std::size_t wanted = static_cast<std::size_t>((2 * radius_ + 1) * (2 * radius_ + 1));
    if (x == center_x_ && z == center_z_ && !geography_changed && chunks_.size() == wanted)
        return;
    if (geography_changed) {
        chunks_.clear();
        coastal_layout_ = TerrainHeight::coastal_layout();
    }
    center_x_ = x;
    center_z_ = z;
    // Out of range, or built coarse for a far ring that is now near: built again.
    std::erase_if(chunks_, [&](const auto &c) {
        const int ring = static_cast<int>(std::max(std::abs(static_cast<double>(c->chunk_x()) - x), std::abs(static_cast<double>(c->chunk_z()) - z)));
        return ring > radius_ || (ring <= kRadius && c->resolution() != resolution_for(c->chunk_x(), c->chunk_z(), ring));
    });
    int built=0;
    for(int ring=0;ring<=radius_;++ring)
        for(int dz=-ring;dz<=ring;++dz) for(int dx=-ring;dx<=ring;++dx) {
            if(std::max(std::abs(dx),std::abs(dz))!=ring) continue;
            const int ix=x+dx,iz=z+dz;
            const bool exists=std::any_of(chunks_.begin(),chunks_.end(),[&](const auto& c){return c->chunk_x()==ix && c->chunk_z()==iz;});
            if(!exists) {
                chunks_.push_back(std::make_unique<TerrainChunk>(ix,iz,kChunkSize,resolution_for(ix,iz,ring)));
                if(++built>=build_budget) return;
            }
        }
}
void TerrainWorld::draw(const engine::ViewFrustum* view) const {
    for (const auto &c : chunks_)
        if(!view || view->sphere({(c->chunk_x()+.5F)*kChunkSize,0,(c->chunk_z()+.5F)*kChunkSize},140)) c->draw();
    if (TerrainHeight::coastal_layout()) {
        // Opaque ocean tiles only near the shore; use the same streamed footprint.
        for (const auto &c : chunks_) {
            const float x = c->chunk_x() * kChunkSize, z = c->chunk_z() * kChunkSize;
            if (std::hypot(x + kChunkSize * .5F, z + kChunkSize * .5F) > 1800)
                DrawPlane({x + kChunkSize * .5F, layout::sea_level, z + kChunkSize * .5F},
                          {kChunkSize, kChunkSize}, {45, 104, 139, 255});
        }
    }
}
float TerrainWorld::height_at(float x, float z) const { return TerrainHeight::sample(x, z); }
int TerrainWorld::chunk_count() const { return static_cast<int>(chunks_.size()); }
} // namespace outland::world::terrain
