#pragma once

namespace outland::world::terrain {

class TerrainHeight {
public:
    // The game has one active world. Set before constructing terrain meshes.
    static void set_coastal_layout(bool enabled);
    [[nodiscard]] static bool coastal_layout();
    [[nodiscard]]
    static float sample(
        float world_x,
        float world_z
    );
    // How far (0..1) the building pads (towns, hamlets, ranches) level the land here.
    [[nodiscard]]
    static float pad_weight(float world_x, float world_z);

private:
    // The land before the highways' roadbeds are cut and filled.
    [[nodiscard]]
    static float unpaved(float world_x, float world_z);

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
