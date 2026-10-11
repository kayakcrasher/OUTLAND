#include "outland/world/Zones.hpp"
#include "outland/world/VerdaLayout.hpp"
#include <cmath>
#include <limits>

namespace outland::world::zones {

ZoneId zone_at(float x, float z) {
    // Verdkerno is the capital district: Verda City and its edge, a rough circle about 560 m
    // across from the centre, its line wobbling a little like a city limit.
    const auto* capital = layout::find("capital_verda");
    if (capital) {
        const float dx = x - capital->center.x, dz = z - capital->center.z;
        const float angle = std::atan2(dz, dx);
        const float limit = capital_radius + 35 * std::sin(3 * angle + .7F) + 20 * std::cos(7 * angle);
        if (dx * dx + dz * dz < limit * limit) return ZoneId::Verdkerno;
    }
    // The four counties share the rest of the island, each nearest its own town. Wander the
    // sample point (up to ~110 m) so their borders curve instead of running straight.
    const float wx = x + 70 * std::sin(z * .0023F + 1.3F) + 40 * std::sin(z * .0061F + x * .0017F);
    const float wz = z + 70 * std::sin(x * .0021F + .4F) + 40 * std::cos(x * .0057F - z * .0019F);
    ZoneId best = ZoneId::Altverdo;
    float nearest = std::numeric_limits<float>::max();
    for (const auto& z_ : zones) {
        if (z_.id == ZoneId::Verdkerno) continue;
        const auto* site = layout::find(z_.seat);
        if (!site) continue;
        const float d = std::hypot(wx - site->center.x, wz - site->center.z);
        if (d < nearest) {nearest = d; best = z_.id;}
    }
    return best;
}

} // namespace outland::world::zones
