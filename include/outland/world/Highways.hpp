#pragma once
#include <raylib.h>
#include <string_view>
#include <vector>

// Verda's highways: the ring road round the island and the two highways across it, both right
// through Verda City (north-south, Espera to Suda Haveno; east-west, Porto Luma to Roka). Each is
// a divided highway: two lanes each way with a hard shoulder, a Jersey-barrier median and a gravel verge, on a graded
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
// A pad standing well above or below the roadbed meets it in a bank this wide past the shoulder.
inline constexpr float pad_bank = 10;
// Wider than this, a road is a highway for traffic (lanes) and placement.
inline constexpr float min_width = 17;   // (downtown's 16 m streets are streets)

// Through Verda City the east-west highway is the city's main avenue: a boulevard this wide (two
// lanes each way, a planted median), the width of the avenue it replaced.
inline constexpr float boulevard_width = 16;

struct Line {
    std::string_view name;
    std::vector<Vector3> points;   // the smoothed centre line (y = 0); a loop does not repeat its first point
    bool loop{false};
    // Metres along the line (from its first point) where it runs through a city as a boulevard,
    // boulevard_width wide (none: from > to).
    float city_from{1}, city_to{0};
};
// The network, curves smoothed (corners filleted, the ring a true circle).
const std::vector<Line>& lines();

// Where a highway crosses the ring, an interchange: the ring rides over a roundabout on a bridge
// (it never stops, like a giant roundabout itself), and the crossing highway runs into the roundabout
// from both sides. Slip roads leave the ring before the bridge and run beside it, at the
// roundabout's level, into the roundabout; others run back up onto the ring beyond it.
inline constexpr float overpass_rise = 7.5F;        // the ring's roadway above the roundabout's
inline constexpr float roundabout_radius = 40;      // the circulating lane's centre line
inline constexpr float roundabout_width = 8;
inline constexpr float roundabout_pad = roundabout_radius + roundabout_width * .5F + 3;   // its level ground
inline constexpr float deck_half = 50;              // the ring's bridge over it, half its length
inline constexpr float slip_offset = 22;            // the slip roads run this far out from the ring's centre line
inline constexpr float slip_reach = 172;            // and leave it, or join it, this far from the crossing
inline constexpr float slip_width = 7;
struct Slip {
    std::vector<Vector3> points;   // in driving order (one way)
    bool off{false};               // off the ring (else back onto it)
    bool inside{false};            // inside the ring (its forward carriageway) or outside (the other)
};
struct Interchange {
    int number{0};                 // the exit's number, clockwise round the ring from the north
    std::string_view spoke;        // the highway crossing the ring
    std::string_view town;         // the town it runs on to beyond the ring
    Vector3 at{};                  // where the centre lines cross: the roundabout's centre
    Vector3 ring_forward{};        // unit, along the ring the way its points run
    Vector3 outward{};             // unit, along the highway away from the island's middle
    float ring_s{0}, spoke_s{0};   // metres along each line
    std::vector<Slip> slips;       // four: off and on, inside and outside the ring
};
const std::vector<Interchange>& interchanges();
// The ring's bridge deck at (x, z), if there is one: its roadway height in `y`. With `heading`, only
// for something travelling along the ring (a car placed on the spoke below is not on the bridge).
bool deck(float x, float z, float& y, const Vector3* heading = nullptr);

// The land at (x, z) once cut and filled to the highways, given its natural height there.
// `natural` is the land without highways (used once, to lay the road's profile); `pad` is how far
// (0..1) a building pad - a town's, a hamlet's, a ranch's - has levelled the land at a point. A
// highway runs level across a pad (its profile keeps to the pad), and the pad keeps its own ground.
using HeightFn = float (*)(float, float);
float graded(float x, float z, float height, HeightFn natural, HeightFn pad);

}
