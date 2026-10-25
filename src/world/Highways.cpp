#include "outland/world/Highways.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace outland::world::highways {
namespace {
constexpr float pi = 3.14159265358979F;
constexpr float ring_radius = 1500;
constexpr float fillet = 240;        // corner radius of the spokes
constexpr float arc_step = 3 * pi / 180;
constexpr float longest = 60;        // straights split into pieces no longer than this

struct V2 { float x, z; };
V2 operator-(V2 a, V2 b) { return {a.x - b.x, a.z - b.z}; }
V2 operator+(V2 a, V2 b) { return {a.x + b.x, a.z + b.z}; }
V2 operator*(V2 a, float s) { return {a.x * s, a.z * s}; }
float length(V2 a) { return std::hypot(a.x, a.z); }
V2 unit(V2 a) { const float l = length(a); return l < 1e-6F ? V2{0, 1} : V2{a.x / l, a.z / l}; }

void push(std::vector<Vector3>& out, V2 p) {
    if (!out.empty() && std::hypot(out.back().x - p.x, out.back().z - p.z) < .01F) return;
    out.push_back({p.x, 0, p.z});
}
// A straight from the last point to p, in pieces no longer than `longest`.
void straight(std::vector<Vector3>& out, V2 p) {
    if (out.empty()) {push(out, p); return;}
    const V2 a{out.back().x, out.back().z};
    const int pieces = std::max(1, static_cast<int>(std::ceil(length(p - a) / longest)));
    for (int i = 1; i <= pieces; ++i) push(out, a + (p - a) * (static_cast<float>(i) / static_cast<float>(pieces)));
}
// A polyline with its corners rounded off by circular arcs (radius `fillet`, less where the legs are short).
std::vector<Vector3> rounded(std::initializer_list<V2> corners) {
    const std::vector<V2> p(corners);
    std::vector<Vector3> out;
    push(out, p.front());
    for (std::size_t i = 1; i + 1 < p.size(); ++i) {
        const V2 in = unit(p[i] - p[i - 1]), on = unit(p[i + 1] - p[i]);
        const float turn = std::acos(std::clamp(in.x * on.x + in.z * on.z, -1.0F, 1.0F));
        if (turn < .01F) {straight(out, p[i]); continue;}
        float tangent = fillet * std::tan(turn * .5F);
        tangent = std::min({tangent, length(p[i] - p[i - 1]) * .45F, length(p[i + 1] - p[i]) * .45F});
        const float radius = tangent / std::tan(turn * .5F);
        const V2 a = p[i] - in * tangent;
        straight(out, a);
        const float side = in.x * on.z - in.z * on.x > 0 ? 1.0F : -1.0F;  // + turns toward +z of +x
        const V2 left{-in.z, in.x};
        const V2 centre = a + left * (side * radius);
        const float start = std::atan2(a.z - centre.z, a.x - centre.x);
        const int steps = std::max(2, static_cast<int>(std::ceil(turn / arc_step)));
        for (int k = 1; k <= steps; ++k) {
            const float angle = start + side * turn * static_cast<float>(k) / static_cast<float>(steps);
            push(out, {centre.x + radius * std::cos(angle), centre.z + radius * std::sin(angle)});
        }
    }
    straight(out, p.back());
    return out;
}

std::vector<Line> make_lines() {
    std::vector<Line> lines;
    // The main highway runs north-south right through Verda City: from Espera's edge street, down
    // the middle of downtown as a planted boulevard (every street crosses it at grade), and on out
    // past the south blocks to Suda Haveno. The east and west spokes leave from the ends of the
    // boulevard on z = 0 and end on Porto Luma's and Roka's grids.
    lines.push_back({"north-south", rounded({{0, -1680}, {0, 300}, {105, 630}, {195, 1170}, {300, 1680}}), false});
    lines.push_back({"east", rounded({{476, 0}, {612, -140}, {1138, -260}, {1624, -440}}), false});
    lines.push_back({"west", rounded({{-476, 0}, {-612, 158}, {-1138, 292}, {-1624, 410}}), false});
    Line ring{"ring", {}, true};
    constexpr int ring_points = 144;   // 2.5 degrees, 65 m apart
    for (int i = 0; i < ring_points; ++i) {
        const float a = 2 * pi * static_cast<float>(i) / ring_points;
        ring.points.push_back({ring_radius * std::cos(a), 0, ring_radius * std::sin(a)});
    }
    lines.push_back(std::move(ring));
    return lines;
}

// ---------------------------------------------------------------- grading
// Each line's road profile: the land's height along the centre line, smoothed (a highway climbs
// and dips gently), then pinned where it must meet something: the land at a spoke's ends (the
// town it runs into) and the other highway where two cross.
struct Profile {
    std::vector<V2> points;          // the line, closed for a loop
    std::vector<float> at;           // arc length of each point
    std::vector<float> height;       // the road every `step` metres
    std::vector<char> locked;        // held level (a town's edge, a crossing)
    float total{0};
    bool loop{false};
};
constexpr float step = 8;
constexpr float smoothing = 110;     // metres either side
constexpr float pin_reach = 220;
constexpr float plateau = 24;        // level either side of a crossing or a spoke's end
constexpr float max_grade = .07F;

float profile_height(const Profile& p, float s) {
    if (p.loop) s = std::fmod(std::fmod(s, p.total) + p.total, p.total);
    else s = std::clamp(s, 0.0F, p.total);
    const float f = s / step;
    const auto n = p.height.size();
    const std::size_t i = std::min(n - 1, static_cast<std::size_t>(f));
    const std::size_t j = p.loop ? (i + 1) % n : std::min(n - 1, i + 1);
    const float t = f - static_cast<float>(i);
    return p.height[i] * (1 - t) + p.height[j] * t;
}
V2 profile_point(const Profile& p, float s) {
    for (std::size_t i = 0; i + 1 < p.points.size(); ++i)
        if (s <= p.at[i + 1] || i + 2 == p.points.size()) {
            const float l = p.at[i + 1] - p.at[i];
            const float t = l < 1e-4F ? 0 : std::clamp((s - p.at[i]) / l, 0.0F, 1.0F);
            return p.points[i] + (p.points[i + 1] - p.points[i]) * t;
        }
    return p.points.back();
}
struct Segment { int line; std::size_t index; };
struct Index {
    static constexpr float cell = 64, extent = 2400;
    static constexpr int cells = static_cast<int>(2 * extent / cell);
    std::vector<Profile> profiles;
    std::vector<std::vector<Segment>> grid;
    static int coordinate(float v) { return std::clamp(static_cast<int>(std::floor((v + extent) / cell)), 0, cells - 1); }
};

Index build(HeightFn natural, HeightFn pad) {
    Index index;
    for (const auto& line : lines()) {
        Profile p;
        p.loop = line.loop;
        for (const auto& q : line.points) p.points.push_back({q.x, q.z});
        if (line.loop) p.points.push_back(p.points.front());
        p.at.push_back(0);
        for (std::size_t i = 1; i < p.points.size(); ++i) p.at.push_back(p.at.back() + length(p.points[i] - p.points[i - 1]));
        p.total = p.at.back();
        const auto n = static_cast<std::size_t>(std::floor(p.total / step)) + (p.loop ? 0 : 1);
        std::vector<float> land(n), level(n);
        for (std::size_t k = 0; k < n; ++k) {
            const V2 q = profile_point(p, static_cast<float>(k) * step);
            land[k] = natural(q.x, q.z);
            level[k] = 1 + 60 * pad(q.x, q.z);   // a pad's ground counts for far more: the road stays level over it
        }
        // A tent-weighted moving average, wrapping round a loop and clamped at a spoke's ends.
        const int reach = static_cast<int>(smoothing / step);
        p.height.assign(n, 0);
        for (std::size_t k = 0; k < n; ++k) {
            float sum = 0, weights = 0;
            for (int o = -reach; o <= reach; ++o) {
                long j = static_cast<long>(k) + o;
                if (p.loop) j = (j % static_cast<long>(n) + static_cast<long>(n)) % static_cast<long>(n);
                else j = std::clamp(j, 0L, static_cast<long>(n) - 1);
                const float w = (1 - static_cast<float>(std::abs(o)) / static_cast<float>(reach + 1)) * level[static_cast<std::size_t>(j)];
                sum += land[static_cast<std::size_t>(j)] * w; weights += w;
            }
            p.height[k] = sum / weights;
        }
        index.profiles.push_back(std::move(p));
    }
    struct Crossing { int a, b; float sa, sb; };
    std::vector<Crossing> crossings;
    for (int a = 0; a < static_cast<int>(index.profiles.size()); ++a)
        for (int b = a + 1; b < static_cast<int>(index.profiles.size()); ++b) {
            const auto& pa = index.profiles[static_cast<std::size_t>(a)];
            const auto& pb = index.profiles[static_cast<std::size_t>(b)];
            for (std::size_t i = 0; i + 1 < pa.points.size(); ++i)
                for (std::size_t j = 0; j + 1 < pb.points.size(); ++j) {
                    const V2 r = pa.points[i + 1] - pa.points[i], q = pb.points[j + 1] - pb.points[j];
                    const float den = r.x * q.z - r.z * q.x;
                    if (std::abs(den) < 1e-6F) continue;
                    const V2 w = pb.points[j] - pa.points[i];
                    const float t = (w.x * q.z - w.z * q.x) / den, u = (w.x * r.z - w.z * r.x) / den;
                    if (t < 0 || t > 1 || u < 0 || u > 1) continue;
                    crossings.push_back({a, b, pa.at[i] + t * length(r), pb.at[j] + u * length(q)});
                }
        }
    // Holds. Over a building pad the road is the pad's level ground; a spoke's ends meet the land
    // of the town it runs into, level for a little way.
    const auto distance_along = [](const Profile& p, std::size_t k, float s) {
        float d = std::abs(static_cast<float>(k) * step - s);
        return p.loop ? std::min(d, p.total - d) : d;
    };
    for (auto& p : index.profiles) {
        p.locked.assign(p.height.size(), 0);
        for (std::size_t k = 0; k < p.height.size(); ++k) {
            const V2 q = profile_point(p, static_cast<float>(k) * step);
            if (pad(q.x, q.z) < .98F) continue;
            p.height[k] = natural(q.x, q.z);
            p.locked[k] = 1;
        }
        if (p.loop) continue;
        for (const float s : {0.0F, p.total}) {
            const V2 q = profile_point(p, s);
            const float land = natural(q.x, q.z);
            for (std::size_t k = 0; k < p.height.size(); ++k)
                if (!p.locked[k] && distance_along(p, k, s) <= plateau) {p.height[k] = land; p.locked[k] = 1;}
        }
    }
    // No grade steeper than `max_grade`: neighbours too far apart meet in the middle (the cut and
    // fill), holds staying put.
    const auto limit = [](Profile& p) {
        const auto n = p.height.size();
        const std::size_t pairs = p.loop ? n : n - 1;
        const float most = max_grade * step;
        for (int round = 0; round < 4000; ++round) {
            bool steep = false;
            for (std::size_t k = 0; k < pairs; ++k) {
                const std::size_t j = (k + 1) % n;
                const float rise = p.height[j] - p.height[k], excess = std::abs(rise) - most;
                if (excess <= 1e-4F || (p.locked[k] && p.locked[j])) continue;
                steep = true;
                const float toward = rise > 0 ? 1.0F : -1.0F;   // k up, j down
                if (p.locked[k]) p.height[j] -= toward * excess;
                else if (p.locked[j]) p.height[k] += toward * excess;
                else {p.height[k] += toward * excess * .5F; p.height[j] -= toward * excess * .5F;}
            }
            if (!steep) break;
        }
    };
    for (auto& p : index.profiles) limit(p);
    // Where two highways cross they share a level plateau, at a height both can reach from their
    // holds within the grade (as near as that allows to halfway between them).
    const auto reachable = [&](const Profile& p, float s, float& lo, float& hi) {
        for (std::size_t k = 0; k < p.height.size(); ++k) {
            if (!p.locked[k]) continue;
            const float slack = max_grade * std::max(0.0F, distance_along(p, k, s) - plateau);
            lo = std::max(lo, p.height[k] - slack);
            hi = std::min(hi, p.height[k] + slack);
        }
    };
    for (const auto& c : crossings) {
        auto& pa = index.profiles[static_cast<std::size_t>(c.a)];
        auto& pb = index.profiles[static_cast<std::size_t>(c.b)];
        float lo = -1e9F, hi = 1e9F;
        reachable(pa, c.sa, lo, hi);
        reachable(pb, c.sb, lo, hi);
        const float halfway = (profile_height(pa, c.sa) + profile_height(pb, c.sb)) * .5F;
        const float meet = lo <= hi ? std::clamp(halfway, lo, hi) : (lo + hi) * .5F;
        for (const auto& [p, s] : {std::make_pair(&pa, c.sa), std::make_pair(&pb, c.sb)})
            for (std::size_t k = 0; k < p->height.size(); ++k) {
                if (p->locked[k]) continue;
                const float d = distance_along(*p, k, s);
                if (d <= plateau) {p->height[k] = meet; p->locked[k] = 1; continue;}
                if (d >= plateau + pin_reach) continue;
                const float u = 1 - (d - plateau) / pin_reach;
                p->height[k] += (meet - p->height[k]) * u * u * (3 - 2 * u);
            }
    }
    for (auto& p : index.profiles) limit(p);
    index.grid.resize(static_cast<std::size_t>(Index::cells * Index::cells));
    const float reach = grade_flat + grade_band;
    for (int l = 0; l < static_cast<int>(index.profiles.size()); ++l) {
        const auto& p = index.profiles[static_cast<std::size_t>(l)];
        for (std::size_t i = 0; i + 1 < p.points.size(); ++i) {
            const V2 a = p.points[i], b = p.points[i + 1];
            for (int x = Index::coordinate(std::min(a.x, b.x) - reach); x <= Index::coordinate(std::max(a.x, b.x) + reach); ++x)
                for (int z = Index::coordinate(std::min(a.z, b.z) - reach); z <= Index::coordinate(std::max(a.z, b.z) + reach); ++z)
                    index.grid[static_cast<std::size_t>(x * Index::cells + z)].push_back({l, i});
        }
    }
    return index;
}
}

