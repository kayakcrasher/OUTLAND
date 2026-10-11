#pragma once
#include <raylib.h>
#include <array>
#include <cmath>

// A bus turnaround: where a bus line has to come back the way it came (a town at the end of its
// road), a short stem leads to a paved ring the bus drives round instead of spinning on the spot.
// The stem leaves the road to one side past the stop, or carries straight on from a dead end
// (a turning circle). The map kit builds both as roads of `width`, a width no other road uses:
// that is how the bus line recognises them.
namespace outland::world::roads {

struct Turnaround {
    Vector3 junction{};               // where the stem meets the road
    Vector3 stem_end{};               // where the stem meets the ring
    Vector3 centre{};
    std::array<Vector3, 8> ring{};    // octagon, ring[0] == stem_end, going round
    static constexpr float radius = 13;
    static constexpr float width = 7.25F;
    // Side turnarounds: distances past the stop to try, in order, for the stem's junction.
    static constexpr std::array<float, 6> beyond{28, 40, 52, 64, 76, 88};
};

inline bool turnaround_road(float road_width) { return std::abs(road_width - Turnaround::width) < .001F; }

// From `junction`, a stem `stem` metres long heading `out` (unit, flat), then the ring.
inline Turnaround turnaround_at(Vector3 junction, Vector3 out, float stem) {
    Turnaround t;
    t.junction = junction;
    t.stem_end = {junction.x + out.x * stem, 0, junction.z + out.z * stem};
    t.centre = {t.stem_end.x + out.x * Turnaround::radius, 0, t.stem_end.z + out.z * Turnaround::radius};
    for (int k = 0; k < 8; ++k) {
        const float a = static_cast<float>(k) * PI / 4;
        const Vector3 from{-out.x, 0, -out.z}; // the stem side of the centre
        const Vector3 rotated{from.x * std::cos(a) - from.z * std::sin(a), 0, from.x * std::sin(a) + from.z * std::cos(a)};
        t.ring[static_cast<std::size_t>(k)] = {t.centre.x + rotated.x * Turnaround::radius, 0, t.centre.z + rotated.z * Turnaround::radius};
    }
    return t;
}

}
