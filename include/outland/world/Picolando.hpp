#pragma once
// Picolando ("Pizza Land"), the island's pizza chain: a pizzeria in each of four cities (the world
// kit's pizzerias(); the city kit's civic/pizzeria.glb - the giant pizza on the roof), each with
// its delivery cars, the Picolando Kuriero, parked in the lay-by in front. Each serves its own
// part of the island:
//   Verda City   everything inside the ring road - Verdkerno, the suburbs, the hamlets on the
//                ring - and the ring itself with La Ringo's bar and grills. Nobody else delivers
//                inside the ring.
//   Roka         all of Ferarbaro, Sovaĝejo with it.
//   Espera       Altverdo, the north.
//   Suda Haveno  Marĉfluo, the south.
// Porto Luma has no pizzeria: Lumamaro, the east coast, is served from whichever of Espera and
// Suda Haveno is nearer.
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Zones.hpp"
#include <raylib.h>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace outland::world::picolando {

inline constexpr const char* model = "assets/verda/city/civic/pizzeria.glb";
// The model's bounds' centre (its local z: the awnings reach further forward than the back wall):
// where the kit stores it, against its design origin.
inline constexpr float centre_z = -.47F;
// In the model's frame (front -Z): just inside the door (the place's marker); the counter; the lay-by in front, where a car pulls up
// for the boxes (by the door, between the parked cars); the delivery cars' bays across it, nose
// out to the street (x; their centres at bay_z).
inline constexpr Vector2 door{0, -3.3F};
inline constexpr Vector2 counter{-1.0F, -1.2F};   // in front of the counter, where you talk to them
inline constexpr float pickup_z = -10.0F;
inline constexpr float bay_z = -8.4F;
inline constexpr std::array<float, 3> bays{-4.6F, 2.5F, 5.1F};
// The lot the kit finds for one: the building, its front `front` from the origin, then the lay-by.
inline constexpr float front = 5.0F, apron = 6.2F;

enum class Shop : unsigned char { Verda, Roka, Espera, SudaHaveno };
inline constexpr std::array<const char*, 4> town_ids{"capital_verda", "west_roka", "village_espera", "south_haven"};
inline constexpr std::array<const char*, 4> names{"Picolando Verda", "Picolando Roka", "Picolando Espera", "Picolando Suda Haveno"};

// Verda's own: within the ring road (and its verges), or at one of La Ringo's bar and grills.
inline bool inner(float x, float z) {
    if (std::hypot(x, z) < 1500 + 30) return true;
    for (const auto& rest : highways::rest_areas())
        if (std::hypot(rest.centre.x - x, rest.centre.z - z) < 120) return true;
    return false;
}

// The pizzeria whose area (x, z) is in.
inline Shop serving(float x, float z) {
    if (inner(x, z)) return Shop::Verda;
    switch (zones::zone_at(x, z)) {
        case zones::ZoneId::Ferarbaro: return Shop::Roka;
        case zones::ZoneId::Altverdo: return Shop::Espera;
        case zones::ZoneId::Marcfluo: return Shop::SudaHaveno;
        case zones::ZoneId::Lumamaro: {
            const auto* espera = layout::find("village_espera");
            const auto* south = layout::find("south_haven");
            if (!espera || !south) return Shop::Espera;
            return std::hypot(espera->center.x - x, espera->center.z - z) <= std::hypot(south->center.x - x, south->center.z - z) ? Shop::Espera
                                                                                                                               : Shop::SudaHaveno;
        }
        default: return Shop::Verda;
    }
}

struct Pizzeria {
    Shop shop{Shop::Verda};
    std::string name;
    Vector3 origin{};      // the design origin, on the ground
    float yaw{0};          // front (-Z) to the street
    [[nodiscard]] Vector3 at(float x, float y, float z) const {
        const float a = yaw * DEG2RAD;
        return {origin.x + x * std::cos(a) + z * std::sin(a), origin.y + y, origin.z - x * std::sin(a) + z * std::cos(a)};
    }
    [[nodiscard]] Vector3 door_point() const { return at(door.x, 0, door.y); }
    [[nodiscard]] Vector3 counter_point() const { return at(counter.x, 0, counter.y); }
    [[nodiscard]] Vector3 pickup() const { return at(0, 0, pickup_z); }
};

// The pizzerias on the map (each named for the town it is nearest).
inline std::vector<Pizzeria> find(const VerdaRegion& region) {
    std::vector<Pizzeria> out;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            if (asset.model_path != model) continue;
            Pizzeria p;
            p.yaw = asset.rotation_y;
            const float a = p.yaw * DEG2RAD;
            p.origin = {asset.position.x - centre_z * std::sin(a), asset.position.y, asset.position.z - centre_z * std::cos(a)};
            float best = 1e9F;
            for (std::size_t i = 0; i < town_ids.size(); ++i)
                if (const auto* site = layout::find(town_ids[i])) {
                    const float d = std::hypot(site->center.x - p.origin.x, site->center.z - p.origin.z);
                    if (d < best) { best = d; p.shop = static_cast<Shop>(i); }
                }
            p.name = names[static_cast<std::size_t>(p.shop)];
            out.push_back(std::move(p));
        }
    return out;
}

} // namespace outland::world::picolando
