#pragma once
// Kristala Turo, Verda's tallest building (the world kit's sky_tower()): a glass skyscraper on the
// block east of the highway at the foot of Norda Kvartalo, its front and its plaza to the south,
// toward downtown. Its lobby is open to everyone; an elevator in the core goes straight up to the
// penthouse on the 25th floor - a glass box with a roof terrace and a pool - which is for sale
// (game::economy::SkyPenthouse).
//
// The model's numbers (tools/generate_city_kit.py kristala_turo()): local coordinates, the front
// at -Z, y up from the plaza's foot.
#include "outland/world/VerdaRegion.hpp"
#include <raylib.h>
#include <cmath>

namespace outland::world::tower {

inline constexpr const char* model = "assets/verda/city/towers/kristala_turo.glb";
inline constexpr Vector3 site{52, 0, -214};
inline constexpr float site_yaw = 180;              // the front (-Z) to the south

inline constexpr float half = 13;                   // the tower's walls, either way
inline constexpr float lobby_floor = .15F;           // the lobby's floor top
inline constexpr float lobby_height = 6;
inline constexpr float storey = 3.6F;
inline constexpr int office_floors = 23;
inline constexpr float penthouse_floor = lobby_height + office_floors * storey + .2F;   // its floor top (88.99 m)
inline constexpr float penthouse_height = 4.4F;
// The penthouse inside its glass walls, the terrace round it, its bed and the elevator.
inline constexpr float suite_x = 9.5F, suite_z0 = -7, suite_z1 = 11.5F;
inline constexpr float terrace = 11.5F;             // the terrace's balustrade, either way
inline constexpr Vector3 lobby_call{0, lobby_floor, 2.4F};            // in front of the elevator doors
inline constexpr Vector3 penthouse_call{0, penthouse_floor, 3.4F};
inline constexpr Vector3 bed{-6.4F, penthouse_floor, 8.2F};
inline constexpr Vector3 door{0, lobby_floor, -half};                 // the lobby's way in

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
    // Feet on the penthouse floor: in the suite or out on its terrace.
    [[nodiscard]] bool in_penthouse(Vector3 feet) const {
        const auto p = to_local(feet);
        return std::abs(p.x) < terrace && std::abs(p.z) < terrace && p.y > penthouse_floor - .6F && p.y < penthouse_floor + penthouse_height;
    }
    [[nodiscard]] bool in_lobby(Vector3 feet) const {
        const auto p = to_local(feet);
        return std::abs(p.x) < half && std::abs(p.z) < half && p.y > -.5F && p.y < lobby_height - .5F;
    }
    // Standing at the elevator's doors (`up`: in the lobby; else up in the penthouse).
    [[nodiscard]] bool at_elevator(Vector3 feet, bool& up) const {
        const auto p = to_local(feet);
        const bool lobby = std::hypot(p.x - lobby_call.x, p.z - lobby_call.z) < 2.2F && std::abs(p.y - lobby_call.y) < 1.2F;
        const bool top = std::hypot(p.x - penthouse_call.x, p.z - penthouse_call.z) < 2.2F && std::abs(p.y - penthouse_call.y) < 1.2F;
        up = lobby;
        return lobby || top;
    }
};

// The tower in the world, if it is built (its design origin: the kit stores a city model at its
// bounds' centre, and the tower's bounds are centred on it but for the plaza, so it is found by
// its model and put back on the site).
[[nodiscard]] inline bool find(const VerdaRegion& region, Site& out) {
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets)
            if (asset.model_path == model) { out = {{site.x, asset.position.y, site.z}, asset.rotation_y}; return true; }
    return false;
}

} // namespace outland::world::tower
