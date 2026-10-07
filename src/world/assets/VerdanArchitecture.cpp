#include "outland/world/assets/VerdanArchitecture.hpp"
#include "outland/world/assets/VerdaGeometry.hpp"

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
    DrawSphereEx(
        {
            position.x + 0.38F,
            position.y,
            position.z - 0.17F
        },
        0.07F,
        4,
        6,
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
    const auto vertices = roof_vertices(position, building_size);
    for (std::size_t face = 0; face < roof_faces.size(); ++face) {
        const auto indices = roof_faces[face];
        const Color color = face < 2 ? style.plaster : style.roof;
        DrawTriangle3D(vertices[indices[0]], vertices[indices[1]], vertices[indices[2]], color);
    }
    // Fascia on both eaves; no wireframe edges in the finished silhouette.
    for (float side : {-1.0F, 1.0F}) {
        DrawCube({position.x + side * (building_size.x * 0.5F + 0.35F),
                  position.y + building_size.y, position.z},
                 0.12F, 0.18F, building_size.z + 0.7F, style.trim);
    }
}

void VerdanArchitecture::draw_house(
    const Vector3 ground_position,
    const Vector3 size,
    const HouseStyle& style,
    const bool detailed
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

    if (detailed) {
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

        // Entry steps, window sills, corner trim and one chimney.
        DrawCube({ground_position.x, ground_position.y + 0.11F, front - 0.55F},
                 1.8F, 0.22F, 1.1F, style.foundation);
        DrawCube({ground_position.x, ground_position.y + 0.33F, front - 0.25F},
                 1.5F, 0.22F, 0.5F, style.foundation);
        for (float side : {-1.0F, 1.0F}) {
            DrawCube({ground_position.x + side * size.x * 0.29F,
                      ground_position.y + 1.04F, front - 0.13F},
                     1.55F, 0.12F, 0.32F, style.foundation);
            for (float end : {-1.0F, 1.0F}) {
                DrawCube({ground_position.x + side * (size.x * 0.5F - 0.08F),
                          ground_position.y + size.y * 0.5F + 0.44F,
                          ground_position.z + end * (size.z * 0.5F - 0.08F)},
                         0.2F, size.y, 0.2F, style.trim);
            }
        }
        DrawCube({ground_position.x + size.x * 0.24F,
                  ground_position.y + size.y + 0.44F + size.x * 0.18F,
                  ground_position.z + size.z * 0.2F},
                 0.65F, size.x * 0.24F, 0.65F, style.foundation);
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
