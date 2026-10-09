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
    Vector3 stairs_start{}; // local position in front of the first step; the flight climbs along +X
                            // and tops out 6 m further on, on the upper floor
};
struct WorldKitReport {
    int assets{0}, buildings{0}, skipped_lots{0};
    std::vector<KitBuilding> enterable;
    std::vector<Vector3> skipped;     // centres of lots left empty because something was there
    std::vector<std::string> missing; // catalog models that could not be placed
};
// Dresses Verda's five towns with the Creator catalog: enterable modular buildings (doorways,
// open windows, floors, roofs, stairs to upper floors) and town-specific props. Every piece is an
// ordinary Creator world asset, so the result is edited, saved and exported like hand-built work.
// Lots that overlap existing buildings or assets are skipped. Deterministic.
WorldKitReport build_verda_towns(world::VerdaRegion& region, const creator::CreatorAssetRegistry& catalog);
}