const std::vector<Line>& lines() {
    static const std::vector<Line> network = make_lines();
    return network;
}

float graded(float x, float z, float height, HeightFn natural, HeightFn pad) {
    static const Index index = build(natural, pad);
    if (std::abs(x) >= Index::extent || std::abs(z) >= Index::extent) return height;
    const auto& cell = index.grid[static_cast<std::size_t>(Index::coordinate(x) * Index::cells + Index::coordinate(z))];
    if (cell.empty()) return height;
    float nearest = grade_flat + grade_band, road = height;
    for (const auto& segment : cell) {
        const auto& p = index.profiles[static_cast<std::size_t>(segment.line)];
        const V2 a = p.points[segment.index], b = p.points[segment.index + 1], d = b - a;
        const float l2 = d.x * d.x + d.z * d.z;
        const float t = l2 < 1e-6F ? 0 : std::clamp(((x - a.x) * d.x + (z - a.z) * d.z) / l2, 0.0F, 1.0F);
        const float distance = std::hypot(x - (a.x + d.x * t), z - (a.z + d.z * t));
        if (distance >= nearest) continue;
        nearest = distance;
        road = profile_height(p, p.at[segment.index] + t * std::sqrt(l2));
    }
    if (nearest >= grade_flat + grade_band) return height;
    float blend = 0;
    if (nearest > grade_flat) {
        const float t = (nearest - grade_flat) / grade_band;
        blend = t * t * (3 - 2 * t);
    }
    if (nearest <= grade_flat) return road;   // the roadbed itself is always level across
    // Well inside a building pad, the pad's level ground stands (the road runs at nearly its height
    // anyway). Where the roadbed runs well below or above the pad, a short bank (pad_bank) eases
    // between them by the shoulder instead: no cliff, and the pad stays level beyond it.
    const float apart = std::clamp(1.6F - std::abs(road - height), 0.0F, 1.0F);
    const float bank = std::clamp((nearest - grade_flat) / pad_bank, 0.0F, 1.0F);
    const float levelled = std::clamp((pad(x, z) - .9F) / .1F, 0.0F, 1.0F) * std::max(apart, bank * bank * (3 - 2 * bank));
    blend = std::max(blend, levelled * levelled * (3 - 2 * levelled));
    return road * (1 - blend) + height * blend;
}

}
