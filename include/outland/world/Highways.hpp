#pragma once
#include <raylib.h>
#include <string_view>
#include <vector>

// Verda's highways: the ring road round the island and the four spokes from Verda City (north to
// Espera, east to Porto Luma, south to Suda Haveno, west to Roka). Each is a divided highway: two
// lanes each way with a hard shoulder, a Jersey-barrier median and a gravel verge, on a graded
// roadbed (level across, eased along: the land is cut and filled to it). The world kit lays the
// centre lines below as roads of type RoadType::Highway; the terrain grades to them; the renderer
// (engine/render/HighwayRender) draws them with the asset pack's road textures and barriers.
namespace outland::world::highways {

// Cross-section, metres from the centre line.
inline constexpr float median_half = 1.0F;                          // the barrier, with a paved strip either side
inline constexpr float lane_width = 3.5F;
inline constexpr float lanes_outer = median_half + 2 * lane_width;  // 8: the outer (slow) lane's edge line
inline constexpr float paved_half = lanes_outer + 1.4F;             // 9.4: the hard shoulder's edge
inline constexpr float width = 2 * paved_half;                      // a highway Road's width
inline constexpr float verge = 1.8F;                                // gravel beyond the shoulder
inline constexpr float slow_lane = median_half + lane_width * 1.5F; // 6.25: traffic keeps right, in the outer lane
inline constexpr float fast_lane = median_half + lane_width * .5F;  // 2.75
inline constexpr float barrier_half = .41F;                         // Jersey_barrier.glb: 2.48 m long, .82 m wide
inline constexpr float barrier_length = 2.48F;
// The roadbed: level over the whole width, then blending back into the land.
inline constexpr float grade_flat = paved_half + verge + 1.2F;
inline constexpr float grade_band = 28;
// Wider than this, a road is a highway for traffic (lanes) and placement.
inline constexpr float min_width = 17;   // (downtown's 16 m streets are streets)

struct Line {
    std::string_view name;
    std::vector<Vector3> points;   // the smoothed centre line (y = 0); a loop does not repeat its first point
    bool loop{false};
};
// The network, curves smoothed (corners filleted, the ring a true circle).
const std::vector<Line>& lines();

// The land at (x, z) once cut and filled to the highways, given its natural height there.
// `natural` is the land without highways (used once, to lay the road's profile); `pad` is how far
// (0..1) a building pad - a town's, a hamlet's, a ranch's - has levelled the land at a point. A
// highway runs level across a pad (its profile keeps to the pad), and the pad keeps its own ground.
using HeightFn = float (*)(float, float);
float graded(float x, float z, float height, HeightFn natural, HeightFn pad);

}
