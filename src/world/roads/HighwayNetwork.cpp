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
constexpr float roundabout_taper = 50, roundabout_half = 7.5F, roundabout_apron = 12;   // into an interchange's roundabout
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
bool inside_any(const std::vector<Span>& spans, float s) {
    return std::any_of(spans.begin(), spans.end(), [&](const Span& span) { return s >= span.from && s <= span.to; });
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
    roads_.clear(); runs_.clear(); grid_.clear(); aprons_.clear(); junctions_ = 0; rail_count_ = 0;
    const auto& towns = region.settlements();
    roads_.resize(towns.size());
    std::vector<Piece> pieces;
    struct Other { Vector2 a, b; float width; RoadType type; };
    std::vector<Other> others;
    for (std::size_t s = 0; s < towns.size(); ++s) {
        roads_[s].resize(towns[s].roads.size());
        for (std::size_t r = 0; r < towns[s].roads.size(); ++r) {
            const auto& road = towns[s].roads[r];
            const Vector2 a{road.start.x, road.start.z}, b{road.end.x, road.end.z};
            const float l = len(sub(b, a));
            if (!(l > .5F) || !std::isfinite(l)) continue;
            if (road.type != RoadType::Highway) {others.push_back({a, b, road.width, road.type}); continue;}
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
    // A highway's end at an interchange's roundabout (not a town's street): a short taper to the
    // roundabout's entry and exit lanes.
    const auto at_roundabout = [&](Vector2 p) {
        return std::any_of(interchanges().begin(), interchanges().end(), [&](const Interchange& x) {
            return std::hypot(p.x - x.at.x, p.y - x.at.z) < roundabout_radius + 2; });
    };
    for (const auto& chain : chains) {
        const auto first = chain.pieces.front(), last = chain.pieces.back();
        const bool round_start = chain.open_start && at_roundabout(forward[first] ? pieces[first].a : pieces[first].b);
        const bool round_end = chain.open_end && at_roundabout(forward[last] ? pieces[last].b : pieces[last].a);
        const float taper_start = round_start ? roundabout_taper : taper, taper_end = round_end ? roundabout_taper : taper;
        const float apron_start = round_start ? roundabout_apron : town_apron, apron_end = round_end ? roundabout_apron : town_apron;
        // Where the chain runs through a city as a boulevard (its distances along the chain).
        std::vector<Span> city;
        for (const auto i : chain.pieces)
            if (pieces[i].boulevard) city.push_back({along[i], along[i] + pieces[i].length});
        city = merged(city);
        const auto half_at = [&](float s) {
            float half = paved_half;
            const auto narrow = [&](float from_end, float over, float to) {
                if (from_end >= over) return;
                const float t = std::clamp(from_end / over, 0.0F, 1.0F), eased = t * t * (3 - 2 * t);
                half = std::min(half, to + (paved_half - to) * eased);
            };
            if (chain.open_start) narrow(s, taper_start, round_start ? roundabout_half : town_half);
            if (chain.open_end) narrow(chain.length - s, taper_end, round_end ? roundabout_half : town_half);
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
            if (chain.open_start && s0 < taper_start) {extra_gaps[i].push_back(span(0, taper_start)); extra_plain[i].push_back(span(0, apron_start));}
            if (chain.open_end && s1 > chain.length - taper_end) {extra_gaps[i].push_back(span(chain.length - taper_end, chain.length)); extra_plain[i].push_back(span(chain.length - apron_end, chain.length));}
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
    for (const auto& o : others) {
        if (o.type == RoadType::Ramp) continue;   // slip roads join the carriageway, not across the median
        for (const auto& p : pieces) {
            // (Under the ring's bridge over a roundabout, nothing meets the ring.)
            const bool over = [&] {
                for (const auto& x : interchanges())
                    if (std::hypot((p.a.x + p.b.x) * .5F - x.at.x, (p.a.y + p.b.y) * .5F - x.at.z) < deck_half + p.length &&
                        std::abs(p.d.x * x.ring_forward.x + p.d.y * x.ring_forward.z) > .8F && o.type == RoadType::Roundabout) return true;
                return false;
            }();
            if (over) continue;
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
                    const float r = std::hypot(x.at.x, x.at.z) + side * parapet;
                    const float bearing = std::atan2(x.at.z, x.at.x) + along / std::hypot(x.at.x, x.at.z) * (x.ring_forward.x * -x.at.z + x.ring_forward.z * x.at.x > 0 ? 1.0F : -1.0F);
                    return Vector2{r * std::cos(bearing), r * std::sin(bearing)};
                };
                const Vector2 a = point(k), b = point(k + 1);
                float y = 0;
                const Vector3 heading = x.ring_forward;
                const float mx = (a.x + b.x) * .5F, mz = (a.y + b.y) * .5F;
                if (deck(mx - side * 1.0F * mx / std::hypot(mx, mz), mz - side * 1.0F * mz / std::hypot(mx, mz), y, &heading)) add_run({a, b, y, true});
            }
    islands();
    guardrails(region);
    drive_ins(region);
}

// Each drive-in's apron: its lot's front (local -z: it faces the road) out to the nearest road's
// edge straight ahead (a highway's to its gravel verge), at most 16 m.
void Network::drive_ins(const VerdaRegion& region) {
    struct Kind { const char* model; float half_x, front, back; };
    static constexpr Kind kinds[]{
        {"civic/gas_station.glb", 12, 11, 11},       // (the forecourt is the model's front half)
        {"civic/mechanic_shop.glb", 9, 6.2F, 6},
        {"civic/car_lot.glb", 18, 13.6F, 13},
    };
    const auto& towns = region.settlements();
    for (const auto& town : towns)
        for (const auto& asset : town.assets) {
            const Kind* kind = nullptr;
            for (const auto& k : kinds) if (asset.model_path.size() >= std::char_traits<char>::length(k.model) && asset.model_path.ends_with(k.model)) kind = &k;
            if (!kind) continue;
            const float a = asset.rotation_y * DEG2RAD;
            const Vector2 ahead{-std::sin(a), -std::cos(a)}, across{std::cos(a), -std::sin(a)};   // local -z, local +x
            const Vector2 front{asset.position.x + ahead.x * kind->front, asset.position.z + ahead.y * kind->front};
            // The road straight ahead: the nearest whose edge the ray from the front reaches.
            float best = 16;
            for (const auto& t : towns)
                for (const auto& road : t.roads) {
                    const Vector2 p{road.start.x, road.start.z}, q{road.end.x, road.end.z}, d = sub(q, p);
                    const float l = len(d);
                    if (l < 1) continue;
                    const Vector2 u{d.x / l, d.y / l};
                    // Where the ray (front + ahead * s) crosses the road's centre line.
                    const float den = ahead.x * u.y - ahead.y * u.x;
                    if (std::abs(den) < .2F) continue;   // (running alongside, not across the way ahead)
                    const Vector2 w = sub(p, front);
                    const float s = (w.x * u.y - w.y * u.x) / den, along = (w.x * ahead.y - w.y * ahead.x) / den;
                    if (along < -2 || along > l + 2) continue;
                    const float half = road.type == RoadType::Highway ? road.width * .5F + verge : road.width * .5F;
                    const float edge = s - half / std::abs(den);   // (slantwise: the edge comes sooner)
                    if (edge > -1 && edge < best) best = std::max(edge, 0.0F);
                }
            if (best >= 16 || best < .3F) continue;
            const float reach = best + .6F;   // (onto the road's edge)
            const auto at = [&](float side, float out) {
                return Vector3{front.x + across.x * side + ahead.x * out, 0, front.y + across.y * side + ahead.y * out};
            };
            aprons_.push_back({at(-kind->half_x, 0), at(kind->half_x, 0), at(-kind->half_x, reach), at(kind->half_x, reach), asset.position.y, kind->front + kind->back});
        }
}

bool Network::on_apron(float x, float z) const {
    for (const auto& a : aprons_) {
        // (A parallelogram: from the lot's front edge to the road's.)
        const Vector2 o{a.lot_left.x, a.lot_left.z}, e1 = sub({a.lot_right.x, a.lot_right.z}, o), e2 = sub({a.road_left.x, a.road_left.z}, o);
        const Vector2 q = sub({x, z}, o);
        const float u = dot(q, e1) / dot(e1, e1), v = dot(q, e2) / dot(e2, e2);
        if (u >= -.02F && u <= 1.02F && v >= -.05F && v <= 1.05F) return true;
    }
    return false;
}

// Guardrails: along a highway or a slip road wherever the land falls away beside its roadbed (the
// embankments up to the ring's bridges, a slip road built up over a dip), steel on posts out on the
// verge. Never on a bridge (it has parapets), across a junction, where another road joins or runs
// alongside (a slip road's taper off the ring), round a roundabout, or through a town.
void Network::guardrails(const VerdaRegion& region) {
    struct Other { std::size_t s, r; Vector2 a, b; float half; };
    std::vector<Other> others;
    const auto& towns = region.settlements();
    for (std::size_t s = 0; s < towns.size(); ++s)
        for (std::size_t r = 0; r < towns[s].roads.size(); ++r) {
            const auto& road = towns[s].roads[r];
            others.push_back({s, r, {road.start.x, road.start.z}, {road.end.x, road.end.z}, road.width * .5F});
        }
    constexpr float step = 2, drop = 1.4F, shortest = 8, bridge_gap = 6;
    for (std::size_t s = 0; s < towns.size(); ++s)
        for (std::size_t r = 0; r < towns[s].roads.size(); ++r) {
            const auto& road = towns[s].roads[r];
            auto& info = roads_[s][r];
            const bool slip = road.type == RoadType::Ramp;
            if (!slip && !(info.highway && road.width >= min_width && info.start_half == paved_half && info.end_half == paved_half)) continue;
            const Vector2 a{road.start.x, road.start.z}, b{road.end.x, road.end.z};
            const float length = len(sub(b, a));
            if (!(length > step)) continue;
            const Vector2 d = unit(sub(b, a)), right = right_of(d);
            if (std::any_of(interchanges().begin(), interchanges().end(), [&](const Interchange& x) {
                    return std::min(len(sub(a, {x.at.x, x.at.z})), len(sub(b, {x.at.x, x.at.z}))) < roundabout_pad + 4; }))
                continue;   // (into the roundabout: its pad is level)
            const float out = slip ? slip_width * .5F + 1.1F : paved_half + verge - .45F;
            const float every = slip ? step : 2 * step;   // (the long highways more coarsely)
            for (const float side : {-1.0F, 1.0F}) {
                std::vector<Span> spans;
                for (float at = every * .5F; at < length; at += every) {
                    if (!slip && inside_any(info.plain, at)) continue;
                    const Vector2 e{a.x + d.x * at + right.x * side * out, a.y + d.y * at + right.y * side * out};
                    float y = 0;
                    if (deck(e.x, e.y, y)) continue;
                    y = terrain::TerrainHeight::sample(e.x, e.y);
                    float land = 1e9F;
                    for (const float further : {3.0F, 6.0F})
                        land = std::min(land, terrain::TerrainHeight::sample(e.x + right.x * side * further, e.y + right.y * side * further));
                    if (y - land < drop) continue;
                    // Another road there (not the pieces this one runs on into).
                    bool clear = true;
                    for (const auto& o : others) {
                        if (o.s == s && o.r == r) continue;
                        if (std::min({len(sub(o.a, a)), len(sub(o.a, b)), len(sub(o.b, a)), len(sub(o.b, b))}) < 1) continue;
                        const Vector2 od = sub(o.b, o.a), q = sub(e, o.a);
                        const float l2 = dot(od, od);
                        const float t = l2 < 1e-6F ? 0 : std::clamp(dot(q, od) / l2, 0.0F, 1.0F);
                        if (len(sub(q, {od.x * t, od.y * t})) < o.half + 2.5F) {clear = false; break;}
                    }
                    if (!clear) continue;
                    const Span here{std::max(0.0F, at - every * .5F), std::min(length, at + every * .5F)};
                    if (!spans.empty() && here.from - spans.back().to <= bridge_gap) spans.back().to = here.to;
                    else spans.push_back(here);
                }
                for (const auto& span : spans) {
                    if (span.to - span.from < shortest) continue;
                    info.rails.push_back({side * out, span.from, span.to});
                    ++rail_count_;
                    // Solid, in short runs that each know their level.
                    const int count = std::max(1, static_cast<int>(std::ceil((span.to - span.from) / 4)));
                    for (int k = 0; k < count; ++k) {
                        const float f0 = span.from + (span.to - span.from) * k / count, f1 = span.from + (span.to - span.from) * (k + 1) / count;
                        const Vector2 p0{a.x + d.x * f0 + right.x * side * out, a.y + d.y * f0 + right.y * side * out};
                        const Vector2 p1{a.x + d.x * f1 + right.x * side * out, a.y + d.y * f1 + right.y * side * out};
                        add_run({p0, p1, terrain::TerrainHeight::sample((p0.x + p1.x) * .5F, (p0.y + p1.y) * .5F), true});
                    }
                }
            }
        }
}

const std::vector<Rail>& Network::rails(std::size_t settlement, std::size_t road) const {
    static const std::vector<Rail> none;
    if (settlement >= roads_.size() || road >= roads_[settlement].size()) return none;
    return roads_[settlement][road].rails;
}

// Under each ring bridge: the two piers on the roundabout's island, the abutments at the deck's
// ends holding back the embankment, and the retaining walls between the climbing ring and the slip
// roads beside it - walls for cars and people alike, each at its own level (traffic up on the deck
// passes over). The island's kerb is low and mountable (a car cutting the corner, a rig's trailer,
// rolls over the apron and onto the grass rather than into a wall).
void Network::islands() {
    for (const auto& x : interchanges()) {
        const float y = terrain::TerrainHeight::sample(x.at.x, x.at.z);
        const Vector2 f{x.ring_forward.x, x.ring_forward.z}, o{x.outward.x, x.outward.z};
        const float radius = std::hypot(x.at.x, x.at.z), bearing = std::atan2(x.at.z, x.at.x);
        const float sense = x.ring_forward.x * -std::sin(bearing) + x.ring_forward.z * std::cos(bearing) > 0 ? 1.0F : -1.0F;
        // A point `s` metres along the ring from the crossing, `lateral` out from its centre line
        // (as HighwayRender's bridge() draws them).
        const auto ring_point = [&](float s, float lateral) {
            const float b = bearing + sense * s / radius, r = radius + lateral;
            return Vector2{r * std::cos(b), r * std::sin(b)};
        };
        for (const float side : {-1.0F, 1.0F}) {
            // A pier: 2.4 m along the ring, 5 m across (as drawn), outlined by runs.
            const Vector2 c{x.at.x + f.x * side * pier_along, x.at.z + f.y * side * pier_along};
            const float ha = 1.2F - barrier_half, hc = 2.5F - barrier_half;
            const auto corner = [&](float a, float b) { return Vector2{c.x + f.x * a + o.x * b, c.y + f.y * a + o.y * b}; };
            const Vector2 p0 = corner(-ha, -hc), p1 = corner(ha, -hc), p2 = corner(ha, hc), p3 = corner(-ha, hc);
            for (const auto& [a, b] : {std::pair{p0, p1}, std::pair{p1, p2}, std::pair{p2, p3}, std::pair{p3, p0}}) add_run({a, b, y, true, true});
            // The abutment: across the whole roadbed at the deck's end.
            const float at = side * (roundabout_pad + .6F), across = grade_flat + 1;
            add_run({ring_point(at, -across), ring_point(at, across), y, true, true});
            // The retaining walls, where the ring stands .4 m or more above the slip road beside it.
            for (const float lateral : {-1.0F, 1.0F}) {
                bool open = false;
                Vector2 from{};
                float low_from = 0;
                for (float t = roundabout_pad + 2; t < slip_reach - 10; t += 2) {
                    const Vector2 top = ring_point(side * t, lateral * (grade_flat + .8F)), foot = ring_point(side * t, lateral * (grade_flat + 4.5F));
                    const float high = terrain::TerrainHeight::sample(top.x, top.y), low = terrain::TerrainHeight::sample(foot.x, foot.y);
                    const Vector2 here = ring_point(side * t, lateral * (grade_flat + 1.6F));
                    if (high - low < .4F) {open = false; continue;}
                    if (open) add_run({from, here, low_from, true, true});
                    from = here; low_from = low; open = true;
                }
            }
        }
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

bool Network::barrier_hit(Vector3 p, float radius) const { return hit(p, radius, false); }
bool Network::wall_hit(Vector3 p, float radius) const { return hit(p, radius, true); }

bool Network::hit(Vector3 p, float radius, bool walls) const {
    if (runs_.empty()) return false;
    const auto found = grid_.find(cell_key(static_cast<int>(std::floor(p.x / cell)), static_cast<int>(std::floor(p.z / cell))));
    if (found == grid_.end()) return false;
    for (const int i : found->second) {
        const auto& run = runs_[static_cast<std::size_t>(i)];
        if (walls && !run.wall) continue;
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
