#include "outland/world/assets/VerdanArchitecture.hpp"

namespace outland::world::assets {

void VerdanArchitecture::draw_window(
    const Vector3 position,
    const float width,
    const float height,
    const HouseStyle& style
) {
    // Dark recess behind glass.
    DrawCube(
        position,
        width + 0.18F,
        height + 0.18F,
        0.12F,
        style.trim
    );

    DrawCube(
        {
            position.x,
            position.y,
            position.z - 0.07F
        },
        width,
        height,
        0.06F,
        style.glass
    );

    // Window divider.
    DrawCube(
        {
            position.x,
            position.y,
            position.z - 0.11F
        },
        0.07F,
        height,
        0.04F,
        style.trim
    );
}

void VerdanArchitecture::draw_door(
    const Vector3 position,
    const HouseStyle& style
) {
    DrawCube(
        position,
        1.35F,
        2.35F,
        0.15F,
        style.trim
    );

    DrawCube(
        {
            position.x,
            position.y,
            position.z - 0.09F
        },
        1.12F,
        2.12F,
        0.08F,
        Color{
            82,
            65,
            48,
            255
        }
    );

    // Handle.
    DrawSphere(
        {
            position.x + 0.38F,
            position.y,
            position.z - 0.17F
        },
        0.07F,
        Color{
            170,
            150,
            95,
            255
        }
    );
}

void VerdanArchitecture::draw_balcony(
    const Vector3 position,
    const float width
) {
    // Balcony slab.
    DrawCube(
        position,
        width,
        0.18F,
        1.35F,
        Color{
            115,
            112,
            100,
            255
        }
    );

    const float front_z =
        position.z - 0.67F;

    // Rail.
    DrawCube(
        {
            position.x,
            position.y + 0.75F,
            front_z
        },
        width,
        0.08F,
        0.08F,
        DARKGRAY
    );

    // Vertical railing.
    for (
        float x = -width * 0.45F;
        x <= width * 0.45F;
        x += 0.65F
    ) {
        DrawCube(
            {
                position.x + x,
                position.y + 0.38F,
                front_z
            },
            0.06F,
            0.75F,
            0.06F,
            DARKGRAY
        );
    }
}

void VerdanArchitecture::draw_roof(
    const Vector3 position,
    const Vector3 building_size,
    const HouseStyle& style
) {
    /*
     * Temporary low-cost roof.
     *
     * Later this becomes proper pitched geometry.
     * The overhang already improves the silhouette.
     */

    DrawCube(
        {
            position.x,
            position.y +
                building_size.y +
                0.28F,
            position.z
        },
        building_size.x + 0.65F,
        0.55F,
        building_size.z + 0.65F,
        style.roof
    );

    // Roof edge/fascia.
    DrawCubeWires(
        {
            position.x,
            position.y +
                building_size.y +
                0.28F,
            position.z
        },
        building_size.x + 0.65F,
        0.55F,
        building_size.z + 0.65F,
        Color{
            70,
            65,
            58,
            255
        }
    );
}

void VerdanArchitecture::draw_house(
    const Vector3 ground_position,
    const Vector3 size,
    const HouseStyle& style
) {
    // Foundation.
    DrawCube(
        {
            ground_position.x,
            ground_position.y + 0.22F,
            ground_position.z
        },
        size.x + 0.25F,
        0.44F,
        size.z + 0.25F,
        style.foundation
    );

    // Main wall mass.
    DrawCube(
        {
            ground_position.x,
            ground_position.y +
                size.y * 0.5F +
                0.44F,
            ground_position.z
        },
        size.x,
        size.y,
        size.z,
        style.plaster
    );

    const float front =
        ground_position.z -
        size.z * 0.5F -
        0.06F;

    // Door.
    draw_door(
        {
            ground_position.x,
            ground_position.y + 1.60F,
            front
        },
        style
    );

    // Ground-floor windows.
    draw_window(
        {
            ground_position.x - size.x * 0.29F,
            ground_position.y + 1.75F,
            front
        },
        1.25F,
        1.35F,
        style
    );

    draw_window(
        {
            ground_position.x + size.x * 0.29F,
            ground_position.y + 1.75F,
            front
        },
        1.25F,
        1.35F,
        style
    );

    if (style.upper_floor) {
        draw_window(
            {
                ground_position.x - size.x * 0.27F,
                ground_position.y +
                    size.y * 0.70F,
                front
            },
            1.30F,
            1.40F,
            style
        );

        draw_window(
            {
                ground_position.x + size.x * 0.27F,
                ground_position.y +
                    size.y * 0.70F,
                front
            },
            1.30F,
            1.40F,
            style
        );
    }

    if (style.balcony) {
        draw_balcony(
            {
                ground_position.x,
                ground_position.y +
                    size.y * 0.57F,
                front - 0.55F
            },
            size.x * 0.72F
        );
    }

    draw_roof(
        {
            ground_position.x,
            ground_position.y + 0.44F,
            ground_position.z
        },
        size,
        style
    );
}

} // namespace outland::world::assets
