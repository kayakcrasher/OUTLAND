#pragma once

#include <raylib.h>

namespace outland::world {

enum class RoadType {
    Dirt,
    Gravel,
    Asphalt,
    Highway,    // a divided highway (world/Highways.hpp)
    Ramp,       // a slip road off a highway or onto one: one way, start to end
    Roundabout  // a piece of a roundabout's circulating lane: one way, start to end (anticlockwise on the map)
};

// One-way roads: traffic only runs from start to end.
inline constexpr bool one_way(RoadType type) { return type == RoadType::Ramp || type == RoadType::Roundabout; }

struct Road {
    Vector3 start{
        0.0F,
        0.02F,
        0.0F
    };

    Vector3 end{
        0.0F,
        0.02F,
        10.0F
    };

    float width{5.0F};

    RoadType type{
        RoadType::Dirt
    };
};

}
