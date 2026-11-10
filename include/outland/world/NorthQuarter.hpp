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
#include <vector>

namespace outland::world::north {
inline constexpr std::array<float, 2> rows{-210, -294};      // the blocks' centres (z)
inline constexpr std::array<float, 2> streets{-252, -336};   // the new east-west streets (z)
inline constexpr std::array<float, 4> avenues{-171, -87, 87, 171};
inline constexpr float north_edge = 344;    // the level ground reaches z = -north_edge
inline constexpr float alley_half = 3;      // each block's alley, along x down its middle
inline constexpr float front = 30;          // the buildings' fronts: this far from a block's centre

// La Levita, the elevated railway: out of Okcidenta Parko (in the west suburbs' park, the empty
// middle of their blocks), up the suburb street at x = -330 on bents either side of it, round onto
// Norda Kvartalo's second-row alley and east along it over the avenues and the highway, round again
// and down the east suburbs' street at x = 330 to Orienta Verda. The line runs on its centre line,
// its two tracks either side of it; where it crosses open country the ground is levelled under it.
inline constexpr float l_z = rows[1];
inline constexpr float l_deck = 6.8F;       // the deck's top above the street; the rails on it
inline constexpr float l_rail = .16F;
inline constexpr float l_platform = 7.9F;
inline constexpr std::array<float, 2> l_tracks{-1.8F, 1.8F};   // the tracks either side of the centre line (+: to its left)
inline constexpr float l_piece = 20;        // a straight deck piece's length (round a curve, 4.4 m)
inline constexpr float suburb_street = 330; // the suburb streets the line runs down (x = -330, 330)
inline constexpr float car_length = 14.7F, car_gap = .6F;

// The line's pieces, west to east: straights and quarter circles (centre, radius).
struct Piece { bool arc; Vector2 a, b, centre; float radius; };
inline constexpr std::array<Piece, 7> pieces{{
    {false, {-226, -35}, {-290, -35}, {}, 0},
    {true, {-290, -35}, {-330, -75}, {-290, -75}, 40},
    {false, {-330, -75}, {-330, -250}, {}, 0},
    {true, {-330, -250}, {-286, -294}, {-286, -250}, 44},
    {false, {-286, -294}, {286, -294}, {}, 0},
    {true, {286, -294}, {330, -250}, {286, -250}, 44},
    {false, {330, -250}, {330, -110}, {}, 0},
}};
struct Station { const char* name; Vector2 at; };
inline constexpr std::array<Station, 5> stations{{{"Okcidenta Parko", {-256, -35}}, {"Okcidento", {-130, -294}}, {"Centro", {45, -294}},
                                                 {"Oriento", {130, -294}}, {"Orienta Verda", {330, -136}}}};
inline constexpr float station_half = 20, train_half = car_length + car_gap * .5F;

namespace detail {
inline float piece_length(const Piece& p) {
    if (!p.arc) return std::hypot(p.b.x - p.a.x, p.b.y - p.a.y);
    const float a0 = std::atan2(p.a.y - p.centre.y, p.a.x - p.centre.x), a1 = std::atan2(p.b.y - p.centre.y, p.b.x - p.centre.x);
    return std::abs(std::remainder(a1 - a0, 6.2831853F)) * p.radius;
}
// The point `d` metres into a piece and the way the line runs there.
inline Vector2 piece_at(const Piece& p, float d, Vector2& heading) {
    const float l = piece_length(p);
    if (!p.arc) {
        heading = {(p.b.x - p.a.x) / l, (p.b.y - p.a.y) / l};
        return {p.a.x + heading.x * d, p.a.y + heading.y * d};
    }
    const float a0 = std::atan2(p.a.y - p.centre.y, p.a.x - p.centre.x), a1 = std::atan2(p.b.y - p.centre.y, p.b.x - p.centre.x);
    const float turn = std::remainder(a1 - a0, 6.2831853F), a = a0 + turn * std::clamp(d / l, 0.0F, 1.0F);
    const float sense = turn > 0 ? 1.0F : -1.0F;
    heading = {-std::sin(a) * sense, std::cos(a) * sense};
    return {p.centre.x + std::cos(a) * p.radius, p.centre.y + std::sin(a) * p.radius};
}
}
// The line's length, and the point `s` metres along it from its west end (with the way it runs).
inline float length() {
    float total = 0;
    for (const auto& p : pieces) total += detail::piece_length(p);
    return total;
}
inline Vector2 at(float s, Vector2* heading = nullptr) {
    Vector2 h{};
    s = std::clamp(s, 0.0F, length());
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        const float l = detail::piece_length(pieces[i]);
        if (s <= l || i + 1 == pieces.size()) {
            const Vector2 p = detail::piece_at(pieces[i], std::min(s, l), h);
            if (heading) *heading = h;
            return p;
        }
        s -= l;
    }
    return {};
}
// How far along the line its nearest point to (x, z) is, and how far off it (x, z) lies.
inline float along(float x, float z, float* off = nullptr) {
    float best = 1e9F, best_s = 0, base = 0;
    for (const auto& p : pieces) {
        const float l = detail::piece_length(p);
        float d, s;
        if (!p.arc) {
            const float dx = p.b.x - p.a.x, dz = p.b.y - p.a.y;
            const float t = std::clamp(((x - p.a.x) * dx + (z - p.a.y) * dz) / (l * l), 0.0F, 1.0F);
            d = std::hypot(x - p.a.x - dx * t, z - p.a.y - dz * t);
            s = t * l;
        } else {
            const float a0 = std::atan2(p.a.y - p.centre.y, p.a.x - p.centre.x), a1 = std::atan2(p.b.y - p.centre.y, p.b.x - p.centre.x);
            const float turn = std::remainder(a1 - a0, 6.2831853F);
            float t = std::remainder(std::atan2(z - p.centre.y, x - p.centre.x) - a0, 6.2831853F) / turn;
            t = std::clamp(t, 0.0F, 1.0F);
            Vector2 h{};
            const Vector2 q = detail::piece_at(p, t * l, h);
            d = std::hypot(x - q.x, z - q.y);
            s = t * l;
        }
        if (d < best) {best = d; best_s = base + s;}
        base += l;
    }
    if (off) *off = best;
    return best_s;
}
// How far (x, z) is from the line (the ground under it is levelled within reach of it).
inline float distance(float x, float z) {
    float d = 0;
    along(x, z, &d);
    return d;
}
// How far (x, z) is outside the levelled strip under the line where it crosses open country (its
// corners between Norda Kvartalo and the suburbs; 1e9 elsewhere - the city's ground is level there).
inline float corridor_outside(float x, float z) {
    if (z > -150 || z < -360 || std::abs(x) < 150 || std::abs(x) > 400) return 1e9F;
    return std::max(distance(x, z) - 14, 0.0F);
}
inline float station_s(std::size_t i) { return along(stations[i].at.x, stations[i].at.y); }
// A point beside the line: `s` along it, `lateral` to the left of the way it runs.
inline Vector2 beside(float s, float lateral) {
    Vector2 h{};
    const Vector2 p = at(s, &h);
    return {p.x - h.y * lateral, p.y + h.x * lateral};
}

