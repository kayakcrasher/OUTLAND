#include "outland/world/foliage/FoliageSystem.hpp"

#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaLayout.hpp"

#include <raylib.h>
#include <raymath.h>

#include <cmath>
#include <algorithm>
#include "outland/world/assets/VerdaGeometry.hpp"
#include "outland/world/assets/GroundSurface.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"

namespace outland::world::foliage {

namespace {

constexpr float grass_spacing = 2.8F;

constexpr int grass_radius_cells = 25;

constexpr float maximum_draw_distance =
    grass_spacing *
    static_cast<float>(
        grass_radius_cells
    );

Color grass_color(
    const float variation
) {
    return {
        static_cast<unsigned char>(
            80.0F +
            variation * 30.0F
        ),
        static_cast<unsigned char>(
            130.0F +
            variation * 48.0F
        ),
        static_cast<unsigned char>(
            49.0F +
            variation * 24.0F
        ),
        255
    };
}

Color weed_color(
    const float variation
) {
    return {
        static_cast<unsigned char>(
            45.0F +
            variation * 22.0F
        ),
        static_cast<unsigned char>(
            117.0F +
            variation * 35.0F
        ),
        static_cast<unsigned char>(
            34.0F +
            variation * 18.0F
        ),
        255
    };
}

Color shrub_color(
    const float variation
) {
    return {
        static_cast<unsigned char>(
            49.0F +
            variation * 22.0F
        ),
        static_cast<unsigned char>(
            112.0F +
            variation * 32.0F
        ),
        static_cast<unsigned char>(
            31.0F +
            variation * 17.0F
        ),
        255
    };
}

} // namespace


FoliageSystem::FoliageSystem() = default;


float FoliageSystem::hash(
    const int x,
    const int z,
    const int salt
) {
    return assets::variation(x, z, salt);
}


void FoliageSystem::draw_grass_clump(
    const Vector3 position,
    const float height,
    const float width,
    const Color color
) {
    // Three bent, two-sided blades instead of a single green cross.
    for (int blade = 0; blade < 3; ++blade) {
        const float offset = (blade - 1) * width * 1.6F;
        const float h = height * (blade == 1 ? 1.0F : 0.73F);
        const Vector3 tip{position.x + offset + width * 1.5F,
                          position.y + h, position.z + offset};
        assets::draw_blade({position.x + offset - width, position.y, position.z + offset},
                          {position.x + offset + width, position.y, position.z + offset}, tip, color);
    }
}


void FoliageSystem::draw_weed(
    const Vector3 position,
    const float height,
    const Color color
) {
    /*
     * Three narrow blades make taller weeds
     * visibly different from ordinary grass.
     */

    draw_grass_clump(
        position,
        height,
        0.075F,
        color
    );

    draw_grass_clump(
        {
            position.x + 0.10F,
            position.y,
            position.z + 0.05F
        },
        height * 0.72F,
        0.06F,
        color
    );

    draw_grass_clump(
        {
            position.x - 0.08F,
            position.y,
            position.z - 0.07F
        },
        height * 0.85F,
        0.055F,
        color
    );
}


void FoliageSystem::draw_shrub(
    const Vector3 position,
    const float scale,
    const Color color
) {
    /*
     * Cheap stylized bush.
     *
     * Three overlapping spheres give us a
     * readable silhouette without a model.
     */

    DrawSphereEx(
        {
            position.x,
            position.y + 0.42F * scale,
            position.z
        },
        0.48F * scale,
        4, 6,
        color
    );

    DrawSphereEx(
        {
            position.x - 0.32F * scale,
            position.y + 0.31F * scale,
            position.z + 0.06F * scale
        },
        0.35F * scale,
        4, 6,
        color
    );

    DrawSphereEx(
        {
            position.x + 0.31F * scale,
            position.y + 0.34F * scale,
            position.z - 0.08F * scale
        },
        0.38F * scale,
        4, 6,
        color
    );
}


void FoliageSystem::draw_flower(
    const Vector3 position,
    const float height,
    const Color flower_color
) {
    const Vector3 flower_top{
        position.x,
        position.y + height,
        position.z
    };

    DrawLine3D(
        position,
        flower_top,
        Color{
            55,
            100,
            45,
            255
        }
    );

    DrawSphereEx(
        flower_top,
        0.055F,
        3, 5,
        flower_color
    );
}


void FoliageSystem::draw(
    const Vector3& camera_position,
    const VerdaRegion& region
) const {
    const int camera_cell_x =
        static_cast<int>(
            std::floor(
                camera_position.x /
                grass_spacing
            )
        );

    const int camera_cell_z =
        static_cast<int>(
            std::floor(
                camera_position.z /
                grass_spacing
            )
        );

    for (
        int dz = -grass_radius_cells;
        dz <= grass_radius_cells;
        ++dz
    ) {
        for (
            int dx = -grass_radius_cells;
            dx <= grass_radius_cells;
            ++dx
        ) {
            const int cell_x =
                camera_cell_x + dx;

            const int cell_z =
                camera_cell_z + dz;

            const float density =
                hash(
                    cell_x,
                    cell_z,
                    1
                );

            if (density < 0.28F) {
                continue;
            }

            const float jitter_x =
                (
                    hash(
                        cell_x,
                        cell_z,
                        2
                    ) -
                    0.5F
                ) *
                grass_spacing *
                0.78F;

            const float jitter_z =
                (
                    hash(
                        cell_x,
                        cell_z,
                        3
                    ) -
                    0.5F
                ) *
                grass_spacing *
                0.78F;

            const float world_x =
                static_cast<float>(
                    cell_x
                ) *
                grass_spacing +
                jitter_x;

            const float world_z =
                static_cast<float>(
                    cell_z
                ) *
                grass_spacing +
                jitter_z;

            const float camera_dx =
                world_x -
                camera_position.x;

            const float camera_dz =
                world_z -
                camera_position.z;

            const float distance_squared =
                camera_dx * camera_dx +
                camera_dz * camera_dz;

            if (
                distance_squared >
                maximum_draw_distance *
                maximum_draw_distance
            ) {
                continue;
            }

            // Thin the outer rings deterministically, before expensive height sampling.
            if ((distance_squared > 40.0F * 40.0F && density < 0.55F) ||
                (distance_squared > 58.0F * 58.0F && density < 0.78F)) continue;
            if (physics::WorldCollision::blocked({world_x, 0.0F, world_z}, region, 0.35F)) continue;
            bool on_road = false;
            for (const auto& settlement : region.settlements()) {
                for (const auto& road : settlement.roads) {
                    if (assets::on_road({world_x, 0.0F, world_z}, road, 0.25F)) on_road = true;
                }
            }
            if (on_road) continue;

            const float world_y =
                terrain::TerrainHeight::sample(
                    world_x,
                    world_z
                ) +
                0.025F;

            if (region.coastal_layout() && world_y <= layout::sea_level+0.025F && std::hypot(world_x,world_z)>1950) continue;

            const Vector3 position{
                world_x,
                world_y,
                world_z
            };

            // ---------------------------------------------
            // BASE GRASS
            // ---------------------------------------------

            const float grass_height =
                (0.24F +
                hash(
                    cell_x,
                    cell_z,
                    4
                ) *
                0.44F) * std::clamp((maximum_draw_distance - std::sqrt(distance_squared)) / 10.0F, 0.0F, 1.0F);

            const float grass_width =
                0.05F +
                hash(
                    cell_x,
                    cell_z,
                    5
                ) *
                0.065F;

            draw_grass_clump(
                position,
                grass_height,
                grass_width,
                grass_color(
                    hash(
                        cell_x,
                        cell_z,
                        6
                    )
                )
            );

            // ---------------------------------------------
            // TALL WEEDS
            //
            // Roughly one in seven populated cells.
            // ---------------------------------------------

            const float weed_chance =
                hash(
                    cell_x,
                    cell_z,
                    20
                );

            if (weed_chance > 0.86F && distance_squared < 38.0F * 38.0F) {
                draw_weed(
                    {
                        world_x + 0.20F,
                        world_y,
                        world_z - 0.12F
                    },
                    0.70F +
                    hash(
                        cell_x,
                        cell_z,
                        21
                    ) *
                    0.65F,
                    weed_color(
                        hash(
                            cell_x,
                            cell_z,
                            22
                        )
                    )
                );
            }

            // ---------------------------------------------
            // SHRUBS
            //
            // Much rarer and only rendered fairly close.
            // ---------------------------------------------

            const float shrub_chance =
                hash(
                    cell_x,
                    cell_z,
                    30
                );

            if (
                shrub_chance > 0.965F &&
                distance_squared <
                    48.0F * 48.0F
            ) {
                draw_shrub(
                    position,
                    0.70F +
                    hash(
                        cell_x,
                        cell_z,
                        31
                    ) *
                    0.65F,
                    shrub_color(
                        hash(
                            cell_x,
                            cell_z,
                            32
                        )
                    )
                );
            }

            // ---------------------------------------------
            // WILDFLOWERS
            // ---------------------------------------------

            const float flower_chance =
                hash(
                    cell_x,
                    cell_z,
                    40
                );

            if (
                flower_chance > 0.955F &&
                distance_squared <
                    38.0F * 38.0F
            ) {
                const float flower_type =
                    hash(
                        cell_x,
                        cell_z,
                        41
                    );

                Color flower_color{
                    235,
                    220,
                    95,
                    255
                };

                if (flower_type > 0.66F) {
                    flower_color = {
                        225,
                        225,
                        215,
                        255
                    };
                }
                else if (
                    flower_type > 0.33F
                ) {
                    flower_color = {
                        175,
                        120,
                        185,
                        255
                    };
                }

                draw_flower(
                    {
                        world_x - 0.15F,
                        world_y,
                        world_z + 0.12F
                    },
                    0.34F +
                    hash(
                        cell_x,
                        cell_z,
                        42
                    ) *
                    0.28F,
                    flower_color
                );
            }
        }
    }
}

} // namespace outland::world::foliage
