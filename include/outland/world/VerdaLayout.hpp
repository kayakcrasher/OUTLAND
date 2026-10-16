#pragma once
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
inline constexpr float capital_flat_radius = 180;
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
