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
    {0, "capital_verda", {606, 0, 76}},       // Verda City: the plot kept for it east of Orienta
    {1, "village_espera", {-256, 0, -1755}},     // west of Espera, toward the hills
    {2, "port_luma", {1630, 0, -186}},
    {3, "south_haven", {180, 0, 1586}},
    {4, "west_roka", {-1708, 0, 686}},
}};
inline constexpr float flat_radius = 80, flat_blend = 70;   // the level pad (eased out wide: an even climb to the gate)

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
// Along a base line: t metres down it from the plate (first base's line is `first`) and d metres
// out from it into foul ground.
[[nodiscard]] inline Vector3 along(bool first, float t, float d, float y = 0) {
    constexpr float s = .70710678F;
    const float side = first ? -1.0F : 1.0F;
    return {side * s * (t + d), y, plate.z + s * (t - d)};
}
// The model yaw of something facing the diamond square-on from beside a line.
[[nodiscard]] inline float facing_field(bool first) { return first ? 45.0F : -45.0F; }
// The grandstand: covered, behind the plate and the backstop netting, facing the field (+Z). Two
// wings of stepped seats either side of a middle aisle - the home crowd on the first-base side
// (-X), the visitors on the third-base side - with a roof over them and the press box at the top
// of the aisle. Row k's seat is (k + 1) * row_rise up, k rows back from the front.
inline constexpr float stand_front = -49.6F, wing_x0 = 3.4F, wing_x1 = 25.6F;
inline constexpr float row_rise = .45F, row_depth = .8F;
inline constexpr int rows = 8;
inline constexpr float seat_spacing = .7F;
inline constexpr int seats_per_row = static_cast<int>((wing_x1 - wing_x0 - .7F) / seat_spacing);
// The dugouts along the lines (home by first), their benches; the home club's locker room behind.
inline constexpr float dugout_t0 = 16.0F, dugout_t1 = 28.0F, bench_d = 10.1F;   // (clear of the grandstand's view of the infield)
inline const Vector3 home_dugout = along(true, 22, 9.3F), away_dugout = along(false, 22, 9.3F);
inline constexpr float locker_x0 = -44, locker_x1 = -30, locker_z0 = -62, locker_z1 = -52;
inline constexpr Vector3 locker_door{-37, 0, -62.6F};
// The on-deck circles and the coaches' boxes (batting side's), the plate umpire.
inline const Vector3 home_on_deck = along(true, 5.5F, 9.5F), away_on_deck = along(false, 5.5F, 9.5F);
inline const Vector3 first_coach = along(true, 23.5F, 6.0F), third_coach = along(false, 23.5F, 6.0F);
inline constexpr Vector3 umpire{0, 0, -38.9F};
// The bus bays in the car park (home, visitors) and the gate.
inline constexpr Vector3 home_bus{-16, 0, -68}, away_bus{16, 0, -68};
inline constexpr Vector3 gate{0, 0, -60};   // behind the grandstand, on the car park
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
        const float side = home ? -1.0F : 1.0F;
        return to_world({side * (wing_x0 + .35F + static_cast<float>(place) * seat_spacing), static_cast<float>(row + 1) * row_rise,
                         stand_front - static_cast<float>(row) * row_depth - row_depth * .5F});
    }
    // The way a seat faces, in world yaw: out over the backstop at the field (turned a little in
    // toward the plate from the far ends of the wings).
    [[nodiscard]] float seat_yaw(bool home) const { return yaw + (home ? 8.0F : -8.0F); }
    // A place on a dugout's bench (k from the plate end), facing the field.
    [[nodiscard]] Vector3 bench(bool home, int k) const { return to_world(along(home, dugout_t0 + .8F + static_cast<float>(k) * 1.15F, bench_d)); }
};

// The model's bounds (tools/generate_city_kit.py ballpark()): the renderer centres a model on
// them, so the world kit stores a park at the bounds' centre, not at its design origin.
inline constexpr Vector3 model_min{-68, -.3F, -76}, model_max{68, 19.6F, 64};

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
