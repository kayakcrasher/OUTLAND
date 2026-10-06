#pragma once

#include <raylib.h>

namespace outland::world::terrain {

class TerrainChunk {
public:
    TerrainChunk(
        int chunk_x,
        int chunk_z,
        float size = 128.0F,
        int resolution = 24
    );

    ~TerrainChunk();

    TerrainChunk(
        const TerrainChunk&
    ) = delete;

    TerrainChunk& operator=(
        const TerrainChunk&
    ) = delete;

    TerrainChunk(
        TerrainChunk&&
    ) = delete;

    TerrainChunk& operator=(
        TerrainChunk&&
    ) = delete;

    void draw() const;

    [[nodiscard]]
    int chunk_x() const;

    [[nodiscard]]
    int chunk_z() const;

    [[nodiscard]]
    float size() const;

private:
    int chunk_x_{0};
    int chunk_z_{0};

    float size_{128.0F};
    int resolution_{24};

    Model model_{};
    bool loaded_{false};

    void build();
};

} // namespace outland::world::terrain
