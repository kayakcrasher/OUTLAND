#include "outland/world/Zones.hpp"
#include "outland/world/VerdaLayout.hpp"
#include <cmath>
#include <limits>

namespace outland::world::zones {

ZoneId zone_at(float x, float z) {
    // Wander the sample point a little (up to ~110 m) so borders curve instead of running straight.
    const float wx = x + 70 * std::sin(z * .0023F + 1.3F) + 40 * std::sin(z * .0061F + x * .0017F);
    const float wz = z + 70 * std::sin(x * .0021F + .4F) + 40 * std::cos(x * .0057F - z * .0019F);
    // The capital's zone is the island's core: it reaches a little further than its seat's
    // distance alone would give it.
    ZoneId best = ZoneId::Verdkerno;
    float nearest = std::numeric_limits<float>::max();
    for (const auto& z_ : zones) {
        const auto* site = layout::find(z_.seat);
        if (!site) continue;
        const float d = std::hypot(wx - site->center.x, wz - site->center.z) * (z_.id == ZoneId::Verdkerno ? .82F : 1.0F);
        if (d < nearest) {nearest = d; best = z_.id;}
    }
    return best;
}

} // namespace outland::world::zones
