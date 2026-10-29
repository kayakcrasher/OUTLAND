#include "outland/world/Highways.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
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
constexpr float city_edge = 476;     // Verda City's suburbs end at x = +-476 (their outer streets)
constexpr bool interchanges_open = false;   // (being built)
constexpr float city_corner = 530;   // where the east-west highway turns off z = 0 (its curve clear of the stadium)

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
    // past the south blocks to Suda Haveno.
    lines.push_back({"north-south", rounded({{0, -1680}, {0, 300}, {105, 630}, {195, 1170}, {300, 1680}}), false});
    // East to west, Porto Luma to Roka, straight through Verda City on z = 0: the city's main avenue
    // from the east suburbs' edge to the west suburbs' (the corners just outside them).
    Line east_west{"east-west", rounded({{1624, -440}, {1138, -260}, {612, -140}, {city_corner, 0}, {-city_corner, 0}, {-612, 158}, {-1138, 292}, {-1624, 410}}), false};
    // A break exactly where it enters and leaves the city, so the city's pieces are the boulevard's.
    for (std::size_t i = 0; i + 1 < east_west.points.size(); ++i) {
        const Vector3 a = east_west.points[i], b = east_west.points[i + 1];
        if (std::abs(a.z) > .01F || std::abs(b.z) > .01F) continue;
        for (const float x : {city_edge, -city_edge})
            if ((a.x - x) * (b.x - x) < 0 && std::abs(a.x - x) > .5F && std::abs(b.x - x) > .5F) {
                east_west.points.insert(east_west.points.begin() + static_cast<std::ptrdiff_t>(i) + 1, Vector3{x, 0, 0});
                break;
            }
    }
    float along = 0;
    for (std::size_t i = 0; i + 1 < east_west.points.size(); ++i) {
        const Vector3 a = east_west.points[i], b = east_west.points[i + 1];
        const float l = std::hypot(b.x - a.x, b.z - a.z);
        // (The pieces on z = 0 that reach in past x = +-city_edge, clipped to it.)
        if (std::abs(a.z) < .01F && std::abs(b.z) < .01F && l > 0) {
            const float t0 = std::clamp((city_edge - a.x) / (b.x - a.x), 0.0F, 1.0F), t1 = std::clamp((-city_edge - a.x) / (b.x - a.x), 0.0F, 1.0F);
            const float from = along + std::min(t0, t1) * l, to = along + std::max(t0, t1) * l;
            if (to - from > .01F) {
                if (east_west.city_from > east_west.city_to) east_west.city_from = from;
                east_west.city_to = to;
            }
        }
        along += l;
    }
    lines.push_back(std::move(east_west));
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
constexpr float pad_band = 14;      // a roundabout's level pad eases back into the land over this
constexpr char soft = 1, hard = 2;   // holds: a pad's ground (the grade may ease it), or an end or a crossing

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
            p.locked[k] = soft;
        }
        if (p.loop) continue;
        for (const float s : {0.0F, p.total}) {
            // The town's ground where the spoke runs into it: a little way on into its streets (the
            // end itself is often on the edge of the town's levelled pad, still part way up the
            // hill, and holding it there left a step down onto the streets).
            const V2 q = profile_point(p, s), inward = s == 0 ? profile_point(p, 0) - profile_point(p, 8) : profile_point(p, p.total) - profile_point(p, p.total - 8);
            const float span = length(inward);
            const V2 into = span > 1e-3F ? q + inward * (24 / span) : q;
            const float land = natural(into.x, into.z);
            // (This wins over the pad's own hold: on the pad's edge its ground is not yet level.)
            for (std::size_t k = 0; k < p.height.size(); ++k)
                if (distance_along(p, k, s) <= plateau) {p.height[k] = land; p.locked[k] = hard;}
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
                // (Two holds side by side stay as they are, unless both are a pad's: on a pad's edge,
                // where the land is still easing up, the grade wins.)
                const bool pads = p.locked[k] == soft && p.locked[j] == soft;
                const bool fixed_k = p.locked[k] && !pads, fixed_j = p.locked[j] && !pads;
                if (excess <= 1e-4F || (fixed_k && fixed_j)) continue;
                steep = true;
                const float toward = rise > 0 ? 1.0F : -1.0F;   // k up, j down
                if (fixed_k) p.height[j] -= toward * excess;
                else if (fixed_j) p.height[k] += toward * excess;
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
        if (interchanges_open && pa.loop != pb.loop) {
            // A highway under the ring: it stays at its own level, held flat across the interchange's
            // roundabout and a little beyond; the ring climbs to a bridge over it.
            auto& spoke = pa.loop ? pb : pa;
            auto& ring = pa.loop ? pa : pb;
            const float ss = pa.loop ? c.sb : c.sa, rs = pa.loop ? c.sa : c.sb;
            const float level = profile_height(spoke, ss);
            const float flat = roundabout_pad + 30;
            for (std::size_t k = 0; k < spoke.height.size(); ++k)
                if (distance_along(spoke, k, ss) <= flat) {spoke.height[k] = level; spoke.locked[k] = hard;}
            for (std::size_t k = 0; k < ring.height.size(); ++k)
                if (distance_along(ring, k, rs) <= deck_half + step) {ring.height[k] = level + overpass_rise; ring.locked[k] = hard;}
            continue;
        }
        float lo = -1e9F, hi = 1e9F;
        reachable(pa, c.sa, lo, hi);
        reachable(pb, c.sb, lo, hi);
        const float halfway = (profile_height(pa, c.sa) + profile_height(pb, c.sb)) * .5F;
        const float meet = lo <= hi ? std::clamp(halfway, lo, hi) : (lo + hi) * .5F;
        for (const auto& [p, s] : {std::make_pair(&pa, c.sa), std::make_pair(&pb, c.sb)})
            for (std::size_t k = 0; k < p->height.size(); ++k) {
                if (p->locked[k]) continue;
                const float d = distance_along(*p, k, s);
                if (d <= plateau) {p->height[k] = meet; p->locked[k] = hard; continue;}
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

const std::vector<Interchange>& interchanges() {
    static const std::vector<Interchange> all = [] {
        std::vector<Interchange> out;
        if (!interchanges_open) return out;
        const Line* ring = nullptr;
        for (const auto& line : lines()) if (line.loop) ring = &line;
        if (!ring) return out;
        const auto n = ring->points.size();
        for (const auto& spoke : lines()) {
            if (spoke.loop) continue;
            float spoke_s = 0;
            for (std::size_t i = 0; i + 1 < spoke.points.size(); ++i) {
                const V2 a{spoke.points[i].x, spoke.points[i].z}, b{spoke.points[i + 1].x, spoke.points[i + 1].z};
                float ring_s = 0;
                for (std::size_t j = 0; j < n; ++j) {
                    const V2 c{ring->points[j].x, ring->points[j].z}, d{ring->points[(j + 1) % n].x, ring->points[(j + 1) % n].z};
                    const V2 r = b - a, q = d - c, w = c - a;
                    const float den = r.x * q.z - r.z * q.x;
                    if (std::abs(den) > 1e-6F) {
                        const float t = (w.x * q.z - w.z * q.x) / den, u = (w.x * r.z - w.z * r.x) / den;
                        if (t >= 0 && t < 1 && u >= 0 && u < 1) {
                            Interchange x;
                            x.spoke = spoke.name;
                            const V2 at = a + r * t, along = unit(r), round = unit(q);
                            x.at = {at.x, 0, at.z};
                            x.ring_forward = {round.x, 0, round.z};
                            const float away = along.x * at.x + along.z * at.z > 0 ? 1.0F : -1.0F;
                            x.outward = {along.x * away, 0, along.z * away};
                            x.spoke_s = spoke_s + length(r) * t;
                            x.ring_s = ring_s + length(q) * u;
                            x.town = std::abs(at.z) > std::abs(at.x) ? (at.z < 0 ? "Espera" : "Suda Haveno") : (at.x > 0 ? "Porto Luma" : "Roka");
                            // The slip roads, laid out by metres along the ring from the crossing (s) and
                            // out from its centre line (lateral). Forward traffic (the way the ring's points
                            // run) keeps right: inside the ring; the other way, outside it.
                            const float radius = std::hypot(at.x, at.z), bearing = std::atan2(at.z, at.x);
                            const float sense = x.ring_forward.x * -std::sin(bearing) + x.ring_forward.z * std::cos(bearing) > 0 ? 1.0F : -1.0F;
                            const auto point = [&](float s_, float lateral) {
                                const float b = bearing + sense * s_ / radius, r = radius + lateral;
                                return Vector3{r * std::cos(b), 0, r * std::sin(b)};
                            };
                            const float enter = std::sqrt(roundabout_radius * roundabout_radius - slip_offset * slip_offset);
                            const auto lateral = [](float from_ring, float side) {
                                // Off the carriageway's outer lane and out to slip_offset over 60 m.
                                const float t = std::clamp((slip_reach - 2 - from_ring) / 60.0F, 0.0F, 1.0F);
                                return side * (slow_lane + (slip_offset - slow_lane) * t * t * (3 - 2 * t));
                            };
                            for (const bool inside : {true, false})
                                for (const bool off : {true, false}) {
                                    // Inside, forward traffic comes off before the crossing (s < 0) and goes back on
                                    // after it; outside, the other way round.
                                    const float side = inside ? -1.0F : 1.0F, way = inside == off ? -1.0F : 1.0F;
                                    Slip slip;
                                    slip.off = off; slip.inside = inside;
                                    slip.points.push_back(point(way * slip_reach, 0));
                                    for (float d = slip_reach - 2; d > enter + .5F; d -= 10) slip.points.push_back(point(way * d, lateral(d, side)));
                                    slip.points.push_back(point(way * enter, side * slip_offset));
                                    if (!off) std::reverse(slip.points.begin(), slip.points.end());
                                    x.slips.push_back(std::move(slip));
                                }
                            out.push_back(x);
                        }
                    }
                    ring_s += length(q);
                }
                spoke_s += length(b - a);
            }
        }
        // Numbered clockwise round the ring from the north, as the map shows it (north is -z).
        const auto bearing = [](const Interchange& x) { const float b = std::atan2(x.at.x, -x.at.z); return b < 0 ? b + 2 * pi : b; };
        std::sort(out.begin(), out.end(), [&](const Interchange& p, const Interchange& q) { return bearing(p) < bearing(q); });
        for (std::size_t i = 0; i < out.size(); ++i) out[i].number = static_cast<int>(i) + 1;
        return out;
    }();
    return all;
}

namespace {
const Index* built = nullptr;   // the profiles, once graded() has laid them
// The ring at a point near an interchange: its profile, and the distance along it.
const Profile* ring_profile() {
    if (!built) return nullptr;
    for (const auto& p : built->profiles) if (p.loop) return &p;
    return nullptr;
}
}

float graded(float x, float z, float height, HeightFn natural, HeightFn pad) {
    static const Index index = build(natural, pad);
    built = &index;
    if (std::abs(x) >= Index::extent || std::abs(z) >= Index::extent) return height;
    const auto& cell = index.grid[static_cast<std::size_t>(Index::coordinate(x) * Index::cells + Index::coordinate(z))];
    if (cell.empty()) return height;
    float nearest = grade_flat + grade_band, road = height, lowest = 1e9F;
    // Each line's nearest point, then the nearest of them.
    std::array<float, 8> line_distance, line_height;
    line_distance.fill(1e9F);
    for (const auto& segment : cell) {
        const auto& p = index.profiles[static_cast<std::size_t>(segment.line)];
        const V2 a = p.points[segment.index], b = p.points[segment.index + 1], d = b - a;
        const float l2 = d.x * d.x + d.z * d.z;
        const float t = l2 < 1e-6F ? 0 : std::clamp(((x - a.x) * d.x + (z - a.z) * d.z) / l2, 0.0F, 1.0F);
        const float distance = std::hypot(x - (a.x + d.x * t), z - (a.z + d.z * t));
        const auto l = static_cast<std::size_t>(segment.line) % line_distance.size();
        if (distance < line_distance[l]) {line_distance[l] = distance; line_height[l] = profile_height(p, p.at[segment.index] + t * std::sqrt(l2));}
    }
    for (std::size_t l = 0; l < line_distance.size(); ++l) {
        // On two roadbeds at once (a crossing, or under a bridge) the lower road has the ground:
        // the higher is on its deck.
        if (line_distance[l] <= grade_flat) lowest = std::min(lowest, line_height[l]);
        if (line_distance[l] < nearest) {nearest = line_distance[l]; road = line_height[l];}
    }
    // An interchange's roundabout sits on a level pad at the crossing highway's height, and its slip
    // roads on roadbeds of their own, from the ring's height where they leave it to the pad's.
    struct Slipway { std::vector<V2> points; std::vector<float> at; float from, to; };
    static const auto levels = [&] {
        std::vector<std::pair<float, std::vector<Slipway>>> out;
        const Profile* ring = nullptr;
        for (const auto& p : index.profiles) if (p.loop) ring = &p;
        for (const auto& i : interchanges()) {
            float level = 0;
            for (std::size_t l = 0; l < lines().size(); ++l)
                if (lines()[l].name == i.spoke) level = profile_height(index.profiles[l], i.spoke_s);
            std::vector<Slipway> ways;
            for (const auto& slip : i.slips) {
                Slipway w;
                for (const auto& q : slip.points) w.points.push_back({q.x, q.z});
                w.at.push_back(0);
                for (std::size_t k = 1; k < w.points.size(); ++k) w.at.push_back(w.at.back() + length(w.points[k] - w.points[k - 1]));
                // (Its end on the ring: at the ring's height there.)
                const V2 end = slip.off ? w.points.front() : w.points.back();
                const float along = (end.x - i.at.x) * i.ring_forward.x + (end.z - i.at.z) * i.ring_forward.z;
                const float ring_height = ring ? profile_height(*ring, i.ring_s + along) : level;
                w.from = slip.off ? ring_height : level;
                w.to = slip.off ? level : ring_height;
                ways.push_back(std::move(w));
            }
            out.emplace_back(level, std::move(ways));
        }
        return out;
    }();
    float pad_level = 0, pad_weight = 0;
    for (std::size_t k = 0; k < interchanges().size() && k < levels.size(); ++k) {
        const Vector3 c = interchanges()[k].at;
        const float d = std::hypot(x - c.x, z - c.z);
        if (d > roundabout_pad + slip_reach + grade_band) continue;
        if (d <= roundabout_pad) lowest = std::min(lowest, levels[k].first);   // (the pad is a roadbed: under the bridge too)
        if (d < roundabout_pad + pad_band) {
            const float t = std::clamp((d - roundabout_pad) / pad_band, 0.0F, 1.0F), w = 1 - t * t * (3 - 2 * t);
            if (w > pad_weight) {pad_weight = w; pad_level = levels[k].first;}
        }
        for (const auto& w : levels[k].second)
            for (std::size_t j = 0; j + 1 < w.points.size(); ++j) {
                const V2 a = w.points[j], b = w.points[j + 1], dd = b - a;
                const float l2 = dd.x * dd.x + dd.z * dd.z;
                const float t = l2 < 1e-6F ? 0 : std::clamp(((x - a.x) * dd.x + (z - a.z) * dd.z) / l2, 0.0F, 1.0F);
                const float distance = std::hypot(x - (a.x + dd.x * t), z - (a.z + dd.z * t));
                if (distance > slip_width * .5F + 2) continue;
                const float f = (w.at[j] + t * std::sqrt(l2)) / std::max(1.0F, w.at.back());
                lowest = std::min(lowest, w.from + (w.to - w.from) * f);
                if (distance < nearest) {nearest = distance; road = w.from + (w.to - w.from) * f;}
            }
    }
    const auto with_pads = [&](float h) { return h * (1 - pad_weight) + pad_level * pad_weight; };
    // The roadbed itself is always level across (another road's roadbed - the ring's, climbing to its
    // bridge - is not the pad's).
    if (lowest < 1e8F) return std::abs(lowest - pad_level) < .5F ? with_pads(lowest) : lowest;
    if (nearest >= grade_flat + grade_band) return with_pads(height);
    const float t = (nearest - grade_flat) / grade_band;
    float blend = t * t * (3 - 2 * t);
    // Inside a building pad, the pad's level ground stands beyond a bank by the shoulder: three
    // times as wide as the road is above or below the pad (at most pad_bank), so the pad stays
    // level beyond it and there is never a step down at the roadbed's edge.
    const float width = std::clamp(std::abs(road - height) * 3, .5F, pad_bank);
    const float bank = std::clamp((nearest - grade_flat) / width, 0.0F, 1.0F);
    const float levelled = std::clamp((pad(x, z) - .9F) / .1F, 0.0F, 1.0F) * bank * bank * (3 - 2 * bank);
    blend = std::max(blend, levelled * levelled * (3 - 2 * levelled));
    return with_pads(road * (1 - blend) + height * blend);
}

bool deck(float x, float z, float& y, const Vector3* heading) {
    if (!built) terrain::TerrainHeight::sample(0, 0);   // (lays the profiles)
    const Profile* ring = ring_profile();
    if (!ring) return false;
    for (const auto& i : interchanges()) {
        const float dx = x - i.at.x, dz = z - i.at.z;
        const float along = dx * i.ring_forward.x + dz * i.ring_forward.z;
        if (std::abs(along) > deck_half + 1) continue;
        // (The ring curves gently: its centre line is a 1,500 m circle.)
        const float across = std::hypot(x, z) - std::hypot(i.at.x, i.at.z);
        if (std::abs(across) > paved_half + verge) continue;
        if (heading && std::abs(heading->x * i.ring_forward.x + heading->z * i.ring_forward.z) < .6F) continue;
        y = profile_height(*ring, i.ring_s + along);
        return true;
    }
    return false;
}

}
