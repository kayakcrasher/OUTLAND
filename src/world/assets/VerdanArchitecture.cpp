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

    // --------------------------------------------------------
    // PLAYABLE HOUSE SHELL
    // --------------------------------------------------------
    //
    // OUTLAND buildings are gameplay spaces, not solid props.
    // Build the exterior from individual wall sections so the
    // front entrance is a real opening into the structure.

    constexpr float wall_thickness = 0.24F;
    constexpr float doorway_width = 1.45F;
    constexpr float doorway_height = 2.35F;

    const float wall_center_y =
        ground_position.y +
        size.y * 0.5F +
        0.44F;

    const float front_z =
        ground_position.z -
        size.z * 0.5F;

    const float back_z =
        ground_position.z +
        size.z * 0.5F;

    // Back wall.
    DrawCube(
        {
            ground_position.x,
            wall_center_y,
            back_z
        },
        size.x,
        size.y,
        wall_thickness,
        style.plaster
    );

    // Left wall.
    DrawCube(
        {
            ground_position.x - size.x * 0.5F,
            wall_center_y,
            ground_position.z
        },
        wall_thickness,
        size.y,
        size.z,
        style.plaster
    );

    // Right wall.
    DrawCube(
        {
            ground_position.x + size.x * 0.5F,
            wall_center_y,
            ground_position.z
        },
        wall_thickness,
        size.y,
        size.z,
        style.plaster
    );

    // --------------------------------------------------------
    // FRONT WALL WITH REAL OPENINGS
    // --------------------------------------------------------
    // Layout:
    //
    //   [ WINDOW ]   [ DOOR ]   [ WINDOW ]
    //
    // These are actual holes in the wall geometry.

    constexpr float window_width = 1.25F;
    constexpr float window_height = 1.35F;

    // Front-wall pieces are measured upward from the top of
    // the 0.44 m foundation. The visible window center is
    // 1.75 m above ground, so convert it into wall-local Y.
    constexpr float foundation_height = 0.44F;
    constexpr float window_world_center_y = 1.75F;
    constexpr float window_center_y =
        window_world_center_y - foundation_height;

    const float window_bottom =
        window_center_y - window_height * 0.5F;

    const float window_top =
        window_center_y + window_height * 0.5F;

    const float window_x =
        size.x * 0.29F;

    const float left_window_left =
        -window_x - window_width * 0.5F;

    const float left_window_right =
        -window_x + window_width * 0.5F;

    const float right_window_left =
        window_x - window_width * 0.5F;

    const float right_window_right =
        window_x + window_width * 0.5F;

    auto draw_front_piece =
        [&](float x_min,
            float x_max,
            float y_min,
            float y_max) {

            if (x_max <= x_min || y_max <= y_min) {
                return;
            }

            DrawCube(
                {
                    ground_position.x +
                        (x_min + x_max) * 0.5F,
                    ground_position.y +
                        0.44F +
                        (y_min + y_max) * 0.5F,
                    front_z
                },
                x_max - x_min,
                y_max - y_min,
                wall_thickness,
                style.plaster
            );
        };

    const float left_edge = -size.x * 0.5F;
    const float right_edge = size.x * 0.5F;

    // Solid vertical wall sections between openings.
    draw_front_piece(
        left_edge,
        left_window_left,
        0.0F,
        size.y
    );

    draw_front_piece(
        left_window_right,
        -doorway_width * 0.5F,
        0.0F,
        size.y
    );

    draw_front_piece(
        doorway_width * 0.5F,
        right_window_left,
        0.0F,
        size.y
    );

    draw_front_piece(
        right_window_right,
        right_edge,
        0.0F,
        size.y
    );

    // Wall below each window.
    draw_front_piece(
        left_window_left,
        left_window_right,
        0.0F,
        window_bottom
    );

    draw_front_piece(
        right_window_left,
        right_window_right,
        0.0F,
        window_bottom
    );

    // Wall above each window.
    draw_front_piece(
        left_window_left,
        left_window_right,
        window_top,
        size.y
    );

    draw_front_piece(
        right_window_left,
        right_window_right,
        window_top,
        size.y
    );

    // Header above the doorway.
    draw_front_piece(
        -doorway_width * 0.5F,
        doorway_width * 0.5F,
        doorway_height,
        size.y
    );

    const float front =
        front_z - 0.06F;

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

        // Ground-floor windows are real gameplay openings.
        //
        // Do NOT call draw_window() here. That helper draws the
        // old decorative glass/pane assembly across the opening.
        //
        // The wall geometry above already forms the window holes.
        // The sill below remains as the physical vault obstacle.

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
