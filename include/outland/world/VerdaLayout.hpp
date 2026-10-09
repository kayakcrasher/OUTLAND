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
inline constexpr Vector3 legacy_espera{0, 0, -70};
inline constexpr float sea_level = -32;
inline constexpr float capital_flat_radius = 180;
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
