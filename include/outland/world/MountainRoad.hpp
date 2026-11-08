#pragma once
#include <raylib.h>
#include <vector>

// Sovaĝa Monto, Verda's highest mountain, and the dirt road over its shoulder.
//
// A range of peaks stands along the west coast outside the ring road, from above Sovaĝejo north to
// Ranĉo Montaro; its summit, Sovaĝa Monto, rises to about 290 m (the rest of the island stays
// under 30). Sovaĝejo's dirt road carries on through the trailer park's north gate and winds up the
// range's landward flank, through the saddles between the peaks, past the summit at about 110 m,
// and down two hairpins to the ring road, about 3 km in all.
//
// The road's profile (its height along it) follows the land, eased so it never climbs or drops
// more than max_grade; the land is then cut and filled to it: level across the roadbed, banked
// back into the mountainside beside it (wider banks where the cut or fill is deeper). Its pullouts
// (a lay-by, parking, a trailer and a light) widen the roadbed on one side and run level, and the
// fork to Supra Sovaĝejo - six trailers on a bench above the sea, still Sovaĝejo's - leaves from a
// level stretch, so the land meets every road exactly: what you drive on is the ground.
namespace outland::world::mountain {

inline constexpr float road_width = 6;          // the dirt road
inline constexpr float bed_half = 5.5F;         // its level roadbed: the road and a shoulder each side
inline constexpr float max_grade = .095F;
inline constexpr float step = 4;                // metres between the road's samples
inline constexpr float lane_width = 5;          // Supra Sovaĝejo's gravel lane and the lay-bys
inline constexpr float layby_width = 6;

// The road: samples every `step` metres from the trailer park's north gate to the ring road's
// verge. y is the roadbed's height.
struct Line {
    std::vector<Vector3> points;
    std::vector<float> s;       // metres along
    [[nodiscard]] float length() const { return s.empty() ? 0 : s.back(); }
    [[nodiscard]] Vector3 at(float metres) const;      // on the centre line, at the roadbed's height
    [[nodiscard]] Vector3 heading(float metres) const; // unit, level, the way the samples run
};
const Line& road();

// A pullout every few hundred metres: the roadbed widened on one side into a level bench with a
// gravel lay-by, a row of parking bays, a trailer home and a street light. One is the lookout under
// the summit.
inline constexpr float pullout_bench = 16;      // the bench's reach past the roadbed
inline constexpr float pullout_core = 22;       // half its length along the road, at full width
struct Pullout {
    float s{0};
    float side{1};              // +1: to the left of the road's run (heading x up), -1: right
    bool lookout{false};
    Vector3 centre{};           // on the road's centre line
    Vector3 along{}, out{};     // unit: the road's run, and across it toward the bench
    // `a` metres along the road from the centre, `lateral` out toward the bench; y = the roadbed.
    [[nodiscard]] Vector3 at(float a, float lateral) const;
};
const std::vector<Pullout>& pullouts();

// Supra Sovaĝejo: a level pad on the bench seaward of the road, reached by a gravel lane from
// the fork, level with the road there.
inline constexpr float hamlet_flat = 36, hamlet_blend = 26;
struct Hamlet {
    Vector3 fork{};             // on the road's centre line (y: the level)
    Vector3 centre{};           // the pad's centre (y: the level)
    Vector3 lane{};             // unit: from the fork out along the lane
    float fork_s{0};
    [[nodiscard]] Vector3 at(float a, float lateral) const;   // from the centre: `a` along the lane, `lateral` to its left
};
const Hamlet& hamlet();

// The summit (y: its height).
Vector3 summit();

// The range's height above the land at (x, z) (0 away from it).
float massif(float x, float z);

// The land at (x, z) cut and filled to the road, its pullouts, the lane and Supra Sovaĝejo's pad,
// given its height there before (`height`). The road's profile is laid once, from the land before
// any of this (TerrainHeight::ungraded).
float graded(float x, float z, float height);

// Within `margin` of the road, the lane or the pad (the ground the road shapes).
bool near(float x, float z, float margin);

} // namespace outland::world::mountain
