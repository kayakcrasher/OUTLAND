#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/Highways.hpp"

#include <cmath>
#include <algorithm>
#include <string_view>
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/Ranches.hpp"
#include "outland/world/Ballparks.hpp"

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
    // The land, then cut and filled to the highways' roadbeds (world/Highways.hpp).
    const float land = unpaved(world_x, world_z);
    return coastal_enabled ? highways::graded(world_x, world_z, land, &TerrainHeight::unpaved, &TerrainHeight::pad_weight) : land;
}

// Mirrors the pads in unpaved() below: 1 on a pad's level ground, easing to 0 across its edge.
float TerrainHeight::pad_weight(
    const float world_x,
    const float world_z
) {
    const float distance = std::hypot(world_x, world_z);
    const float suburb=std::hypot(std::max({layout::suburb_near-std::abs(world_x),std::abs(world_x)-layout::suburb_far,0.0F}),
                                  std::max(std::abs(world_z)-layout::suburb_half,0.0F));
    float weight = 1 - smooth(std::min({distance-layout::capital_flat_radius,suburb,layout::downtown_outside(world_x,world_z),
                                        layout::freight_outside(world_x,world_z)})/80.0F);
    const auto pad=[&](float cx,float cz,float flat,float band) {
        const float d=std::hypot(world_x-cx,world_z-cz);
        if(d<flat+band) weight=std::max(weight,1-smooth((d-flat)/band));
    };
    for (std::size_t i=1;i<layout::sites.size();++i) {
        const auto c=layout::sites[i].center;
        pad(c.x,c.z,layout::sites[i].id=="port_luma"?layout::port_flat_radius:layout::sites[i].id=="west_roka"?layout::roka_flat_radius:layout::town_flat_radius,65);
    }
    pad(layout::sovagejo_center.x,layout::sovagejo_center.z,layout::sovagejo_flat_radius,60);
    for (const auto& hamlet:layout::hamlets) pad(hamlet.center.x,hamlet.center.z,layout::hamlet_flat_radius,60);
    for (const auto& ranch:ranches::ranches) {
        const auto c=ranches::Frame(ranch.gate,ranch.up_track).at(ranches::pad_centre);
        pad(c.x,c.z,ranches::pad_radius,ranches::pad_blend);
    }
    const auto c=ranches::Frame(ranches::stockyard_gate,ranches::stockyard_up).at(ranches::stockyard_pad_centre);
    pad(c.x,c.z,ranches::stockyard_pad_radius,40);
    pad(layout::prison_center.x,layout::prison_center.z,layout::prison_flat_radius,45);
    for (const auto& yard:layout::freight_yards) pad(yard.centre.x,yard.centre.z,layout::freight_yard_flat_radius,layout::freight_yard_blend);
    for (const auto& park:ballpark::parks) pad(park.centre.x,park.centre.z,ballpark::flat_radius,ballpark::flat_blend);
    return std::clamp(weight,0.0F,1.0F);
}

