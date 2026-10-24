#pragma once
#include "outland/world/VerdaRegion.hpp"
#include <raylib.h>
#include <array>
#include <cmath>

// Parkumejo Centra, downtown Verda City's multi-storey car park (tools/generate_city_kit.py
// parking_garage(); placed by the world kit in the south-west corner block). A ground floor, two
// decks and the roof, all driven up: cars ride on its decks and ramps (VehicleSystem::bind_surface),
// not on the terrain under it. On the roof: a three-storey mansion for sale in Explore
// (game::economy::SkyMansion) and its own parking bay at the door.
//
// Model frame: front -Z, +X toward (cos yaw, -sin yaw) in the world, like every kit model.
namespace outland::world::garage {

inline constexpr const char* model = "assets/verda/city/civic/parking_garage.glb";
inline constexpr float half_x = 19.0F, half_z = 16.0F;
inline constexpr std::array<float, 4> decks{0.1F, 3.3F, 6.5F, 9.7F};   // the levels' tops (the last: the roof)
inline constexpr float roof = decks[3];
inline constexpr float mansion_x0 = -18.0F, mansion_x1 = -3.2F, mansion_z0 = -3.0F, mansion_z1 = 15.2F;
inline constexpr float mansion_storey = 3.2F, mansion_wall = .25F;
inline constexpr float bay_x0 = -12.2F, bay_x1 = -9.0F, bay_z0 = -10.8F, bay_z1 = -5.2F;   // the mansion's own parking bay
inline constexpr Vector3 bed{-14.0F, roof + 2 * mansion_storey, 10.8F};
inline constexpr Vector3 wake{-14.0F, roof + 2 * mansion_storey, 8.4F};
inline constexpr Vector3 door{-10.6F, roof, -3.6F};                   // just outside the front door

struct Site {
    Vector3 origin{};
    float yaw{0};
    [[nodiscard]] Vector3 to_world(Vector3 local) const {
        const float a = yaw * DEG2RAD;
        return {origin.x + local.x * std::cos(a) + local.z * std::sin(a), origin.y + local.y,
                origin.z - local.x * std::sin(a) + local.z * std::cos(a)};
    }
    [[nodiscard]] Vector3 to_local(Vector3 world) const {
        const float a = yaw * DEG2RAD, dx = world.x - origin.x, dz = world.z - origin.z;
        return {dx * std::cos(a) - dz * std::sin(a), world.y - origin.y, dx * std::sin(a) + dz * std::cos(a)};
    }
    // Within the garage's walls (plus `margin`), any level.
    [[nodiscard]] bool over(Vector3 world, float margin = 0) const {
        const auto p = to_local(world);
        return std::abs(p.x) < half_x + margin && std::abs(p.z) < half_z + margin;
    }
    // A car (its centre) parked in the mansion's bay on the roof.
    [[nodiscard]] bool in_bay(Vector3 world) const {
        const auto p = to_local(world);
        return p.x > bay_x0 && p.x < bay_x1 && p.z > bay_z0 && p.z < bay_z1 && std::abs(p.y - roof) < 1.5F;
    }
    // Feet inside the mansion, any of its floors.
    [[nodiscard]] bool in_mansion(Vector3 feet) const {
        const auto p = to_local(feet);
        return p.x > mansion_x0 + mansion_wall && p.x < mansion_x1 - mansion_wall && p.z > mansion_z0 + mansion_wall &&
               p.z < mansion_z1 - mansion_wall && p.y > roof - .6F && p.y < roof + 3 * mansion_storey;
    }
};

// The garage in the world, if it is built.
[[nodiscard]] inline bool find(const VerdaRegion& region, Site& out) {
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets)
            if (asset.model_path == model) { out = {asset.position, asset.rotation_y}; return true; }
    return false;
}

} // namespace outland::world::garage
