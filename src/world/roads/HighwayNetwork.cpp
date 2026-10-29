#include "outland/world/roads/HighwayNetwork.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace outland::world::highways {
namespace {
constexpr float join = .75F;         // ends this close are the same point
constexpr float side_join = paved_half + 3;
constexpr float taper = 150;         // the last stretch into a town narrows over this far (an easy blend)
constexpr float town_half = 6.6F;    // to just wider than the town's 12 m street
constexpr float town_apron = 45;     // plain asphalt (no lane lines, no barrier) over the end of it
constexpr float crossover_every = 420, crossover_half = 11;   // median openings along the open road
constexpr float boulevard_ease = 60;  // from a highway's full width to a city boulevard's, over this far

struct Piece {
    std::size_t settlement, road;
    Vector2 a, b, d, right;
    float length;
    bool boulevard;    // through a city: narrower (highways::boulevard_width)
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
            pieces.push_back({s, r, a, b, d, right_of(d), l, road.width < min_width});
        }
    }
    // Joins: a piece that runs on into exactly one other at an end is mitred there.
    struct Junction { Vector2 p; float plain, gap; };
    std::vector<Junction> junctions;
    constexpr int none = -1;
    std::vector<std::array<int, 2>> next(pieces.size(), {none, none});   // the piece running on at each end
    std::vector<std::array<bool, 2>> ends(pieces.size(), {false, false}); // the highway ends there (a town)
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        const auto& piece = pieces[i];
        auto& info = roads_[piece.settlement][piece.road];
        info.highway = true;
        info.start_half = info.end_half = paved_half;
        for (int end = 0; end < 2; ++end) {
            const Vector2 at = end == 0 ? piece.a : piece.b;
            // The neighbours' directions of travel through `at`, the piece's way (leaving from its
            // end, arriving at its start).
            std::vector<Vector2> onward;
            int neighbour = none;
            for (std::size_t j = 0; j < pieces.size(); ++j) {
                const auto& other = pieces[j];
                if (j == i) continue;
                if (len(sub(other.a, at)) < join) {onward.push_back(end == 1 ? other.d : Vector2{-other.d.x, -other.d.y}); neighbour = static_cast<int>(j);}
                else if (len(sub(other.b, at)) < join) {onward.push_back(end == 1 ? Vector2{-other.d.x, -other.d.y} : other.d); neighbour = static_cast<int>(j);}
            }
            Vector2 cross = piece.right;
            if (onward.size() == 1) {
                const Vector2 ahead = onward[0];
                if (dot(ahead, piece.d) > std::cos(50 * DEG2RAD)) cross = unit({piece.right.x + right_of(ahead).x, piece.right.y + right_of(ahead).y});
                next[i][static_cast<std::size_t>(end)] = neighbour;
            } else if (onward.empty()) {
                ends[i][static_cast<std::size_t>(end)] = true;   // the highway ends: it tapers onto the town's street
            } else {
                junctions.push_back({at, 26, 26});       // highways branch
            }
            (end == 0 ? info.start_cross : info.end_cross) = {cross.x, 0, cross.y};
        }
    }
    // Chains: the pieces of each highway in order, with each piece's distance along it, for the
    // tapers into towns and the crossovers.
    std::vector<float> along(pieces.size(), 0);
    std::vector<bool> forward(pieces.size(), true), seen(pieces.size(), false);
    struct Chain { std::vector<std::size_t> pieces; float length{0}; bool open_start{false}, open_end{false}; };
    std::vector<Chain> chains;
    const auto walk = [&](std::size_t first, bool first_forward) {
        Chain chain;
        std::size_t at = first;
        bool fwd = first_forward;
        float s = 0;
        while (!seen[at]) {
            seen[at] = true;
            along[at] = s; forward[at] = fwd;
            chain.pieces.push_back(at);
            s += pieces[at].length;
            const int leaving_end = fwd ? 1 : 0;
            const int n = next[at][static_cast<std::size_t>(leaving_end)];
            if (n == none) {chain.open_end = ends[at][static_cast<std::size_t>(leaving_end)]; break;}
            const Vector2 exit = fwd ? pieces[at].b : pieces[at].a;
            fwd = len(sub(pieces[static_cast<std::size_t>(n)].a, exit)) < join;
            at = static_cast<std::size_t>(n);
        }
        chain.length = s;
        chains.push_back(std::move(chain));
    };
    for (std::size_t i = 0; i < pieces.size(); ++i) {     // spokes, from a town end
        if (seen[i]) continue;
        if (ends[i][0] && next[i][1] != none) {walk(i, true); chains.back().open_start = true;}
        else if (ends[i][1] && next[i][0] != none) {walk(i, false); chains.back().open_start = true;}
    }
    for (std::size_t i = 0; i < pieces.size(); ++i) if (!seen[i]) walk(i, true);   // the ring (and loners)
    std::vector<std::vector<Span>> extra_gaps(pieces.size()), extra_plain(pieces.size());
    for (const auto& chain : chains) {
        // Where the chain runs through a city as a boulevard (its distances along the chain).
        std::vector<Span> city;
        for (const auto i : chain.pieces)
            if (pieces[i].boulevard) city.push_back({along[i], along[i] + pieces[i].length});
        city = merged(city);
        const auto half_at = [&](float s) {
            float from_end = 1e9F;
            if (chain.open_start) from_end = std::min(from_end, s);
            if (chain.open_end) from_end = std::min(from_end, chain.length - s);
            float half = paved_half;
            if (from_end < taper) {
                const float t = std::clamp(from_end / taper, 0.0F, 1.0F), eased = t * t * (3 - 2 * t);
                half = town_half + (paved_half - town_half) * eased;
            }
            // Easing down to a boulevard's width as it comes into a city.
            float to_city = 1e9F;
            for (const auto& c : city) to_city = std::min(to_city, s < c.from ? c.from - s : s > c.to ? s - c.to : 0.0F);
            if (to_city < boulevard_ease) {
                const float t = std::clamp(to_city / boulevard_ease, 0.0F, 1.0F), eased = t * t * (3 - 2 * t);
                half = std::min(half, boulevard_width * .5F + (paved_half - boulevard_width * .5F) * eased);
            }
            return half;
        };
        for (const auto i : chain.pieces) {
            auto& info = roads_[pieces[i].settlement][pieces[i].road];
            const float s0 = along[i], s1 = along[i] + pieces[i].length;
            // Piece-local distances: chain distance s is local s - s0 going forward, s1 - s going back.
            const auto local = [&](float s) { return forward[i] ? s - s0 : s1 - s; };
            const float h0 = half_at(s0), h1 = half_at(s1);
            info.start_half = forward[i] ? h0 : h1;
            info.end_half = forward[i] ? h1 : h0;
            const auto span = [&](float a, float b) {
                const float la = local(a), lb = local(b);
                return Span{std::clamp(std::min(la, lb), 0.0F, pieces[i].length), std::clamp(std::max(la, lb), 0.0F, pieces[i].length)};
            };
            if (chain.open_start && s0 < taper) {extra_gaps[i].push_back(span(0, taper)); extra_plain[i].push_back(span(0, town_apron));}
            if (chain.open_end && s1 > chain.length - taper) {extra_gaps[i].push_back(span(chain.length - taper, chain.length)); extra_plain[i].push_back(span(chain.length - town_apron, chain.length));}
            // Crossovers along the open road.
            for (float c = crossover_every * .5F; c < chain.length; c += crossover_every)
                if (c + crossover_half > s0 && c - crossover_half < s1) extra_gaps[i].push_back(span(c - crossover_half, c + crossover_half));
            std::erase_if(extra_plain[i], [](const Span& x) { return x.to - x.from < .01F; });
            std::erase_if(extra_gaps[i], [](const Span& x) { return x.to - x.from < .01F; });
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
            // (Not where the ring bridges a spoke: neither stops there.)
            const Vector2 at{p.a.x + r.x * t, p.a.y + r.y * t};
            if (std::any_of(interchanges().begin(), interchanges().end(), [&](const Interchange& x) { return std::hypot(x.at.x - at.x, x.at.z - at.y) < 30; })) continue;
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
            if (t < 0 || t > 1 || u <= .02F || u >= .98F) continue;   // (at a piece's end too)
            junctions.push_back({{p.a.x + r.x * t, p.a.y + r.y * t}, o.width * .5F + 2, o.width * .5F + 7});
        }
    junctions_ = junctions.size();
    // Each junction marks every highway piece that passes through it.
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        const auto& piece = pieces[i];
        auto& info = roads_[piece.settlement][piece.road];
        std::vector<Span> gaps = extra_gaps[i];
        info.plain = extra_plain[i];
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
            // Near an interchange, in short runs that each know their level.
            const bool near = std::any_of(interchanges().begin(), interchanges().end(), [&](const Interchange& x) {
                return std::hypot(x.at.x - (a.x + b.x) * .5F, x.at.z - (a.y + b.y) * .5F) < span.to - span.from + 60; });
            if (!near) {add_run({a, b}); continue;}
            const int count = std::max(1, static_cast<int>(std::ceil((span.to - span.from) / 4)));
            for (int k = 0; k < count; ++k) {
                const Vector2 from{a.x + (b.x - a.x) * k / count, a.y + (b.y - a.y) * k / count};
                const Vector2 to{a.x + (b.x - a.x) * (k + 1) / count, a.y + (b.y - a.y) * (k + 1) / count};
                const float mx = (from.x + to.x) * .5F, mz = (from.y + to.y) * .5F;
                const Vector3 heading{piece.d.x, 0, piece.d.y};
                float y = 0;
                if (deck(mx, mz, y, &heading)) add_run({from, to, y, true});                   // on the bridge
                else if (deck(mx, mz, y)) add_run({from, to, terrain::TerrainHeight::sample(mx, mz), true});   // under it
                else add_run({from, to});
            }
        }
    }
    // The bridges' parapets: a wall along each edge of the deck.
    for (const auto& x : interchanges())
        for (const float side : {-1.0F, 1.0F})
            for (int k = 0; k < 8; ++k) {
                const auto point = [&](int i) {
                    const float along = -deck_half + 2 * deck_half * static_cast<float>(i) / 8;
                    const float r = std::hypot(x.at.x, x.at.z) + side * (paved_half + .45F);
                    const float bearing = std::atan2(x.at.z, x.at.x) + along / std::hypot(x.at.x, x.at.z) * (x.ring_forward.x * -x.at.z + x.ring_forward.z * x.at.x > 0 ? 1.0F : -1.0F);
                    return Vector2{r * std::cos(bearing), r * std::sin(bearing)};
                };
                const Vector2 a = point(k), b = point(k + 1);
                float y = 0;
                const Vector3 heading = x.ring_forward;
                const float mx = (a.x + b.x) * .5F, mz = (a.y + b.y) * .5F;
                if (deck(mx - side * 1.0F * mx / std::hypot(mx, mz), mz - side * 1.0F * mz / std::hypot(mx, mz), y, &heading)) add_run({a, b, y, true});
            }
}

void Network::add_run(Run run) {
    const int index = static_cast<int>(runs_.size());
    runs_.push_back(run);
    const Vector2 a = run.a, b = run.b;
    for (int x = static_cast<int>(std::floor((std::min(a.x, b.x) - 4) / cell)); x <= static_cast<int>(std::floor((std::max(a.x, b.x) + 4) / cell)); ++x)
        for (int z = static_cast<int>(std::floor((std::min(a.y, b.y) - 4) / cell)); z <= static_cast<int>(std::floor((std::max(a.y, b.y) + 4) / cell)); ++z)
            grid_[cell_key(x, z)].push_back(index);
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
        if (run.level && std::abs(p.y - run.y) > 3) continue;   // above or below this one
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