float TerrainHeight::unpaved(
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
        // Flat central building pad (downtown and the suburbs either side), then the original countryside.
        const float suburb=std::hypot(std::max({layout::suburb_near-std::abs(world_x),std::abs(world_x)-layout::suburb_far,0.0F}),
                                      std::max(std::abs(world_z)-layout::suburb_half,0.0F));
        height *= smooth(std::min({distance-layout::capital_flat_radius,suburb,layout::downtown_outside(world_x,world_z),
                                   layout::freight_outside(world_x,world_z)})/80.0F);
        // Small building pads retain hills and wilderness between towns.
        for (std::size_t i=1;i<layout::sites.size();++i) {
            const auto c=layout::sites[i].center;
            const float d=std::hypot(world_x-c.x,world_z-c.z);
            const float flat=layout::sites[i].id=="port_luma"?layout::port_flat_radius:layout::sites[i].id=="west_roka"?layout::roka_flat_radius:layout::town_flat_radius;
            if(d<flat+65) {
                const float pad=std::max(6.0F,large_hills(c.x,c.z)+rolling_ground(c.x,c.z)+small_variation(c.x,c.z));
                const float blend=smooth((d-flat)/65);
                height=pad*(1-blend)+height*blend;
            }
        }
        {
            const auto c=layout::sovagejo_center;
            const float d=std::hypot(world_x-c.x,world_z-c.z);
            if(d<layout::sovagejo_flat_radius+60) {
                const float pad=std::max(6.0F,large_hills(c.x,c.z)+rolling_ground(c.x,c.z)+small_variation(c.x,c.z));
                const float blend=smooth((d-layout::sovagejo_flat_radius)/60);
                height=pad*(1-blend)+height*blend;
            }
        }
        for (const auto& hamlet:layout::hamlets) {
            const auto c=hamlet.center;
            const float d=std::hypot(world_x-c.x,world_z-c.z);
            if(d<layout::hamlet_flat_radius+60) {
                const float pad=std::max(6.0F,large_hills(c.x,c.z)+rolling_ground(c.x,c.z)+small_variation(c.x,c.z));
                const float blend=smooth((d-layout::hamlet_flat_radius)/60);
                height=pad*(1-blend)+height*blend;
            }
        }
        // The Vakeroj's ranches and the stockyard: a level pad round each farm (world/Ranches.hpp).
        // `level` (when given): level with that point's pad instead of its own.
        const auto level_pad=[&](Vector3 c,float flat,float band,const Vector3* level=nullptr) {
            const float d=std::hypot(world_x-c.x,world_z-c.z);
            if(d>=flat+band) return;
            const Vector3 at=level ? *level : c;
            const float pad=std::max(6.0F,large_hills(at.x,at.z)+rolling_ground(at.x,at.z)+small_variation(at.x,at.z));
            const float blend=smooth((d-flat)/band);
            height=pad*(1-blend)+height*blend;
        };
        for (const auto& ranch:ranches::ranches)
            level_pad(ranches::Frame(ranch.gate,ranch.up_track).at(ranches::pad_centre),ranches::pad_radius,ranches::pad_blend);
        level_pad(ranches::Frame(ranches::stockyard_gate,ranches::stockyard_up).at(ranches::stockyard_pad_centre),ranches::stockyard_pad_radius,40);
        level_pad(layout::prison_center,layout::prison_flat_radius,45);   // the state prison
        for (const auto& yard:layout::freight_yards)                      // the freight yards
            level_pad(yard.centre,layout::freight_yard_flat_radius,layout::freight_yard_blend);
        // The ballparks: level with their towns' pads where they meet (Verda City's stands on its own).
        for (const auto& park:ballpark::parks) {
            const Vector3* town=nullptr;
            for (std::size_t i=1;i<layout::sites.size();++i) if (std::string_view(layout::sites[i].id)==park.site) town=&layout::sites[i].center;
            level_pad(park.centre,ballpark::flat_radius,ballpark::flat_blend,town);
        }
        const float shore=layout::shore_radius(world_x,world_z);
        const float blend=smooth((distance-(shore-layout::shoreline_band))/(2*layout::shoreline_band));
        if (distance>shore-layout::shoreline_band) height=height*(1-blend)+(layout::sea_level-8.0F)*blend;
        // Porto Luma's quay: the cargo terminal's apron and the fishers' quay south of it, one level
        // quay at the shore; the ramps down from the town to each end; the dredged berth along the
        // cargo end's face (layout::cargo_*, fish_z, quay_*).
        if (world_x>layout::cargo_ramp_x0 && world_x<layout::cargo_quay_x1+90 && world_z>layout::quay_z0-40 && world_z<layout::quay_z1+40) {
            const auto port=layout::sites[2].center;
            const float top=std::max(6.0F,large_hills(port.x,port.z)+rolling_ground(port.x,port.z)+small_variation(port.x,port.z));
            const float dz=std::abs(world_z-layout::cargo_z);
            if (world_x<=layout::cargo_quay_x1) {
                const float d=std::hypot(std::max(layout::cargo_ramp_x1-world_x,0.0F),std::max({layout::quay_z0-world_z,world_z-layout::quay_z1,0.0F}));
                const float w=1-smooth(d/25.0F);
                height=height+(layout::cargo_quay_height-height)*w;
            } else if (dz<layout::cargo_quay_half+40) {
                const float w=(1-smooth((world_x-layout::cargo_quay_x1-45)/40.0F))*(1-smooth((dz-layout::cargo_quay_half-10)/25.0F));
                height=std::min(height,height+(layout::cargo_berth_depth-height)*w);
            }
            // (The ramps last: their grade stays even right down to the quay.)
            if (world_x<layout::cargo_ramp_x1)
                for (const float ramp_z : {layout::cargo_z, layout::fish_z}) {
                    const float t=(world_x-layout::cargo_ramp_x0)/(layout::cargo_ramp_x1-layout::cargo_ramp_x0);
                    const float ramp=top+(layout::cargo_quay_height-top)*t;
                    const float w=1-smooth((std::abs(world_z-ramp_z)-layout::cargo_ramp_half)/22.0F);
                    height=height+(ramp-height)*w;
                }
        }
    }
    return height;
}

} // namespace outland::world::terrain
