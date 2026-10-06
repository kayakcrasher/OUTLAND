#pragma once

namespace outland::world::terrain {

class TerrainHeight {
public:
    [[nodiscard]]
    static float sample(
        float world_x,
        float world_z
    );

private:
    [[nodiscard]]
    static float large_hills(
        float x,
        float z
    );

    [[nodiscard]]
    static float rolling_ground(
        float x,
        float z
    );

    [[nodiscard]]
    static float small_variation(
        float x,
        float z
    );
};

} // namespace outland::world::terrain