// The trains: each shuttles on its own track from one end of the line to the other and back,
// stopping at every station - braking into each and pulling away (cruise m/s, accel m/s^2),
// standing `dwell` s at each. The second runs half a round behind the first.
inline constexpr float cruise = 14, accel = 1.1F, dwell = 14;
inline const std::vector<float>& stops() {
    static const std::vector<float> list = [] {
        std::vector<float> s;
        for (std::size_t i = 0; i < stations.size(); ++i) s.push_back(station_s(i));
        for (std::size_t i = stations.size() - 1; i-- > 1;) s.push_back(station_s(i));
        return s;
    }();
    return list;
}

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
    const auto& st = stops();
    float total = 0;
    for (std::size_t i = 0; i < st.size(); ++i) total += dwell + leg_time(std::abs(st[(i + 1) % st.size()] - st[i]));
    return total;
}
struct Train { float s; float speed; bool standing; int track; };
// Train k (0 or 1) at `seconds` (any clock): how far along the line its middle is, its speed
// (+ toward the east end) and whether it stands at a station; it keeps to track k.
inline Train train(int k, double seconds) {
    const auto& st = stops();
    const double round = round_time();
    double t = std::fmod(seconds + (k ? round * .5 : 0), round);
    if (t < 0) t += round;
    for (std::size_t i = 0; i < st.size(); ++i) {
        const float from = st[i], to = st[(i + 1) % st.size()], d = std::abs(to - from);
        if (t < dwell) return {from, 0, true, k & 1};
        t -= dwell;
        const float run = leg_time(d);
        if (t < run) {
            const float s = leg_distance(d, static_cast<float>(t)), dir = to > from ? 1.0F : -1.0F;
            const float ahead = leg_distance(d, static_cast<float>(t) + .05F);
            return {from + dir * s, dir * (ahead - s) / .05F, false, k & 1};
        }
        t -= run;
    }
    return {st[0], 0, true, k & 1};
}
// Where a car of a train stands: its middle on the rails (x, z; y the rails' top above the street)
// and its yaw (degrees, the model's +Z along the line). car 0 is the west-end car.
struct Car { Vector3 at; float yaw; };
inline Car car(const Train& t, int which) {
    const float s = t.s + (which ? 1.0F : -1.0F) * (car_length + car_gap) * .5F;
    Vector2 h{};
    at(s, &h);
    const Vector2 p = beside(s, l_tracks[static_cast<std::size_t>(t.track)]);
    return {{p.x, l_deck + l_rail, p.y}, std::atan2(h.x, h.y) * 180 / 3.14159265F};
}
}  // namespace outland::world::north
