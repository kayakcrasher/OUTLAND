#pragma once
#include "outland/game/economy/Economy.hpp"
#include "outland/game/life/LifeTypes.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

// Homes for sale in Explore, besides the Stela Turo penthouse: fourteen empty houses and flats
// round the island, each with a VENDATA board out front. Walk in and buy one (E / USE); it is
// furnished the moment it is yours (game::interiors::furnish_home). At home: sleep in your bed
// till morning (healed), and when you die you wake up in the home nearest to where you fell.
//
// The listings: detached houses in Okcidenta Verda, Verda City's west suburb, and Main Street
// flats in the other towns (two in each that has the rows to spare, the ones furthest from the
// centre). Nobody lives in them (their building is vacant until it is yours). Fixed by the map,
// the same every session; ownership is kept with the Explore wallet (EconomyState::properties).
namespace outland::game::economy {

struct Listing {
    std::string id;         // the property id kept in the wallet: "home_" + building
    std::string building;   // the world building (or Main Street asset) it is
    std::string name;       // on the board and the map
    int settlement{0};
    Vector3 centre{};       // ground-floor centre of the building (y: its base)
    float yaw{0};
    Vector3 size{};
    bool house{false};      // a detached house (else a Main Street flat)
    std::int64_t price{0};
    Vector3 board{};        // the VENDATA board: on the pavement in front of the door
};

inline constexpr int home_listings = 14;

// The listings in a region, in a fixed order (houses first). Ground heights are left to the
// caller for houses (their base is 0 in the map).
std::vector<Listing> plan_homes(const world::VerdaRegion& region);
// Make every listed building vacant (nobody lives there) for this session; returns what each
// was before, for restore_purposes.
struct Vacated { std::string id; world::BuildingPurpose purpose; bool asset; };
std::vector<Vacated> vacate(world::VerdaRegion& region, const std::vector<Listing>& homes);
void restore_purposes(world::VerdaRegion& region, const std::vector<Vacated>& vacated);

[[nodiscard]] bool owns(const EconomyState& wallet, const Listing& home);
// Pays and takes the keys (all or nothing); returns what to tell the player.
std::string buy(EconomyState& wallet, const Listing& home);
// Feet inside the listing's ground floor.
[[nodiscard]] bool inside(const Listing& home, Vector3 feet, float margin = .3F);
// The place island life would have made of it (for the furniture).
life::Place place_of(const Listing& home);

} // namespace outland::game::economy
