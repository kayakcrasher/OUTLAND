#include "outland/world/roads/HighwayNetwork.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace outland::world::highways {
namespace {
constexpr float join = .75F;         // ends this close are the same point
constexpr float side_join = paved_half + 3;

struct Piece {
    std::size_t settlement, road;
    Vector2 a, b, d, right;
    float length;
};
Vector2 sub(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
float dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
float len(Vector2 a) { return std::hypot(a.x, a.y); }
Vector2 unit(Vector2 a) { const float l = len(a); return l < 1e-6F ? Vector2{0, 0} : Vector2{a.x / l, a.y / l}; }
Vector2 right_of(Vector2 d) { return {-d.y, d.x}; }

// Union of spans, then [0, length] minus it, in pieces at least `shortest` long.
std::vector<Span> complement(std::vector<Span> taken, float length, float shortest) {
    std::sort(taken.begin(), taken.end(), [](const Span& x, const Span& y) { return x.from < y.from; });
    std::vector<Span> free;
    float at = 0;
    for (const auto& s : taken) {
        if (s.from > at && s.from - at >= shortest) free.push_back({at, s.from});
        at = std::max(at, s.to);
    }
    if (length - at >= shortest) free.push_back({at, length});
    return free;
}
std::vector<Span> merged(std::vector<Span> spans) {
    std::sort(spans.begin(), spans.end(), [](const Span& x, const Span& y) { return x.from < y.from; });
    std::vector<Span> out;
    for (const auto& s : spans) {
        if (!out.empty() && s.from <= out.back().to) out.back().to = std::max(out.back().to, s.to);
        else out.push_back(s);
    }
    return out;
}
long long cell_key(int x, int z) { return (static_cast<long long>(x) << 32) ^ static_cast<std::uint32_t>(z); }
constexpr float cell = 32;
}

Vector3 cross_at(const RoadInfo& info, Vector3 direction, float distance, float length) {
    const float t = length > 1e-4F ? std::clamp(distance / length, 0.0F, 1.0F) : 0;
    Vector3 c{info.start_cross.x * (1 - t) + info.end_cross.x * t, 0, info.start_cross.z * (1 - t) + info.end_cross.z * t};
    const float l = std::hypot(c.x, c.z);
    const Vector3 right{-direction.z, 0, direction.x};
    if (l < 1e-4F) return right;
    c = {c.x / l, 0, c.z / l};
    const float square = c.x * right.x + c.z * right.z;
    const float scale = 1 / std::max(.5F, square);
    return {c.x * scale, 0, c.z * scale};
}

void Network::build(const VerdaRegion& region) {
    roads_.clear(); runs_.clear(); grid_.clear(); junctions_ = 0;
    const auto& towns = region.settlements();
    roads_.resize(towns.size());
    std::vector<Piece> pieces;
    struct Other { Vector2 a, b; float width; };
    std::vector<Other> others;
    for (std::size_t s = 0; s < towns.size(); ++s) {
        roads_[s].resize(towns[s].roads.size());
        for (std::size_t r = 0; r < towns[s].roads.size(); ++r) {
            const auto& road = towns[s].roads[r];
            const Vector2 a{road.start.x, road.start.z}, b{road.end.x, road.end.z};
            const float l = len(sub(b, a));
            if (!(l > .5F) || !std::isfinite(l)) continue;
            if (road.type != RoadType::Highway) {others.push_back({a, b, road.width}); continue;}
            const Vector2 d = unit(sub(b, a));
            pieces.push_back({s, r, a, b, d, right_of(d), l});
        }
    }
    // Joins: a piece that runs on into exactly one other at an end is mitred there.
    struct Junction { Vector2 p; float plain, gap; };
    std::vector<Junction> junctions;
    for (const auto& piece : pieces) {
        auto& info = roads_[piece.settlement][piece.road];
        info.highway = true;
        for (int end = 0; end < 2; ++end) {
            const Vector2 at = end == 0 ? piece.a : piece.b;
            // The neighbours' directions of travel through `at`, the piece's way (leaving from its
            // end, arriving at its start).
            std::vector<Vector2> onward;
            for (const auto& other : pieces) {
                if (&other == &piece) continue;
                if (len(sub(other.a, at)) < join) onward.push_back(end == 1 ? other.d : Vector2{-other.d.x, -other.d.y});
                else if (len(sub(other.b, at)) < join) onward.push_back(end == 1 ? Vector2{-other.d.x, -other.d.y} : other.d);
            }
            Vector2 cross = piece.right;
            if (onward.size() == 1) {
                const Vector2 next = onward[0];
                if (dot(next, piece.d) > std::cos(50 * DEG2RAD)) cross = unit({piece.right.x + right_of(next).x, piece.right.y + right_of(next).y});
            } else if (onward.empty()) {
                junctions.push_back({at, 24, 24});       // the highway ends: an apron onto the town's street
            } else {
                junctions.push_back({at, 26, 26});       // highways branch
            }
            (end == 0 ? info.start_cross : info.end_cross) = {cross.x, 0, cross.y};
        }
    }
    // Highways crossing each other (not at shared ends).
    for (std::size_t i = 0; i < pieces.size(); ++i)
        for (std::size_t j = i + 1; j < pieces.size(); ++j) {
            const auto& p = pieces[i];
            const auto& q = pieces[j];
            const Vector2 r = sub(p.b, p.a), s = sub(q.b, q.a), w = sub(q.a, p.a);
            const float den = r.x * s.y - r.y * s.x;
            if (std::abs(den) < 1e-6F) continue;
            const float t = (w.x * s.y - w.y * s.x) / den, u = (w.x * r.y - w.y * r.x) / den;
            if (t <= 0 || t >= 1 || u <= 0 || u >= 1) continue;
            const float sine = std::max(std::sin(25 * DEG2RAD), std::abs(p.d.x * q.d.y - p.d.y * q.d.x));
            const float reach = paved_half / sine + 8;
            junctions.push_back({{p.a.x + r.x * t, p.a.y + r.y * t}, reach, reach});
        }
    // Other roads that join or cross a highway: a median opening (and, where a street crosses, no
    // lines across its way).
    for (const auto& o : others)
        for (const auto& p : pieces) {
            for (const Vector2 end : {o.a, o.b}) {
                const float along = dot(sub(end, p.a), p.d), aside = std::abs(dot(sub(end, p.a), p.right));
                if (along > -2 && along < p.length + 2 && aside < side_join) junctions.push_back({end, 0, o.width * .5F + 7});
            }
            const Vector2 r = sub(p.b, p.a), s = sub(o.b, o.a), w = sub(o.a, p.a);
            const float den = r.x * s.y - r.y * s.x;
            if (std::abs(den) < 1e-6F) continue;
            const float t = (w.x * s.y - w.y * s.x) / den, u = (w.x * r.y - w.y * r.x) / den;
            if (t <= 0 || t >= 1 || u <= .02F || u >= .98F) continue;
            junctions.push_back({{p.a.x + r.x * t, p.a.y + r.y * t}, o.width * .5F + 2, o.width * .5F + 7});
        }
    junctions_ = junctions.size();
    // Each junction marks every highway piece that passes through it.
    for (const auto& piece : pieces) {
        auto& info = roads_[piece.settlement][piece.road];
        std::vector<Span> gaps;
        for (const auto& j : junctions) {
            const float along = dot(sub(j.p, piece.a), piece.d), aside = std::abs(dot(sub(j.p, piece.a), piece.right));
            if (aside > side_join) continue;
            const float reach = std::max(j.plain, j.gap);
            if (along < -reach || along > piece.length + reach) continue;
            if (j.plain > 0) info.plain.push_back({std::max(0.0F, along - j.plain), std::min(piece.length, along + j.plain)});
            gaps.push_back({std::max(0.0F, along - j.gap), std::min(piece.length, along + j.gap)});
        }
        info.plain = merged(info.plain);
        info.barrier = complement(gaps, piece.length, barrier_length);
        for (const auto& span : info.barrier) {
            const Vector2 a{piece.a.x + piece.d.x * span.from, piece.a.y + piece.d.y * span.from};
            const Vector2 b{piece.a.x + piece.d.x * span.to, piece.a.y + piece.d.y * span.to};
            const int index = static_cast<int>(runs_.size());
            runs_.push_back({a, b});
            for (int x = static_cast<int>(std::floor((std::min(a.x, b.x) - 4) / cell)); x <= static_cast<int>(std::floor((std::max(a.x, b.x) + 4) / cell)); ++x)
                for (int z = static_cast<int>(std::floor((std::min(a.y, b.y) - 4) / cell)); z <= static_cast<int>(std::floor((std::max(a.y, b.y) + 4) / cell)); ++z)
                    grid_[cell_key(x, z)].push_back(index);
        }
    }
}

const RoadInfo* Network::info(std::size_t settlement, std::size_t road) const {
    if (settlement >= roads_.size() || road >= roads_[settlement].size()) return nullptr;
    const auto& info = roads_[settlement][road];
    return info.highway ? &info : nullptr;
}

bool Network::barrier_hit(Vector3 p, float radius) const {
    if (runs_.empty()) return false;
    const auto found = grid_.find(cell_key(static_cast<int>(std::floor(p.x / cell)), static_cast<int>(std::floor(p.z / cell))));
    if (found == grid_.end()) return false;
    for (const int i : found->second) {
        const auto& run = runs_[static_cast<std::size_t>(i)];
        const Vector2 d = sub(run.b, run.a), q{p.x - run.a.x, p.z - run.a.y};
        const float l2 = dot(d, d);
        const float t = l2 < 1e-6F ? 0 : std::clamp(dot(q, d) / l2, 0.0F, 1.0F);
        if (std::hypot(q.x - d.x * t, q.y - d.y * t) < radius + barrier_half) return true;
    }
    return false;
}

const Network& network(const VerdaRegion& region) {
    static Network cached;
    static std::size_t signature = 0;
    static const VerdaRegion* owner = nullptr;
    std::size_t now = region.settlements().size();
    for (const auto& town : region.settlements())
        now = now * 1000003 + town.roads.size() * 31 + reinterpret_cast<std::uintptr_t>(town.roads.data());
    if (owner != &region || signature != now) {
        cached.build(region);
        owner = &region;
        signature = now;
    }
    return cached;
}

}
