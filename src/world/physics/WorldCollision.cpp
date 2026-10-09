#include "outland/world/physics/WorldCollision.hpp"

#include <algorithm>
#include <cmath>
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaLayout.hpp"

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
    // Shoreline blocks walkers until swimming exists; Creator flight remains independent.
    if(region.coastal_layout() && std::hypot(position.x,position.z)>1950 &&
       terrain::TerrainHeight::sample(position.x,position.z)<=layout::sea_level) return true;

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
            // OUTLAND buildings are playable spaces.
            // Match the modular shell rendered by VerdanArchitecture:
            // back wall, two side walls, and two front wall sections
            // separated by a real doorway.

            constexpr float wall_thickness = 0.24F;
            constexpr float doorway_width = 1.45F;

            const float half_width =
                building.size.x * 0.5F;

            const float half_depth =
                building.size.z * 0.5F;

            const float half_wall =
                wall_thickness * 0.5F;

            const float front_section_width =
                (building.size.x - doorway_width) * 0.5F;

            const float front_section_half =
                front_section_width * 0.5F;

            // Back wall.
            if (
                circle_hits_box(
                    local_x,
                    local_z,
                    player_radius,
                    0.0F,
                    half_depth,
                    half_width,
                    half_wall
                )
            ) {
                return true;
            }

            // Left wall.
            if (
                circle_hits_box(
                    local_x,
                    local_z,
                    player_radius,
                    -half_width,
                    0.0F,
                    half_wall,
                    half_depth
                )
            ) {
                return true;
            }

            // Right wall.
            if (
                circle_hits_box(
                    local_x,
                    local_z,
                    player_radius,
                    half_width,
                    0.0F,
                    half_wall,
                    half_depth
                )
            ) {
                return true;
            }

            // ------------------------------------------------
            // FRONT WALL / WINDOW SILLS
            // ------------------------------------------------
            //
            // The visual shell contains two real windows.
            // At walking height their sills remain physical
            // obstacles. The vault system will deliberately
            // allow the player to cross these zones.

            constexpr float window_width = 1.25F;

            const float window_x =
                building.size.x * 0.29F;

            const float left_window_left =
                -window_x - window_width * 0.5F;

            const float left_window_right =
                -window_x + window_width * 0.5F;

            const float right_window_left =
                window_x - window_width * 0.5F;

            const float right_window_right =
                window_x + window_width * 0.5F;

            auto front_wall_hit =
                [&](float x_min, float x_max) {

                    if (x_max <= x_min) {
                        return false;
                    }

                    return circle_hits_box(
                        local_x,
                        local_z,
                        player_radius,
                        (x_min + x_max) * 0.5F,
                        -half_depth,
                        (x_max - x_min) * 0.5F,
                        half_wall
                    );
                };

            // Far left wall.
            if (
                front_wall_hit(
                    -half_width,
                    left_window_left
                )
            ) {
                return true;
            }

            // Left window sill.
            if (
                front_wall_hit(
                    left_window_left,
                    left_window_right
                )
            ) {
                return true;
            }

            // Wall between left window and doorway.
            if (
                front_wall_hit(
                    left_window_right,
                    -doorway_width * 0.5F
                )
            ) {
                return true;
            }

            // Doorway intentionally has NO collision.

            // Wall between doorway and right window.
            if (
                front_wall_hit(
                    doorway_width * 0.5F,
                    right_window_left
                )
            ) {
                return true;
            }

            // Right window sill.
            if (
                front_wall_hit(
                    right_window_left,
                    right_window_right
                )
            ) {
                return true;
            }

            // Far right wall.
            if (
                front_wall_hit(
                    right_window_right,
                    half_width
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


bool WorldCollision::window_vault_target(
    Vector3 position,
    Vector3 forward,
    const VerdaRegion& region,
    Vector3& landing_position
) {
    constexpr float window_width = 1.25F;

    // How close the player must be to the window wall.
    constexpr float vault_reach = 1.35F;

    // Distance placed beyond the wall after vaulting.
    // Keep the player's body and first-person camera comfortably
        // clear of the wall after crossing the window opening.
        constexpr float landing_clearance = 1.65F;

    // Require the player to actually face toward the wall.
    constexpr float minimum_facing = 0.30F;

    for (const Settlement& settlement : region.settlements()) {
        for (const Building& building : settlement.buildings) {
            const float angle =
                building.rotation_y * DEG2RAD;

            const float cosine =
                std::cos(angle);

            const float sine =
                std::sin(angle);

            const float dx =
                position.x - building.position.x;

            const float dz =
                position.z - building.position.z;

            // World -> building-local coordinates.
            const float local_x =
                dx * cosine -
                dz * sine;

            const float local_z =
                dx * sine +
                dz * cosine;

            const float half_depth =
                building.size.z * 0.5F;

            // Windows currently live on the front wall.
            const float front_z =
                -half_depth;

            if (
                std::abs(local_z - front_z) >
                vault_reach
            ) {
                continue;
            }

            const float window_x =
                building.size.x * 0.29F;

            const float half_window =
                window_width * 0.5F;

            const bool at_left_window =
                std::abs(local_x + window_x) <=
                half_window;

            const bool at_right_window =
                std::abs(local_x - window_x) <=
                half_window;

            if (
                !at_left_window &&
                !at_right_window
            ) {
                continue;
            }

            // Transform facing direction into building-local space.
            const float local_forward_z =
                forward.x * sine +
                forward.z * cosine;

            // Determine whether we're outside or inside.
            const bool outside =
                local_z < front_z;

            // Outside player must face inward (+local Z).
            // Inside player must face outward (-local Z).
            const float required_direction =
                outside ? 1.0F : -1.0F;

            if (
                local_forward_z *
                required_direction <
                minimum_facing
            ) {
                continue;
            }

            // Preserve the player's X alignment with the window,
            // but move them safely to the opposite side.
            const float landing_local_x =
                local_x;

            const float landing_local_z =
                outside
                    ? front_z + landing_clearance
                    : front_z - landing_clearance;

            // Building-local -> world coordinates.
            const float landing_dx =
                landing_local_x * cosine +
                landing_local_z * sine;

            const float landing_dz =
                -landing_local_x * sine +
                landing_local_z * cosine;

            landing_position = {
                building.position.x + landing_dx,
                position.y,
                building.position.z + landing_dz
            };

            return true;
        }
    }

    return false;
}

} // namespace outland::world::physics
