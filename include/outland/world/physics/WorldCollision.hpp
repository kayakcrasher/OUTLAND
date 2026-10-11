#pragma once

#include <raylib.h>
#include <string_view>

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

    // The static-asset spatial index rebuilds itself when assets are added or removed; call this
    // after moving or reshaping assets in place (Creator move/rotate, undo, load).
    static void invalidate_index();

    // Shared occupancy query for collision and vegetation exclusion.
    static bool blocked(
        Vector3 position,
        const VerdaRegion& region,
        float player_radius,
        std::string_view ignore_asset = {}
    );

    // For vehicles: like blocked(), but light street furniture (fences, bins, cones, benches,
    // pallets) does not stop a car - it is brushed aside. Walls, trees, posts, bollards, barriers
    // and other vehicles still do.
    static bool vehicle_blocked(
        Vector3 position,
        const VerdaRegion& region,
        float radius,
        std::string_view ignore_asset = {}
    );
    static bool light_prop(const WorldAsset& asset);

    // Height-aware queries for walking bodies (player, NPCs). Model assets use their real
    // triangles (doorways, window openings, interiors); everything else matches blocked().
    static bool body_blocked(
        Vector3 position,
        float feet_y,
        const VerdaRegion& region,
        float player_radius,
        std::string_view ignore_asset = {}
    );
    static Vector3 resolve_body_movement(
        Vector3 current_position,
        Vector3 desired_position,
        float feet_y,
        const VerdaRegion& region,
        float player_radius = 0.45F
    );
    // Terrain or the highest model floor/stair at most `step` above the feet.
    static float ground_height(
        Vector3 position,
        float feet_y,
        const VerdaRegion& region,
        float step = 0.5F
    );
    // Vault through a window opening in a model building (sill blocks, opening above is clear).
    static bool mesh_vault_target(
        Vector3 position,
        float feet_y,
        Vector3 forward,
        const VerdaRegion& region,
        Vector3& landing_position
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
    static bool blocked_impl(
        Vector3 position,
        const VerdaRegion& region,
        float player_radius,
        std::string_view ignore_asset,
        const float* feet_y,
        bool vehicle = false
    );
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
