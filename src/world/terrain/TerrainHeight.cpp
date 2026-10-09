#include "outland/world/terrain/TerrainHeight.hpp"

#include <cmath>
#include <algorithm>
#include "outland/world/VerdaLayout.hpp"

namespace outland::world::terrain {

namespace {
bool coastal_enabled = false;
float smooth(float value) { value=std::clamp(value,0.0F,1.0F); return value*value*(3-2*value); }

/*
 * Deterministic pseudo-noise.
 *
 * No random state.
 * No external noise library.
 * Same coordinates always produce
 * the same Verdan landscape.
 */
float pseudo_noise(
    const float x,
    const float z
) {
    const float a =
        std::sin(
            x * 0.0173F +
            z * 0.0117F
        );

    const float b =
        std::sin(
            x * 0.0091F -
            z * 0.0153F +
            1.73F
        );

    const float c =
        std::cos(
            x * 0.0237F +
            z * 0.0191F +
            0.81F
        );

    return (
        a +
        b * 0.65F +
        c * 0.35F
    ) / 2.0F;
}

/*
 * Creates broad ridge-like terrain rather
 * than perfectly rounded sine-wave hills.
 */
float ridge_noise(
    const float x,
    const float z
) {
    const float n =
        pseudo_noise(
            x,
            z
        );

    return
        1.0F -
        std::fabs(n);
}

} // namespace


void TerrainHeight::set_coastal_layout(bool enabled) { coastal_enabled=enabled; }
bool TerrainHeight::coastal_layout() { return coastal_enabled; }

float TerrainHeight::large_hills(
    const float x,
    const float z
) {
    /*
     * Broad geography.
     *
     * These features should be visible
     * hundreds of meters away.
     */

    const float ridge_a =
        ridge_noise(
            x * 0.55F,
            z * 0.55F
        );

    const float ridge_b =
        ridge_noise(
            x * 0.31F + 240.0F,
            z * 0.31F - 170.0F
        );

    const float valley =
        std::sin(
            x * 0.0032F +
            z * 0.0021F
        );

    return
        ridge_a * 15.0F +
        ridge_b * 8.0F +
        valley * 5.0F -
        13.0F;
}


float TerrainHeight::rolling_ground(
    const float x,
    const float z
) {
    /*
     * Medium-scale countryside.
     */

    const float rolling_a =
        pseudo_noise(
            x * 1.4F,
            z * 1.4F
        );

    const float rolling_b =
        pseudo_noise(
            x * 2.3F + 91.0F,
            z * 2.3F - 47.0F
        );

    return
        rolling_a * 4.5F +
        rolling_b * 2.0F;
}


float TerrainHeight::small_variation(
    const float x,
    const float z
) {
    /*
     * Small terrain imperfections.
     *
     * Kept deliberately subtle because
     * excessive amplitude here would make
     * roads/buildings painful later.
     */

    const float detail =
        pseudo_noise(
            x * 5.5F,
            z * 5.5F
        );

    return
        detail * 0.75F;
}


float TerrainHeight::sample(
    const float world_x,
    const float world_z
) {
    float height =
        large_hills(
            world_x,
            world_z
        );

    height +=
        rolling_ground(
            world_x,
            world_z
        );

    height +=
        small_variation(
            world_x,
            world_z
        );

    /*
     * TRAINING BASIN
     *
     * OUTLAND's starting training area gets
     * gentler terrain without becoming an
     * artificial perfectly-flat rectangle.
     *
     * 0m   -> strongest flattening
     * 45m  -> transition begins
     * 95m  -> untouched natural terrain
     */

    const float distance =
        std::sqrt(
            world_x * world_x +
            world_z * world_z
        );

    if (distance < 95.0F) {

        float natural_weight =
            (
                distance -
                45.0F
            ) / 50.0F;

        if (natural_weight < 0.0F) {
            natural_weight = 0.0F;
        }

        if (natural_weight > 1.0F) {
            natural_weight = 1.0F;
        }

        /*
         * Smoothstep.
         *
         * Prevents an obvious circular ridge
         * around the training area.
         */
        natural_weight =
            natural_weight *
            natural_weight *
            (
                3.0F -
                2.0F *
                natural_weight
            );

        const float training_ground =
            pseudo_noise(
                world_x * 2.0F,
                world_z * 2.0F
            ) * 0.55F;

        height =
            training_ground *
            (
                1.0F -
                natural_weight
            ) +
            height *
            natural_weight;
    }

    if (coastal_enabled) {
        // Flat central building pad, then the original countryside.
        height *= smooth((distance-layout::capital_flat_radius)/80.0F);
        // Small building pads retain hills and wilderness between towns.
        for (std::size_t i=1;i<layout::sites.size();++i) {
            const auto c=layout::sites[i].center;
            const float d=std::hypot(world_x-c.x,world_z-c.z);
            if(d<150) {
                const float pad=std::max(6.0F,large_hills(c.x,c.z)+rolling_ground(c.x,c.z)+small_variation(c.x,c.z));
                const float blend=smooth((d-85)/65);
                height=pad*(1-blend)+height*blend;
            }
        }
        const float shore=layout::shore_radius(world_x,world_z);
        const float blend=smooth((distance-(shore-layout::shoreline_band))/(2*layout::shoreline_band));
        if (distance>shore-layout::shoreline_band) height=height*(1-blend)+(layout::sea_level-8.0F)*blend;
    }
    return height;
}

} // namespace outland::world::terrain
