#include "outland/world/terrain/TerrainHeight.hpp"

#include <cmath>

namespace outland::world::terrain {

float TerrainHeight::large_hills(
    const float x,
    const float z
) {
    const float broad_ridge =
        std::sin(x * 0.0065F) *
        std::cos(z * 0.0050F);

    const float diagonal_ridge =
        std::sin(
            (x + z) * 0.0032F
        );

    return
        broad_ridge * 8.0F +
        diagonal_ridge * 4.5F;
}

float TerrainHeight::rolling_ground(
    const float x,
    const float z
) {
    const float wave_x =
        std::sin(x * 0.021F);

    const float wave_z =
        std::cos(z * 0.018F);

    const float diagonal =
        std::sin(
            (x - z) * 0.013F
        );

    return
        wave_x * 2.0F +
        wave_z * 1.7F +
        diagonal * 1.1F;
}

float TerrainHeight::small_variation(
    const float x,
    const float z
) {
    return
        std::sin(x * 0.071F) *
        std::cos(z * 0.063F) *
        0.45F;
}

float TerrainHeight::sample(
    const float world_x,
    const float world_z
) {
    return
        large_hills(
            world_x,
            world_z
        ) +
        rolling_ground(
            world_x,
            world_z
        ) +
        small_variation(
            world_x,
            world_z
        );
}

} // namespace outland::world::terrain
