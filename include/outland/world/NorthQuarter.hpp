#pragma once
// Norda Kvartalo: Verda City's north side, built like a Chicago neighbourhood on downtown's grid
// (the world kit's north_quarter()). Two rows of 68 m blocks north of downtown - the avenues carry
// on north to two new east-west streets that cross the highway at grade - each block split by an
// alley down its middle, greystone three-flats and brick walk-ups fronting the streets, corner
// taverns, murals on the end walls. Over the second row's alley runs La Levita, the elevated
// railway: a steel deck on bents, three stations, and two trains running between its ends.
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace outland::world::north {
inline constexpr std::array<float, 2> rows{-210, -294};      // the blocks' centres (z)
inline constexpr std::array<float, 2> streets{-252, -336};   // the new east-west streets (z)
inline constexpr std::array<float, 4> avenues{-171, -87, 87, 171};
inline constexpr float north_edge = 344;    // the level ground reaches z = -north_edge
inline constexpr float alley_half = 3;      // each block's alley, along x down its middle
inline constexpr float front = 30;          // the buildings' fronts: this far from a block's centre

// La Levita over the second row's alley, east-west.
inline constexpr float l_z = rows[1];
inline constexpr float l_deck = 6.8F;       // the deck's top above the street; the rails on it
inline constexpr float l_rail = .16F;
inline constexpr float l_platform = 7.9F;
inline constexpr float l_west = -150, l_east = 150;
inline constexpr std::array<float, 2> l_tracks{-1.8F, 1.8F};   // the tracks' z, from l_z
inline constexpr float l_piece = 20;        // a deck piece's length
struct Station { const char* name; float x; };
inline constexpr std::array<Station, 3> stations{{{"Okcidento", -130}, {"Centro", 45}, {"Oriento", 130}}};
inline constexpr float station_half = 20, train_half = 15;

// The trains: each shuttles on its own track - out of the west terminal, a stop at Centro, the
// east terminal, Centro, and back - braking into each stop and pulling away (cruise m/s, accel
// m/s^2), standing `dwell` s at each. The second runs half a round behind the first.
inline constexpr float cruise = 14, accel = 1.1F, dwell = 14;
inline constexpr std::array<float, 4> stops{l_west + 2 + train_half, stations[1].x, l_east - 2 - train_half, stations[1].x};

// Time to run `d` metres from a stand to a stand.
inline float leg_time(float d) {
    const float ramp = cruise * cruise / accel;   // distance spent speeding up and slowing down
    return d >= ramp ? d / cruise + cruise / accel : 2 * std::sqrt(d / accel);
}
// Distance covered `t` s into a run of `d` metres.
inline float leg_distance(float d, float t) {
    const float total = leg_time(d);
    t = std::clamp(t, 0.0F, total);
    const float top = std::min(cruise, std::sqrt(d * accel)), ramp_t = top / accel;
    if (t < ramp_t) return .5F * accel * t * t;
    if (t > total - ramp_t) {const float r = total - t; return d - .5F * accel * r * r;}
    return .5F * accel * ramp_t * ramp_t + (t - ramp_t) * top;
}
inline float round_time() {
    float total = 0;
    for (std::size_t i = 0; i < stops.size(); ++i) total += dwell + leg_time(std::abs(stops[(i + 1) % stops.size()] - stops[i]));
    return total;
}
struct Train { Vector3 at; float speed; bool standing; };
// Train k (0 or 1) at `seconds` (any clock): its centre on the rails (y: the rails' top above the
// deck's foot, add the street's height), its speed (+ eastward) and whether it stands at a stop.
inline Train train(int k, double seconds) {
    const double round = round_time();
    double t = std::fmod(seconds + (k ? round * .5 : 0), round);
    if (t < 0) t += round;
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const float from = stops[i], to = stops[(i + 1) % stops.size()], d = std::abs(to - from);
        const float z = l_z + l_tracks[static_cast<std::size_t>(k & 1)];
        if (t < dwell) return {{from, l_deck + l_rail, z}, 0, true};
        t -= dwell;
        const float run = leg_time(d);
        if (t < run) {
            const float s = leg_distance(d, static_cast<float>(t)), dir = to > from ? 1.0F : -1.0F;
            const float ahead = leg_distance(d, static_cast<float>(t) + .05F);
            return {{from + dir * s, l_deck + l_rail, z}, dir * (ahead - s) / .05F, false};
        }
        t -= run;
    }
    return {{stops[0], l_deck + l_rail, l_z + l_tracks[static_cast<std::size_t>(k & 1)]}, 0, true};
}
}  // namespace outland::world::north
