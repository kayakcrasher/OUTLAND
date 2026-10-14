#pragma once
#include <raylib.h>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::creator { class CreatorAssetRegistry; }

namespace outland::dev {
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
    int police_stations{0}, police_cars{0}, ambulances{0}, south_blocks{0};
    int retagged{0}; // purpose markers inside a model building (they retag it, not make a place)
    bool hospital{false};
    bool parliament{false}, stadium_reserved{false};
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
inline constexpr Vector3 stadium_reserve{600, 0, 40};
inline constexpr float stadium_half = 75;
WorldKitReport build_verda_towns(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog);
}
