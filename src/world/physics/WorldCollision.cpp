#include "outland/world/physics/WorldCollision.hpp"

#include <algorithm>
#include <cmath>

namespace outland::world::physics {

bool WorldCollision::circle_hits_box(
    const float player_x,
    const float player_z,
    const float player_radius,
    const float box_x,
    const float box_z,
    const float half_width,
    const float half_depth
) {
    const float nearest_x =
        std::clamp(
            player_x,
            box_x - half_width,
            box_x + half_width
        );

    const float nearest_z =
        std::clamp(
            player_z,
            box_z - half_depth,
            box_z + half_depth
        );

    const float dx =
        player_x - nearest_x;

    const float dz =
        player_z - nearest_z;

    return
        dx * dx +
        dz * dz <
        player_radius *
        player_radius;
}

bool WorldCollision::blocked(
    const Vector3 position,
    const VerdaRegion& region,
    const float player_radius
) {
    for (
        const Settlement& settlement :
        region.settlements()
    ) {
        // ----------------------------------------------------
        // BUILDINGS
        // ----------------------------------------------------

        for (
            const Building& building :
            settlement.buildings
        ) {
            const float angle = building.rotation_y * DEG2RAD;
            const float dx = position.x - building.position.x;
            const float dz = position.z - building.position.z;
            // Match the rendered house's local coordinates for any yaw.
            const float local_x = dx * std::cos(angle) - dz * std::sin(angle);
            const float local_z = dx * std::sin(angle) + dz * std::cos(angle);
            if (
                circle_hits_box(
                    local_x,
                    local_z,
                    player_radius,

                    0.0F,
                    0.0F,

                    building.size.x * 0.5F,
                    building.size.z * 0.5F
                )
            ) {
                return true;
            }
        }

        // ----------------------------------------------------
        // WORLD ASSETS
        // ----------------------------------------------------

        for (
            const WorldAsset& asset :
            settlement.assets
        ) {
            if (!asset.collision) {
                continue;
            }

            float radius = 0.75F;

            if (
                asset.type ==
                AssetType::Tree
            ) {
                radius = 0.55F;
            }

            const float dx =
                position.x -
                asset.position.x;

            const float dz =
                position.z -
                asset.position.z;

            const float combined_radius =
                player_radius +
                radius;

            if (
                dx * dx +
                dz * dz <
                combined_radius *
                combined_radius
            ) {
                return true;
            }
        }
    }

    return false;
}

Vector3 WorldCollision::resolve_player_movement(
    const Vector3 current_position,
    const Vector3 desired_position,
    const VerdaRegion& region,
    const float player_radius
) {
    Vector3 resolved =
        current_position;

    /*
     * Resolve X and Z independently.
     *
     * This allows the player to slide along
     * walls instead of sticking to them.
     */

    Vector3 test_x =
        resolved;

    test_x.x =
        desired_position.x;

    if (
        !blocked(
            test_x,
            region,
            player_radius
        )
    ) {
        resolved.x =
            desired_position.x;
    }

    Vector3 test_z =
        resolved;

    test_z.z =
        desired_position.z;

    if (
        !blocked(
            test_z,
            region,
            player_radius
        )
    ) {
        resolved.z =
            desired_position.z;
    }

    return resolved;
}

} // namespace outland::world::physics
