#pragma once

#include <raylib.h>

namespace outland::world::assets {

class VerdanProps {
public:
    static void draw_utility_pole(
        Vector3 ground_position
    );

    static void draw_concrete_barrier(
        Vector3 ground_position,
        float length
    );

    static void draw_road_sign(
        Vector3 ground_position,
        const char* text
    );

    static void draw_grass_clump(
        Vector3 ground_position,
        float scale = 1.0F
    );

    static void draw_rock(
        Vector3 ground_position,
        float scale = 1.0F
    );
};

} // namespace outland::world::assets
