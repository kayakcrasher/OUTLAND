#include "outland/world/physics/WorldCollision.hpp"

#include <algorithm>
#include <cmath>
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/physics/MeshCollision.hpp"

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
    const float player_radius,
    const std::string_view ignore_asset
) {
    return blocked_impl(position, region, player_radius, ignore_asset, nullptr);
}

bool WorldCollision::body_blocked(
    const Vector3 position,
    const float feet_y,
    const VerdaRegion& region,
    const float player_radius,
    const std::string_view ignore_asset
) {
    return blocked_impl(position, region, player_radius, ignore_asset, &feet_y);
}

namespace {
// Body volume above step height: anything lower than 0.5 m is stepped onto, not walked into.
// Against real geometry the torso is 0.32 m: a 1 m doorway or a stair between side rails fits.
bool mesh_body_blocked(const CollisionMesh& mesh, Vector3 local, float local_feet, float radius) {
    radius = std::min(radius, .32F);
    for (const float height : {0.85F, 1.3F, 1.7F})
        if (mesh.sphere_blocked({local.x, local_feet + height, local.z}, radius)) return true;
    return false;
}
}

bool WorldCollision::blocked_impl(
    const Vector3 position,
    const VerdaRegion& region,
    const float player_radius,
    const std::string_view ignore_asset,
    const float* feet_y
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
            if(std::abs(position.x-building.position.x)>building.size.x+building.size.z+player_radius || std::abs(position.z-building.position.z)>building.size.x+building.size.z+player_radius)continue;
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
            if(asset.id==ignore_asset)continue;
            if(!asset.vehicle.definition.empty() || (!asset.model_path.empty() && asset.collision)) {
                if(!asset.vehicle.definition.empty() && !asset.vehicle.enabled)continue;
                const float dx=position.x-asset.position.x,dz=position.z-asset.position.z;
                const float reach=asset.size.x+asset.size.z+player_radius;if(std::abs(dx)>reach||std::abs(dz)>reach)continue;
                const float yaw=asset.rotation_y*DEG2RAD;
                // Walkers use the asset's real shape: doorways, windows and hollow interiors.
                if(feet_y && asset.vehicle.definition.empty())if(const auto* mesh=MeshCollisionLibrary::get(asset.model_path)) {
                    if(mesh_body_blocked(*mesh,{dx*std::cos(yaw)-dz*std::sin(yaw),0,dx*std::sin(yaw)+dz*std::cos(yaw)},*feet_y-asset.position.y,player_radius))return true;
                    continue;
                }
                if(circle_hits_box(dx*std::cos(yaw)-dz*std::sin(yaw),dx*std::sin(yaw)+dz*std::cos(yaw),player_radius,0,0,asset.size.x*.5F,asset.size.z*.5F))return true;
                continue;
            }
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


Vector3 WorldCollision::resolve_body_movement(
    const Vector3 current_position,
    const Vector3 desired_position,
    const float feet_y,
    const VerdaRegion& region,
    const float player_radius
) {
    Vector3 resolved = current_position;
    Vector3 test_x = resolved; test_x.x = desired_position.x;
    if (!body_blocked(test_x, feet_y, region, player_radius)) resolved.x = desired_position.x;
    Vector3 test_z = resolved; test_z.z = desired_position.z;
    if (!body_blocked(test_z, feet_y, region, player_radius)) resolved.z = desired_position.z;
    return resolved;
}

float WorldCollision::ground_height(
    const Vector3 position,
    const float feet_y,
    const VerdaRegion& region,
    const float step
) {
    float ground = terrain::TerrainHeight::sample(position.x, position.z);
    for (const Settlement& settlement : region.settlements()) for (const WorldAsset& asset : settlement.assets) {
        // Anything model-backed can be stood on (road pieces, floor slabs), whatever its blocker flag.
        if (asset.model_path.empty() || !asset.vehicle.definition.empty()) continue;
        const float dx = position.x - asset.position.x, dz = position.z - asset.position.z;
        const float reach = asset.size.x + asset.size.z + 2;
        if (std::abs(dx) > reach || std::abs(dz) > reach) continue;
        const auto* mesh = MeshCollisionLibrary::get(asset.model_path);
        if (!mesh) continue;
        const float yaw = asset.rotation_y * DEG2RAD;
        float floor = 0;
        if (mesh->floor_below(dx * std::cos(yaw) - dz * std::sin(yaw), dx * std::sin(yaw) + dz * std::cos(yaw),
                feet_y + step - asset.position.y, floor))
            ground = std::max(ground, floor + asset.position.y);
    }
    return ground;
}

bool WorldCollision::mesh_vault_target(
    const Vector3 position,
    const float feet_y,
    Vector3 forward,
    const VerdaRegion& region,
    Vector3& landing_position
) {
    forward.y = 0;
    const float length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    if (length < .01F) return false;
    forward = {forward.x / length, 0, forward.z / length};
    // Only vault when something waist-high blocks the way but the chest-high gap is open.
    if (!body_blocked({position.x + forward.x * .6F, position.y, position.z + forward.z * .6F}, feet_y, region, .45F)) return false;
    for (const Settlement& settlement : region.settlements()) for (const WorldAsset& asset : settlement.assets) {
        if (asset.model_path.empty() || !asset.collision || !asset.vehicle.definition.empty()) continue;
        const auto* mesh = MeshCollisionLibrary::get(asset.model_path);
        if (!mesh) continue;
        const float dx = position.x - asset.position.x, dz = position.z - asset.position.z;
        const float reach = asset.size.x + asset.size.z + 3;
        if (std::abs(dx) > reach || std::abs(dz) > reach) continue;
        const float yaw = asset.rotation_y * DEG2RAD, c = std::cos(yaw), s = std::sin(yaw);
        const Vector3 local{dx * c - dz * s, feet_y - asset.position.y, dx * s + dz * c};
        const Vector3 local_forward{forward.x * c - forward.z * s, 0, forward.x * s + forward.z * c};
        for (float distance = 1.2F; distance <= 2.21F; distance += .25F) {
            // The chest-height path must pass through an opening (no wall at 1.25-1.75 m).
            bool clear = true;
            for (float along = .2F; along <= distance && clear; along += .2F)
                clear = !mesh->sphere_blocked({local.x + local_forward.x * along, local.y + 1.5F, local.z + local_forward.z * along}, .22F);
            if (!clear) break;
            const Vector3 landing{position.x + forward.x * distance, position.y, position.z + forward.z * distance};
            if (!body_blocked(landing, feet_y, region, .45F)) {landing_position = landing; return true;}
        }
    }
    return false;
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

// Lives here (not MeshCollision.cpp) so the mesh loader stays free of world dependencies.
void MeshCollisionLibrary::preload(const VerdaRegion& region) {
    for (const auto& settlement : region.settlements())
        for (const auto& asset : settlement.assets)
            if (asset.collision && asset.vehicle.definition.empty()) get(asset.model_path);
}
} // namespace outland::world::physics
