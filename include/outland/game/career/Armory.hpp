#pragma once
#include <array>
#include <string>

// What the sides arm themselves with (Explore).
//
// The Policio's armory, at every station's desk: the best kit on the island, and all of it free -
// but a Kadeto draws a sidearm and a vest, and the heavier guns come with rank. An officer on duty
// wears body armour, and every police car carries a pump shotgun.
//
// A gang's GUN MAN, at its main base: guns for the gang's own people, cheaper than the Armilejo
// and no questions asked; the heavier ones for those with the respect to carry them.
namespace outland::game::career {

struct ArmoryItem {
    const char* item;      // items.tsv
    const char* name;      // on the list
    int police_rank;       // the rank an officer needs (0 Kadeto .. 4 Komisaro)
    int gang_rank;         // the rank a gangster needs at the gun man (-1: he doesn't sell it)
    int gang_price;        // kronoj at the gun man
    const char* ammo;      // the rounds that come with it ("" none)
    int rounds;            // ...how many (or how many of the item itself, for gear)
};

inline constexpr std::array<ArmoryItem, 12> armory{{
    {"pistol", "V9 pistol", 0, 0, 150, "pistol_ammo", 45},
    {"vest_heavy", "Plate carrier (armour II)", 0, 2, 900, "", 0},
    {"shotgun", "VS12 pump shotgun", 1, 1, 420, "shotgun_shells", 20},
    {"machine_pistol", "VM9 machine pistol", 1, 0, 330, "pistol_ammo", 90},
    {"smg", "VK45 SMG", 2, 1, 390, "smg_ammo", 75},
    {"hunting_rifle", "VR30 rifle", 2, 2, 650, "rifle_ammo", 90},
    {"frag_grenade", "Frag grenades", 2, 2, 120, "", 2},
    {"double_barrel", "VD2 double barrel", 1, 1, 380, "shotgun_shells", 16},
    {"auto_shotgun", "VA12 auto shotgun", 3, 3, 540, "shotgun_shells", 24},
    {"sniper", "VP7 sniper rifle", 3, 3, 900, "sniper_ammo", 20},
    {"scope_4x", "4x scope", 3, 3, 300, "", 1},
    {"rocket_launcher", "VR7 rocket launcher", 4, 4, 2400, "rockets", 4},
}};

// Whether an officer of `rank` may draw item `index` (free), or a gangster of `rank` buy it.
[[nodiscard]] inline bool police_may_draw(int rank, std::size_t index) { return index < armory.size() && rank >= armory[index].police_rank; }
[[nodiscard]] inline bool gang_may_buy(int rank, std::size_t index) {
    return index < armory.size() && armory[index].gang_rank >= 0 && rank >= armory[index].gang_rank;
}
// The police vehicles that carry a shotgun (any patrol car, the Interceptor, a trooper's cruiser).
[[nodiscard]] inline bool police_vehicle(const std::string& definition) {
    return definition == "police" || definition == "interceptor" || definition == "highway_patrol";
}

} // namespace outland::game::career
