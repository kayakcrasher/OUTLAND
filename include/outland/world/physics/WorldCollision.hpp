#pragma once

#include <raylib.h>

#include "outland/world/VerdaRegion.hpp"

namespace outland::world::physics {

class WorldCollision {
public:
    static Vector3 resolve_player_movement(
        Vector3 current_position,
        Vector3 desired_position,
        const VerdaRegion& region,
        float player_radius = 0.45F
    );

    // Shared occupancy query for collision and vegetation exclusion.
    static bool blocked(
        Vector3 position,
        const VerdaRegion& region,
        float player_radius
    );

    // Find a nearby window that the player can intentionally vault.
    // Returns true and writes the landing position when a valid
    // front-window opening is within reach.
    static bool window_vault_target(
        Vector3 position,
        Vector3 forward,
        const VerdaRegion& region,
        Vector3& landing_position
    );

private:
    static bool circle_hits_box(
        float player_x,
        float player_z,
        float player_radius,
        float box_x,
        float box_z,
        float half_width,
        float half_depth
    );
};

} // namespace outland::world::physics
