#pragma once

#include "outland/world/terrain/TerrainChunk.hpp"

#include <memory>
#include <vector>

namespace outland::world::terrain {

class TerrainWorld {
public:
    TerrainWorld();

    void draw() const;

    [[nodiscard]]
    float height_at(
        float world_x,
        float world_z
    ) const;

    [[nodiscard]]
    int chunk_count() const;

private:
    static constexpr int kRadius = 2;
    static constexpr float kChunkSize = 128.0F;
    static constexpr int kResolution = 24;

    std::vector<
        std::unique_ptr<TerrainChunk>
    > chunks_;

    void build_initial_region();
};

} // namespace outland::world::terrain
