#pragma once
#include "outland/world/Zones.hpp"
#include <raylib.h>
#include <array>
#include <cmath>
#include <string_view>

// The Vakeroj's ranches: five family farms spread over Verda, one in each county, each beside a
// highway. Each family grows a field of corn and one of wheat and runs a herd of beef cattle in a
// fenced pasture; a corral and a loading chute behind the pasture, off the highway on the farm's
// own dirt track, are where the cattle truck picks up.
//
// Every ranch is laid out in its own frame: the origin is the gate on the highway's verge, +v runs
// up the farm track away from the road and +u along the road (to the right looking up the track).
// The world kit (dev::build_verda_towns) places the pieces; the game finds a ranch again from its
// settlement ("ranch_..."), whose first road is the farm track, gate to yard.
namespace outland::world::ranches {

struct Ranch {
    std::string_view id;       // settlement id
    std::string_view name;     // on the map
    std::string_view family;   // the Vakero family
    zones::ZoneId county;
    Vector2 gate;              // world x, z of the gate on the highway verge
    Vector2 up_track;          // unit, world x, z
};

// Sites on open, gently rolling land beside a highway out in each county, clear of towns and
// roads (chosen once by searching the island; the terrain levels a pad round each farm).
inline constexpr std::array<Ranch, 5> ranches{{
    {"ranch_kampoj", "Ranĉo Verdaj Kampoj", "Garza", zones::ZoneId::Verdkerno, {-12.50F, -551.00F}, {-1.0F, 0.0F}},
    {"ranch_montaro", "Ranĉo Montaro", "Montoya", zones::ZoneId::Altverdo, {-408.61F, -1455.97F}, {-0.15129F, -0.98849F}},
    {"ranch_sunlevigo", "Ranĉo Sunleviĝo", "Villarreal", zones::ZoneId::Lumamaro, {731.56F, -180.10F}, {-0.22282F, -0.97486F}},
    {"ranch_kanoj", "Ranĉo Kanoj", "Cortez", zones::ZoneId::Marcfluo, {-18.44F, 1487.09F}, {-0.04145F, -0.99914F}},
    {"ranch_pinarbo", "Ranĉo Pinarbo", "Salazar", zones::ZoneId::Ferarbaro, {-1478.46F, -160.52F}, {0.97928F, 0.20252F}},
}};

// A rectangle in the ranch frame (u0..u1 along the road, v0..v1 up the track).
struct Area { float u0, u1, v0, v1; };

// The layout (metres, ranch frame).
inline constexpr float track_length = 55;                 // gate to yard
inline constexpr Vector2 yard{0, 55};
inline constexpr Vector2 house{-20, 66};                  // farmhouse centre (front door toward the track)
inline constexpr Vector2 barn{24, 68};                    // barn centre (long axis up the track)
inline constexpr Vector2 bins[2]{{44, 48}, {44, 58}};
inline constexpr Vector2 windmill{-58, 64};
inline constexpr Area pasture{-84, -32, 28, 100};         // fenced grazing
inline constexpr Area corral{-60, -40, 104, 118};         // pen behind the pasture, beside the chute
inline constexpr Area lane{-53, -47, 100, 104};           // the open way between pasture and corral
// The cattle track: a dirt loop for the cattle truck, off the farm track at the yard, up between
// the wheat and the barn, along the back of the farm past the chute and down the far side of the
// pasture to the highway again, so a rig never has to back up or stop on the highway.
inline constexpr Vector2 cattle_track[]{{0, 55}, {10, 62}, {10, 90}, {20, 100}, {20, 127}, {-96, 127}, {-96, 12}, {-90, 4}};
// The loading point: the truck stands on the back track (load frame origin 3 m on), the chute runs
// from its high end at the track into the corral. In the load frame +v runs from the track into
// the corral, so the chute's numbers read like the stockyard's and the cattle quay's.
inline constexpr Vector2 load_gate{-50, 124};
inline constexpr Vector2 chute{0, 2.5F};                  // loading chute, in the load frame
inline constexpr Area corn{30, 66, 84, 120};              // 3 x 3 tiles of 12 m
inline constexpr Area wheat{-26, 10, 96, 120};            // 3 x 2 tiles
inline constexpr Vector2 sxs{8, 50};                      // the side-by-side's spot in the yard
inline constexpr Area footprint{-100, 74, 4, 132};        // everything (kept clear and level)

// The stockyard (Bovbazaro) of Porto Luma, Verda's cattle market, where the cattle truck unloads:
// on the ring road at the edge of the port city. A receiving pen behind the chute opens into two
// big holding pens; a sale barn and an office behind; the big rigs park beside it.
inline constexpr std::string_view stockyard_id = "ranch_bovbazaro";
inline constexpr std::string_view stockyard_name = "Bovbazaro de Porto Luma";
inline constexpr Area stockyard_receiving{-12, 12, 8, 26};
inline constexpr Area stockyard_pens[2]{{-42, -14, 8, 42}, {14, 42, 8, 42}};
inline constexpr Vector2 stockyard_chute{0, 2.5F};
inline constexpr Area stockyard_footprint{-46, 62, 4, 64};
inline constexpr Vector2 stockyard_gate{1289.34F, -790.08F}, stockyard_up{0.8466F, -0.53223F};

// The cattle quay at Porto Luma's cargo terminal (world::layout::cargo_*), where the ranches' cattle
// are shipped: the frame's gate is on the quay lane (x 2214) and +v runs west, away from the ship,
// into a pen behind a loading chute. From the pen the cattle walk across the lane and up the
// gangway (foot to top, world x/z) into the livestock deck of the ship at the berth.
inline constexpr std::string_view cattle_quay_name = "Brutkajo de Porto Luma";
inline constexpr Vector2 cattle_quay_gate{2209, -430}, cattle_quay_up{-1, 0};
inline constexpr Area cattle_quay_pen{-10, 10, 8, 24};
inline constexpr Vector2 cattle_quay_chute{0, 2.5F};
inline constexpr Vector2 cattle_gangway_foot{2225, -447}, cattle_gangway_top{2244, -447};

// The level pads the terrain makes for them (ranch frame centre, radius, blend).
inline constexpr Vector2 pad_centre{-8, 64};
inline constexpr float pad_radius = 112, pad_blend = 60;
inline constexpr Vector2 stockyard_pad_centre{8, 34};
inline constexpr float stockyard_pad_radius = 64;

// The frame from the gate and the direction up the track.
struct Frame {
    Frame() = default;
    Frame(Vector2 g, Vector2 u) : gate{g.x, 0, g.y}, up_track{u.x, 0, u.y} {}
    Vector3 gate{};
    Vector3 up_track{0, 0, 1};   // unit, level
    [[nodiscard]] Vector3 right() const { return {-up_track.z, 0, up_track.x}; }
    [[nodiscard]] Vector3 at(float u, float v) const {
        const Vector3 r = right();
        return {gate.x + r.x * u + up_track.x * v, gate.y, gate.z + r.z * u + up_track.z * v};
    }
    [[nodiscard]] Vector3 at(Vector2 p) const { return at(p.x, p.y); }
    // Game yaw (degrees) of something facing up the track.
    [[nodiscard]] float yaw() const { return std::atan2(up_track.x, up_track.z) * RAD2DEG; }
    // A world point in the frame (u, v).
    [[nodiscard]] Vector2 local(Vector3 p) const {
        const Vector3 r = right();
        const float dx = p.x - gate.x, dz = p.z - gate.z;
        return {dx * r.x + dz * r.z, dx * up_track.x + dz * up_track.z};
    }
    [[nodiscard]] bool inside(Vector3 p, Area a) const {
        const Vector2 l = local(p);
        return l.x >= a.u0 && l.x <= a.u1 && l.y >= a.v0 && l.y <= a.v1;
    }
};

// A ranch's load frame: origin on the back track at the chute, +v from the track into the corral.
inline Frame load_frame(const Frame& ranch) {
    const Vector3 g = ranch.at(load_gate);
    return Frame({g.x, g.z}, {-ranch.up_track.x, -ranch.up_track.z});
}

} // namespace outland::world::ranches
