#pragma once

#include <raylib.h>

namespace outland::player {

class VerdanCharacter {
public:
    static void draw(
        Vector3 feet_position,
        float yaw,
        float movement_amount,
        float animation_time,
        bool armed = false
    );
};

} // namespace outland::player
