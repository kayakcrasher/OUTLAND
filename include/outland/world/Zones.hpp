#pragma once
#include <raylib.h>
#include <array>
#include <cstddef>
#include <string_view>

// Verda's five zonoj (counties). Each grew round one of the island's towns; borders follow
// distance to those seats, roughened so they wander like real county lines.
namespace outland::world::zones {

enum class ZoneId : std::size_t { Verdkerno, Altverdo, Lumamaro, Marcfluo, Ferarbaro };
inline constexpr std::size_t zone_count = 5;

struct Zone {
    ZoneId id;
    std::string_view name;     // UTF-8 Esperanto
    std::string_view meaning;  // English gloss
    std::string_view seat;     // layout site id of its main town
    std::string_view character;
    Color color;               // map tint
};

inline constexpr std::array<Zone, zone_count> zones{{
    {ZoneId::Verdkerno, "Verdkerno", "Green Core", "capital_verda",
     "The capital, farms, administration and the island's roads: the heart of the nation.", {104, 168, 84, 255}},
    {ZoneId::Altverdo, "Altverdo", "High Green", "village_espera",
     "Northern highlands: forests, hills, hunting and sparse settlements on colder ground.", {70, 128, 104, 255}},
    {ZoneId::Lumamaro, "Lumamaro", "Sea of Light", "port_luma",
     "The bright eastern coast: the port, beaches, markets and the lighthouse.", {220, 186, 96, 255}},
    {ZoneId::Marcfluo, "Marĉfluo", "Marsh Flow", "south_haven",
     "Southern wetlands: rivers, marshes, reeds, fishing and floodplain life.", {92, 150, 156, 255}},
    {ZoneId::Ferarbaro, "Ferarbaro", "Iron Forest", "west_roka",
     "The western backcountry: gravel roads, trailers, mines and self-reliant people.", {168, 112, 76, 255}},
}};

inline const Zone& zone(ZoneId id) { return zones[static_cast<std::size_t>(id)]; }

// The zone a world position (x, z) lies in.
ZoneId zone_at(float x, float z);
inline ZoneId zone_at(Vector3 position) { return zone_at(position.x, position.z); }

} // namespace outland::world::zones
