#include "outland/world/assets/VerdanProps.hpp"

#include <raylib.h>

namespace outland::world::assets {

void VerdanProps::draw_utility_pole(
    const Vector3 p
) {
    DrawCylinder(
        {
            p.x,
            p.y + 3.5F,
            p.z
        },
        0.13F,
        0.18F,
        7.0F,
        8,
        Color{82, 62, 43, 255}
    );

    DrawCube(
        {
            p.x,
            p.y + 6.4F,
            p.z
        },
        2.3F,
        0.14F,
        0.14F,
        Color{72, 54, 38, 255}
    );

    DrawSphere(
        {
            p.x - 0.8F,
            p.y + 6.25F,
            p.z
        },
        0.12F,
        DARKGRAY
    );

    DrawSphere(
        {
            p.x + 0.8F,
            p.y + 6.25F,
            p.z
        },
        0.12F,
        DARKGRAY
    );
}

void VerdanProps::draw_concrete_barrier(
    const Vector3 p,
    const float length
) {
    DrawCube(
        {
            p.x,
            p.y + 0.55F,
            p.z
        },
        length,
        1.1F,
        0.45F,
        Color{145, 143, 132, 255}
    );

    DrawCubeWires(
        {
            p.x,
            p.y + 0.55F,
            p.z
        },
        length,
        1.1F,
        0.45F,
        Color{80, 80, 75, 255}
    );
}

void VerdanProps::draw_road_sign(
    const Vector3 p,
    const char* text
) {
    DrawCylinder(
        {
            p.x,
            p.y + 1.3F,
            p.z
        },
        0.06F,
        0.06F,
        2.6F,
        6,
        DARKGRAY
    );

    DrawCube(
        {
            p.x,
            p.y + 2.35F,
            p.z
        },
        2.8F,
        0.85F,
        0.10F,
        Color{42, 112, 65, 255}
    );

    /*
     * World-space sign geometry now.
     * Proper texture-backed Esperanto lettering
     * comes with the material pass.
     */

    (void)text;
}

void VerdanProps::draw_grass_clump(
    const Vector3 p,
    const float scale
) {
    const Color grass{
        65,
        92,
        48,
        255
    };

    for (int i = -2; i <= 2; ++i) {
        DrawCube(
            {
                p.x +
                    static_cast<float>(i) *
                    0.08F *
                    scale,

                p.y +
                    0.25F *
                    scale,

                p.z +
                    static_cast<float>(
                        i % 2
                    ) *
                    0.06F
            },
            0.035F * scale,
            0.50F * scale,
            0.035F * scale,
            grass
        );
    }
}

void VerdanProps::draw_rock(
    const Vector3 p,
    const float scale
) {
    DrawSphere(
        {
            p.x,
            p.y + 0.35F * scale,
            p.z
        },
        0.55F * scale,
        Color{105, 105, 95, 255}
    );
}

} // namespace outland::world::assets
