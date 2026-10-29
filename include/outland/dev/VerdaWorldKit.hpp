#pragma once
#include <raylib.h>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::creator { class CreatorAssetRegistry; }

namespace outland::dev {
// A town street has a street light at least this often (alternating sides): street_lighting().
inline constexpr float street_light_spacing = 28;
struct KitBuilding {
    Vector3 origin{};     // ground centre
    float yaw{0}, width{0}, depth{0};
    int storeys{1};
    float door_x{0};      // local X of the front doorway; the front wall is at local z = -depth/2
    Vector3 stairs_start{}; // local position in front of the first step
    Vector3 stairs_direction{1, 0, 0}; // local direction the flight climbs
    float stairs_length{6};  // walk this far from stairs_start to stand on the upper floor
    float ground_floor{.1F}, upper_floor{3.1F}; // floor-top heights above the building base
    Vector3 upstairs_walk{0, 0, -4}; // from the top of the stairs, a walk across the upper floor
};
struct WorldKitReport {
    int assets{0}, buildings{0}, skipped_lots{0}, purposes{0}, parks{0}, driveways{0}, homesteads{0}, bus_stops{0}, turnarounds{0}, houses{0}, trucks{0}, hamlets{0}, cities{0}, loot_spots{0};
    int police_stations{0}, police_cars{0}, ambulances{0}, clinics{0}, south_blocks{0};
    int baptist_churches{0}, orthodox_churches{0};   // game::culture::Faith: the west's chapels, the east's churches
    int street_lights{0};   // added by street_lighting() along dark town streets
    int retagged{0}; // purpose markers inside a model building (they retag it, not make a place)
    int cleared{0};  // older pieces removed where a town was rebuilt (Espera's and Suda Haveno's villages)
    bool hospital{false};
    bool parliament{false}, stadium_reserved{false}, stela_turo{false};
    Vector3 stela_turo_at{};
    std::vector<Vector3> ballparks;    // La Verda Basballigo's parks (origins), in team order
    bool parking_garage{false};       // Parkumejo Centra (world/ParkingGarage.hpp), its origin
    Vector3 parking_garage_at{};
    std::vector<Vector3> ranches;     // each Vakero ranch's yard
    bool stockyard{false};
    bool cargo_terminal{false}, freight_depot{false};
    std::vector<Vector3> radio_stations;   // Verda Popolko (Verda City), Radio Sovaĝejo, Espera Roko (doors), in that order
    std::vector<Vector3> gun_shops;        // Armilejo's shops (doors): one in each of the five main cities
    std::vector<Vector3> gang_houses;      // the county gangs' houses (centres): Espera, Porto Luma, Suda Haveno, Roka
    // The motor trade (world/MotorTrade.hpp): Marnafto's stations (2 in Verda City, 1 in each big
    // town, 1 on the ring road), the Meĥanikejo workshops, the car lot, the offshore rigs (origins).
    std::vector<Vector3> gas_stations, mechanics, oil_rigs;
    bool car_lot{false};
    std::vector<Vector3> freight_yards;    // Verda Transporto's drops off the highways (yard centres)
    Vector3 car_lot_at{};
    int town_halls{0};
    std::vector<KitBuilding> enterable;
    std::vector<Vector3> skipped;     // centres of lots left empty because something was there
    std::vector<std::string> missing; // catalog models that could not be placed
};
// Downtown Verda (the capital) is wiped and rebuilt as a mid-sized American grid city: 80 m blocks,
// 12 m streets with crosswalks, sidewalks, a tower core round a courthouse square, enterable
// Main Street brick rows, parking lots, street trees, lights, signals and parked cars.
// West of downtown, Okcidenta Verda: suburban blocks of detached homes on a boulevard. Porto Luma
// is rebuilt as a grid city round Placo Luma with a waterfront district (see docs/cities.md).
// Dresses Verda's five towns with the Creator catalog: enterable modular buildings (doorways,
// open windows, floors, roofs, stairs to upper floors) and town-specific props. Every piece is an
// ordinary Creator world asset, so the result is edited, saved and exported like hand-built work.
// Lots that overlap existing buildings or assets are skipped. Deterministic.
// Kept free in Verda City for a future baseball stadium: 150 x 150 m of flat land east of Orienta.
inline constexpr Vector3 stadium_reserve{606, 0, 76};  // clear of the east-west highway's curve out of the city
inline constexpr float stadium_half = 75;
WorldKitReport build_verda_towns(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog);
}
