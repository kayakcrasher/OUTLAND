#pragma once
#include "outland/world/VerdaRegion.hpp"
#include <raylib.h>
#include <array>
#include <cmath>
#include <string>

// La Verda Basballigo's ballparks (game::sports): one high-school-sized park in each county, near
// its main town, on a level pad (terrain::TerrainHeight), each in its team's colours
// (tools/generate_city_kit.py ballpark(); placed by the world kit, front toward the nearest road).
//
// Model frame (front -Z: the entrance, the car park and the bus bays): home plate toward the
// front, centre field toward +Z. Seen from behind the plate, first base is on the right (-X).
namespace outland::world::ballpark {

struct Park {
    int team;               // game::sports::teams index
    const char* site;       // the town (world::layout::sites)
    Vector3 centre;         // the pad's centre (the model's origin), on the map
};
// Chosen clear of every road, building and ranch (in each county, off its main town).
inline constexpr std::array<Park, 5> parks{{
    {0, "capital_verda", {606, 0, 46}},       // Verda City: the plot kept for it east of Orienta
    {1, "village_espera", {-256, 0, -1755}},     // west of Espera, toward the hills
    {2, "port_luma", {1630, 0, -186}},
    {3, "south_haven", {180, 0, 1586}},
    {4, "west_roka", {-1708, 0, 686}},
}};
inline constexpr float flat_radius = 74, flat_blend = 30;   // the level pad

inline std::string model(int team) { return "assets/verda/city/civic/ballpark_" + std::to_string(team) + ".glb"; }

// The diamond, in the model's frame (metres; y up from the pad).
inline constexpr Vector3 plate{0, 0, -36};
inline constexpr float base_path = 27.43F, mound_distance = 18.44F, fence_radius = 88;
inline constexpr Vector3 first_base{-19.4F, 0, -16.6F}, second_base{0, 0, 2.8F}, third_base{19.4F, 0, -16.6F};
inline constexpr Vector3 mound{0, .25F, -17.56F};
// The fielders' spots (pitcher, catcher, 1B, 2B, SS, 3B, LF, CF, RF).
inline constexpr std::array<Vector3, 9> fielders{{
    {0, .25F, -17.4F}, {0, 0, -37.6F}, {-17.5F, 0, -12.5F}, {-7.0F, 0, -2.0F}, {7.0F, 0, -2.0F},
    {17.5F, 0, -12.5F}, {30.0F, 0, 22.0F}, {0, 0, 34.0F}, {-30.0F, 0, 22.0F}}};
// The stands: home (first-base side, -X) and visitors (third-base side, +X), stepped rows facing
// the field (+Z). Row k's seat is at height (k + 1) * row_rise, z = stands_z - k * row_depth.
inline constexpr float stands_x0 = 22.0F, stands_x1 = 46.0F;   // |x| span of each side's stand
inline constexpr float stands_z = -41.0F, row_rise = .45F, row_depth = .8F;
inline constexpr int rows = 6;
inline constexpr float seat_spacing = .7F;
// The dugouts (home on the first-base side) and the home team's locker room behind its stand.
inline constexpr Vector3 home_dugout{-14.75F, 0, -41.0F}, away_dugout{14.75F, 0, -41.0F};
inline constexpr float locker_x0 = -44, locker_x1 = -30, locker_z0 = -62, locker_z1 = -52;
inline constexpr Vector3 locker_door{-37, 0, -62.6F};
// The bus bays in the car park (home, visitors) and the gate.
inline constexpr Vector3 home_bus{-16, 0, -68}, away_bus{16, 0, -68};
inline constexpr Vector3 gate{0, 0, -52};
inline constexpr Vector3 scoreboard{0, 6.5F, 60};

struct Site {
    int team{-1};
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
    // A seat in a stand: `home` side, row (0 front), place along it (0..seats_per_row).
    [[nodiscard]] Vector3 seat(bool home, int row, int place) const {
        const float x = (stands_x0 + .4F + place * seat_spacing) * (home ? -1.0F : 1.0F);
        return to_world({x, (row + 1) * row_rise, stands_z - row * row_depth - row_depth * .5F});
    }
    [[nodiscard]] float facing() const { return yaw; }   // the stands face +Z (local yaw 0)
};
inline constexpr int seats_per_row = static_cast<int>((stands_x1 - stands_x0 - .8F) / seat_spacing);

// The model's bounds (tools/generate_city_kit.py ballpark()): the renderer centres a model on
// them, so the world kit stores a park at the bounds' centre, not at its design origin.
inline constexpr Vector3 model_min{-68, -.6F, -76}, model_max{68, 19.6F, 64};

// The parks on the map (by their models), in team order; a missing one has team -1.
inline std::array<Site, 5> find(const VerdaRegion& region) {
    std::array<Site, 5> out{};
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets)
            for (int t = 0; t < 5; ++t)
                if (asset.model_path == model(t)) {
                    Site site{t, {}, asset.rotation_y};
                    const Vector3 shift = site.to_world({(model_min.x + model_max.x) * .5F, 0, (model_min.z + model_max.z) * .5F});
                    site.origin = {asset.position.x - shift.x, asset.position.y - model_min.y, asset.position.z - shift.z};
                    out[static_cast<std::size_t>(t)] = site;
                }
    return out;
}

} // namespace outland::world::ballpark
