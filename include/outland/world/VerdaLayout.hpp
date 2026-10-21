#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <raylib.h>
#include <string_view>

namespace outland::world::layout {
struct Site {
    std::string_view id, name, identity;
    Vector3 center;
};
// Stable IDs are also map, marker and NPC persistence identities.
inline constexpr std::array<Site, 5> sites{
    {{"capital_verda", "Verda", "central capital", {0, 0, 0}},
     {"village_espera", "Espera", "northern rural town", {0, 0, -1800}},
     {"port_luma", "Porto Luma", "eastern trading port", {1750, 0, -400}},
     {"south_haven", "Suda Haveno", "southern residential haven", {300, 0, 1800}},
     {"west_roka", "Roka", "western industrial town", {-1750, 0, 450}}}};
// Hamlets: small places the world kit builds off the highways, each on its own level pad. They
// are not sites (no highway of their own); the ring road passes them.
inline constexpr std::array<Site, 4> hamlets{
    {{"hamlet_ruldomaro", "Ruldomaro", "trailer-park hamlet on the eastern ring road", {997, 0, -997}},
     {"hamlet_vojrando", "Vojrando", "highway stop on the Porto Luma road", {1312, 0, -239}},
     {"hamlet_pinejo", "Pinejo", "pine-wood trailer stop on the north-west ring road", {-965, 0, -1017}},
     {"hamlet_sablovalo", "Sablovalo", "sandy-valley trailer stop on the south-west ring road", {-909, 0, 1077}}}};
// Roka is an industrial city: a street grid with works to the west, on a wide pad like Porto Luma's.
inline constexpr float roka_flat_radius = 220;
// Sovaĝejo, the backcountry settlement, sits north of Roka at the end of a dirt road, on its own pad.
inline constexpr Vector3 sovagejo_center{-1790, 0, 90};
inline constexpr float sovagejo_flat_radius = 95;
inline constexpr float hamlet_flat_radius = 72;
inline constexpr Vector3 legacy_espera{0, 0, -70};
inline constexpr float sea_level = -32;
// Downtown Verda City's street grid (the world kit's downtown()): its outer street edges. The
// north-south highway runs straight through it down x = 0 as a planted boulevard.
inline constexpr float downtown_half_x = 179, downtown_half_z = 176;
inline constexpr float downtown_south_edge = 268;   // the row of blocks south of the grid ends here
// How far (x, z) is outside the level rectangle downtown stands on (0 inside).
inline float downtown_outside(float x, float z) {
    const float dx = std::max(std::abs(x) - downtown_half_x - 6, 0.0F);
    const float dz = std::max({z - downtown_south_edge, -z - downtown_half_z - 6, 0.0F});
    return std::hypot(dx, dz);
}
inline constexpr bool in_downtown(float x, float z) { return x > -downtown_half_x && x < downtown_half_x && z > -downtown_half_z - 90 && z < downtown_half_z + 90; }
// Ŝtata Malliberejo, Verda's only state prison: alone on a level pad outside the ring road to the
// south-east, a kilometre from the nearest town; its gate faces the ring (yaw 45: the model's
// front, -Z, toward the island's centre), and an access road runs out to it from the ring.
inline constexpr Vector3 prison_center{1290, 0, 1290};
inline constexpr float prison_yaw = 45;
inline constexpr float prison_flat_radius = 64;
inline constexpr float capital_flat_radius = 180;
// Verda City's industrial quarter (Industria Kvartalo), west of the west suburbs and south of the
// Roka highway: level ground (the suburbs' level) for Verda Transporto's freight depot.
inline constexpr float freight_x0 = -652, freight_x1 = -490, freight_z0 = -178, freight_z1 = -34;
inline float freight_outside(float x, float z) {
    return std::hypot(std::max({freight_x0 - x, x - freight_x1, 0.0F}), std::max({freight_z0 - z, z - freight_z1, 0.0F}));
}
// Porto Luma's cargo terminal (Kargohaveno de Porto Luma). The town stands 38 m over the sea, so a
// ramp runs from the quay road (x 1950) straight out east down to a quay at the shore: a level
// apron 4 m over the water with deep water dredged along its face, where the cargo ship berths.
inline constexpr float cargo_z = -470;                        // the ramp's centre line
inline constexpr float cargo_ramp_x0 = 1950, cargo_ramp_x1 = 2178, cargo_ramp_half = 8;
inline constexpr float cargo_quay_x1 = 2238, cargo_quay_half = 56;    // the apron: x ramp_x1..quay_x1, z cargo_z +- half
inline constexpr float cargo_quay_height = sea_level + 4;
inline constexpr float cargo_berth_depth = sea_level - 12;   // the dredged berth along the quay face
inline constexpr Vector3 cargo_loading{2214, 0, -481};      // on the quay lane, under the cranes: where rigs load
inline constexpr Vector3 freight_depot_gate{-560, 0, -84};   // Verda Transporto's front, on the yard road
inline constexpr Vector3 freight_boss{-556.5F, 0, -71.5F};     // the boss, out front of the office door (facing the road)
// Verda's suburbs, west (Okcidenta Verda) and east (Orienta Verda): level ground either side of
// downtown, |x| in [suburb_near, suburb_far], |z| <= suburb_half.
inline constexpr float suburb_near = 150, suburb_far = 485, suburb_half = 190;
// Porto Luma is a city now: its building pad is wider than the other towns'.
inline constexpr float town_flat_radius = 205, port_flat_radius = 205;  // Espera and Suda Haveno are cities now too
inline constexpr float shoreline_band = 160;
inline float shore_radius(float x, float z) {
    const float angle = std::atan2(z, x);
    return 2200 + 120 * std::sin(3 * angle) + 80 * std::cos(5 * angle);
}
inline const Site *find(std::string_view id) {
    for (const auto &site : sites)
        if (site.id == id)
            return &site;
    return nullptr;
}
} // namespace outland::world::layout
