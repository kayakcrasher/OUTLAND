#pragma once
// Porto Luma's harbour (the world kit's cargo_terminal()): Stella Maris, the feeder ship that brings
// the island its supplies, and where she lands her passengers.
//
// She calls once a week: in from the open sea early on Monday morning, alongside the cargo quay
// through the morning (her brow down to the quay, the cattle gangway up her side), then away again
// at noon, out past the headland and over the horizon until the next Monday. The first time, the
// story sends her off early: she sails as soon as the player starts talking to Uncle Ron.
//
// The ship and the gangways are map assets (where she lies at her berth); the game draws them where
// this timetable has her. The arrival hall, the zebra over the quay lane and the waiting area where
// Uncle Ron meets the player stand on the quay (layout below).
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace outland::world::harbour {
// The timetable, in island minutes into the week (Monday 00:00 = 0).
inline constexpr double week = 7.0 * 24 * 60;
inline constexpr double inbound_from = 4 * 60;    // Monday 04:00: in from the sea...
inline constexpr double berthed_from = 5 * 60;    // ...alongside at 05:00...
inline constexpr double sails_at = 12 * 60;       // ...away at noon...
inline constexpr double passage = 60;             // ...an hour to or from the open sea

// Her way out, from her berth (offsets in metres from where she lies alongside): off the quay
// sideways (her tugs), then round to the north-east and away. Her way in is the same, reversed.
inline constexpr std::array<Vector2, 6> route{{{0, 0}, {28, 0}, {70, -30}, {220, -170}, {700, -620}, {1500, -1350}}};
inline constexpr float sideways = .14F;           // the share of the passage spent coming off (or on) the berth

// The arrival: where the brow lands on the quay, the hall, the zebra over the lane, the waiting area.
inline constexpr Vector3 brow_foot{2231.6F, 0, -433};
inline constexpr Vector3 hall{2230, 0, -421};      // its doors at z -428 (to the ship) and -414 (out)
inline constexpr Vector3 crossing{2214, 0, -409};  // across the quay lane
inline constexpr Vector3 waiting{2199, 0, -408};   // where Uncle Ron waits, by the pick-up bay
inline constexpr Vector3 pickup_bay{2194, 0, -400};
inline constexpr float gate_x = 1928;              // the port's gate over the terminal road (z = layout::cargo_z)

struct Pose {
    Vector2 offset{};     // from her berth, metres (x, z)
    float yaw{0};         // degrees about Y (0: as at her berth, bow to the north)
    bool alongside{true}; // at her berth, still: the brow and the cattle gangway are down
    bool at_sea{false};   // gone: over the horizon (not drawn)
};

namespace detail {
inline float smooth(float v) { v = std::clamp(v, 0.0F, 1.0F); return v * v * (3 - 2 * v); }
inline float length() {
    float total = 0;
    for (std::size_t i = 1; i < route.size(); ++i) total += std::hypot(route[i].x - route[i - 1].x, route[i].y - route[i - 1].y);
    return total;
}
// The point `d` metres along the route, and the way it runs there.
inline Vector2 along(float d, Vector2* heading = nullptr) {
    for (std::size_t i = 1; i < route.size(); ++i) {
        const Vector2 a = route[i - 1], b = route[i];
        const float l = std::hypot(b.x - a.x, b.y - a.y);
        if (d <= l || i + 1 == route.size()) {
            const float t = std::clamp(d / std::max(l, 1e-3F), 0.0F, 1.0F);
            if (heading) *heading = {(b.x - a.x) / l, (b.y - a.y) / l};
            return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        }
        d -= l;
    }
    return route.back();
}
inline float angle_lerp(float a, float b, float t) { return a + std::remainder(b - a, 360.0F) * t; }
}

// `t` along her way out: 0 at her berth, 1 over the horizon; `inbound`: the same water the other
// way, bow first.
inline Pose sailing(float t, bool inbound) {
    using namespace detail;
    t = std::clamp(t, 0.0F, 1.0F);
    Pose p;
    p.alongside = t <= 0;
    p.at_sea = t >= 1;
    const float first = std::hypot(route[1].x, route[1].y), total = length();
    float d;
    if (t < sideways) d = first * smooth(t / sideways);                                   // the tugs: slow off the quay
    else d = first + (total - first) * std::pow((t - sideways) / (1 - sideways), 1.6F);   // then under way, gathering speed
    Vector2 h{};
    p.offset = along(d, &h);
    // Bow along the way she goes (the model's bow is -Z): out, she swings from north to her heading
    // once clear of the quay; in, she comes bow first and swings round to lie alongside.
    const float course = std::atan2(-h.x, -h.y) * 180 / 3.14159265F + (inbound ? 180.0F : 0.0F);
    const float swing = smooth((d - first) / 60);
    p.yaw = d <= first ? 0.0F : angle_lerp(0.0F, course, swing);
    return p;
}

// Where she is at `minutes` of island time. `sailed_early` (island minutes, < 0: not): she left
// her berth then instead of at noon. `hold`: she waits alongside whatever the hour (the opening, when
// the player has just come down her brow).
inline Pose at(double minutes, double sailed_early = -1, bool hold = false) {
    if (hold) return {};
    double w = std::fmod(minutes, week);
    if (w < 0) w += week;
    const double week_start = minutes - w;
    double leave = sails_at;
    if (sailed_early >= 0 && sailed_early - week_start >= inbound_from && sailed_early - week_start < sails_at) leave = sailed_early - week_start;
    if (w < inbound_from || w >= leave + passage) return {{}, 0, false, true};
    if (w < berthed_from) return sailing(static_cast<float>(1 - (w - inbound_from) / passage), true);
    if (w < leave) return {};
    return sailing(static_cast<float>((w - leave) / passage), false);
}
} // namespace outland::world::harbour
