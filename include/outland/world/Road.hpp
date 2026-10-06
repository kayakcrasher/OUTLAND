#pragma once

#include <raylib.h>

namespace outland::world {

enum class RoadType {
    Dirt,
    Gravel,
    Asphalt
};

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
