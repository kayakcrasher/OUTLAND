#include "outland/world/terrain/TerrainWorld.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace outland::world::terrain {
TerrainWorld::TerrainWorld() { build_initial_region(); }
void TerrainWorld::build_initial_region() {
    chunks_.clear();
    coastal_layout_ = TerrainHeight::coastal_layout();
    for (int z = -kRadius; z <= kRadius; ++z)
        for (int x = -kRadius; x <= kRadius; ++x)
            chunks_.push_back(std::make_unique<TerrainChunk>(x, z, kChunkSize, kResolution));
}
void TerrainWorld::update(Vector3 position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.z))
        return;
    const double cx = std::floor(static_cast<double>(position.x) / kChunkSize),
                 cz = std::floor(static_cast<double>(position.z) / kChunkSize);
    constexpr double limit = static_cast<double>(std::numeric_limits<int>::max()) - kRadius - 1;
    if (std::abs(cx) > limit || std::abs(cz) > limit)
        return;
    const int x = static_cast<int>(cx), z = static_cast<int>(cz);
    const bool geography_changed = coastal_layout_ != TerrainHeight::coastal_layout();
    if (x == center_x_ && z == center_z_ && !geography_changed)
        return;
    if (geography_changed) {
        chunks_.clear();
        coastal_layout_ = TerrainHeight::coastal_layout();
    }
    center_x_ = x;
    center_z_ = z;
    std::erase_if(chunks_, [&](const auto &c) {
        return std::abs(static_cast<double>(c->chunk_x()) - x) > kRadius ||
               std::abs(static_cast<double>(c->chunk_z()) - z) > kRadius;
    });
    for (int iz = z - kRadius; iz <= z + kRadius; ++iz)
        for (int ix = x - kRadius; ix <= x + kRadius; ++ix) {
            const bool exists = std::any_of(chunks_.begin(), chunks_.end(), [&](const auto &c) {
                return c->chunk_x() == ix && c->chunk_z() == iz;
            });
            if (!exists)
                chunks_.push_back(std::make_unique<TerrainChunk>(ix, iz, kChunkSize, kResolution));
        }
}
void TerrainWorld::draw() const {
    for (const auto &c : chunks_)
        c->draw();
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
