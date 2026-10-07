#pragma once

#include "outland/world/VerdaRegion.hpp"
#include <algorithm>

namespace outland::world::assets {

inline bool on_road(Vector3 position, const Road& road, float margin = 0.0F) {
    const float dx = road.end.x - road.start.x;
    const float dz = road.end.z - road.start.z;
    const float length2 = dx*dx + dz*dz;
    if (length2 < 0.000001F || road.width <= 0.0F) return false;
    const float t = std::clamp(((position.x-road.start.x)*dx +
                               (position.z-road.start.z)*dz) / length2, 0.0F, 1.0F);
    const float x = position.x - (road.start.x + t*dx);
    const float z = position.z - (road.start.z + t*dz);
    const float radius = road.width * 0.5F + margin;
    return x*x + z*z < radius*radius;
}

enum class GroundSurface { Grass, Dirt, Gravel };

inline GroundSurface ground_surface(Vector3 position, const VerdaRegion& region) {
    for (const auto& settlement : region.settlements()) {
        for (const auto& road : settlement.roads) {
            if (on_road(position, road)) {
                return road.type == RoadType::Dirt ? GroundSurface::Dirt : GroundSurface::Gravel;
            }
        }
    }
    return GroundSurface::Grass;
}

} // namespace outland::world::assets
