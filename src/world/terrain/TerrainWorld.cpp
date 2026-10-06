#include "outland/world/terrain/TerrainWorld.hpp"

#include "outland/world/terrain/TerrainHeight.hpp"

#include <memory>

namespace outland::world::terrain {

TerrainWorld::TerrainWorld() {
    build_initial_region();
}

void TerrainWorld::build_initial_region() {
    chunks_.clear();

    /*
     * 5 x 5 development region.
     *
     * 5 chunks * 128 meters =
     * 640 x 640 meters of terrain.
     *
     * Later this becomes dynamic streaming
     * centered around the player.
     */
    for (
        int z = -kRadius;
        z <= kRadius;
        ++z
    ) {
        for (
            int x = -kRadius;
            x <= kRadius;
            ++x
        ) {
            chunks_.push_back(
                std::make_unique<TerrainChunk>(
                    x,
                    z,
                    kChunkSize,
                    kResolution
                )
            );
        }
    }
}

void TerrainWorld::draw() const {
    for (
        const auto& chunk :
        chunks_
    ) {
        chunk->draw();
    }
}

float TerrainWorld::height_at(
    const float world_x,
    const float world_z
) const {
    return TerrainHeight::sample(
        world_x,
        world_z
    );
}

int TerrainWorld::chunk_count() const {
    return static_cast<int>(
        chunks_.size()
    );
}

} // namespace outland::world::terrain
