#include "outland/world/SovagejoCounty.hpp"
#include "outland/world/MountainRoad.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace outland::world::county {
namespace {
float smooth(float v) { v = std::clamp(v, 0.0F, 1.0F); return v * v * (3 - 2 * v); }
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 unit(float x, float z) { const float l = std::max(1e-4F, std::hypot(x, z)); return {x / l, 0, z / l}; }
float distance_to_segment(Vector3 p, Vector3 a, Vector3 b, float& t) {
    const float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
    t = l2 > 1e-8F ? std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / l2, 0.0F, 1.0F) : 0;
    return std::hypot(p.x - a.x - dx * t, p.z - a.z - dz * t);
}
// The land before the county shapes it: the island's, cut and filled to the mountain road.
float base(float x, float z) { return mountain::graded(x, z, terrain::TerrainHeight::ungraded(x, z)); }

// The homes: where each stands, the point its front faces, its pad, its family.
struct SiteDef { Home kind; const char* name; Vector2 centre, faces; float flat; int dwellings; };
constexpr std::array<SiteDef, 8> site_defs{{
    {Home::RoadsideTrailer, "BRANDT", {-1748, -18}, {-1745, 4}, 11, 1},
    {Home::HuntingCabin, "HARALDSEN", {-1760, -121}, {-1790, -118}, 10, 1},
    {Home::Compound, "KOVAĈ", {-1690, -184}, {-1702, -166}, 22, 2},
    {Home::CreekFarm, "MORENO", {-1592, -192}, {-1600, -165}, 14, 1},
    {Home::CreekTrailer, "ŜIMKO", {-1586, -112}, {-1614, -112}, 11, 1},
    {Home::WestFarm, "LINDQVIST", {-1925, -72}, {-1914, -56}, 14, 1},
    {Home::FisherTrailer, "O'NEIL", {-1962, -8}, {-1944, -14}, 13, 1},
    {Home::Workshop, "PETROV", {-1718, 262}, {-1740, 263}, 20, 2},
}};
constexpr float site_blend = 14;
// Fajrejo's lot: from the west node (the east road comes in) to the east node.
constexpr Vector2 lot_west{-1680, -46}, lot_east{-1640, -64};

// The tracks: what each leaves from, its bends, and the home (or the village) it ends at.
constexpr int to_village = -2;
struct TrackDef { const char* name; From from; int parent; float mountain_s; std::array<Vector2, 6> bends; int bend_count; int end; };
constexpr std::array<TrackDef, 7> track_defs{{
    {"Orienta vojo", From::Mountain, -1, 20, {{{-1765, 14}, {-1738, 2}, {-1712, -16}, {-1693, -36}, lot_west}}, 5, to_village},
    {"Kreka vojo", From::Village, -1, 0, {{lot_east, {-1626, -84}, {-1614, -112}, {-1606, -140}, {-1595, -181}}}, 5, 3},
    {"Arbara vojeto", From::Village, -1, 0, {{lot_west, {-1700, -70}, {-1716, -98}, {-1722, -128}, {-1712, -152}, {-1702, -167}}}, 6, 2},
    {"Ĉasista vojeto", From::Mountain, -1, 170, {{{-1818, -119}, {-1797, -117}, {-1781, -119}, {-1769, -120}}}, 4, 1},
    {"Okcidenta vojo", From::Mountain, -1, 104, {{{-1845, -50}, {-1872, -42}, {-1900, -34}, {-1926, -24}, {-1951, -13}}}, 5, 6},
    {"Farmvojeto", From::Track, 4, 0, {{{-1900, -34}, {-1908, -49}, {-1917, -63}}}, 3, 5},
    {"Suda vojeto", From::Homesteads, -1, 0, {{{-1790, 265}, {-1768, 265}, {-1752, 264}, {-1736, 263}}}, 4, 7},
}};

// Centripetal Catmull-Rom through the points, resampled every `step` metres.
std::vector<Vector2> spline(const std::vector<Vector2>& p) {
    const auto len = [](Vector2 a, Vector2 b) { return std::hypot(a.x - b.x, a.y - b.y); };
    std::vector<Vector2> padded;
    padded.push_back({2 * p[0].x - p[1].x, 2 * p[0].y - p[1].y});
    padded.insert(padded.end(), p.begin(), p.end());
    padded.push_back({2 * p.back().x - p[p.size() - 2].x, 2 * p.back().y - p[p.size() - 2].y});
    std::vector<Vector2> dense;
    for (std::size_t i = 1; i + 2 < padded.size(); ++i) {
        const Vector2 p0 = padded[i - 1], p1 = padded[i], p2 = padded[i + 1], p3 = padded[i + 2];
        const float t0 = 0, t1 = t0 + std::sqrt(std::max(1e-3F, len(p1, p0))), t2 = t1 + std::sqrt(std::max(1e-3F, len(p2, p1))),
                    t3 = t2 + std::sqrt(std::max(1e-3F, len(p3, p2)));
        const int n = std::max(2, static_cast<int>(len(p2, p1) * 2));
        for (int k = 0; k < n; ++k) {
            const float t = t1 + (t2 - t1) * static_cast<float>(k) / static_cast<float>(n);
            const auto mix = [](Vector2 a, Vector2 b, float wa, float wb) { return Vector2{a.x * wa + b.x * wb, a.y * wa + b.y * wb}; };
            const Vector2 a1 = mix(p0, p1, (t1 - t) / (t1 - t0), (t - t0) / (t1 - t0));
            const Vector2 a2 = mix(p1, p2, (t2 - t) / (t2 - t1), (t - t1) / (t2 - t1));
            const Vector2 a3 = mix(p2, p3, (t3 - t) / (t3 - t2), (t - t2) / (t3 - t2));
            const Vector2 b1 = mix(a1, a2, (t2 - t) / (t2 - t0), (t - t0) / (t2 - t0));
            const Vector2 b2 = mix(a2, a3, (t3 - t) / (t3 - t1), (t - t1) / (t3 - t1));
            dense.push_back(mix(b1, b2, (t2 - t) / (t2 - t1), (t - t1) / (t2 - t1)));
        }
    }
    dense.push_back(p.back());
    std::vector<float> s(dense.size(), 0);
    for (std::size_t i = 1; i < dense.size(); ++i) s[i] = s[i - 1] + len(dense[i], dense[i - 1]);
    std::vector<Vector2> out;
    std::size_t j = 0;
    for (float at = 0;; at += step) {
        const bool last = at >= s.back();
        if (last) at = s.back();
        while (j + 2 < dense.size() && s[j + 1] < at) ++j;
        const float t = s[j + 1] > s[j] ? std::clamp((at - s[j]) / (s[j + 1] - s[j]), 0.0F, 1.0F) : 0;
        out.push_back({dense[j].x + (dense[j + 1].x - dense[j].x) * t, dense[j].y + (dense[j + 1].y - dense[j].y) * t});
        if (last) break;
    }
    return out;
}

struct Plan {
    std::vector<Track> tracks;
    std::vector<std::vector<float>> band;   // per track, per sample: the banks' width
    std::vector<Site> sites;
    Village village;
    // A grid over the county: which segments (track, sample) come near each cell.
    float x0{0}, z0{0};
    int cols{0}, rows{0};
    static constexpr float cell = 16, reach = bed_half + 18;
    std::vector<std::vector<std::pair<int, int>>> cells;
};

float pad_level(Vector3 c, float radius) {
    float sum = 0;
    int n = 0;
    for (float r : {0.0F, radius * .5F, radius})
        for (int a = 0; a < 12; ++a) {
            const float angle = static_cast<float>(a) * 6.2831853F / 12;
            sum += base(c.x + r * std::cos(angle), c.z + r * std::sin(angle));
            ++n;
        }
    return sum / static_cast<float>(n);
}

const Plan& plan() {
    static const Plan built = [] {
        Plan P;
        // The village and the homes' pads, each at its ground's mean height.
        Village& v = P.village;
        v.west = {lot_west.x, 0, lot_west.y};
        v.east = {lot_east.x, 0, lot_east.y};
        v.centre = {(v.west.x + v.east.x) * .5F, 0, (v.west.z + v.east.z) * .5F};
        v.along = unit(v.east.x - v.west.x, v.east.z - v.west.z);
        v.across = {-v.along.z, 0, v.along.x};
        v.centre.y = v.west.y = v.east.y = pad_level(v.centre, v.flat * .8F);
        for (const auto& d : site_defs) {
            Site site;
            site.kind = d.kind;
            site.name = d.name;
            site.centre = {d.centre.x, 0, d.centre.y};
            site.front = unit(d.faces.x - d.centre.x, d.faces.y - d.centre.y);
            site.flat = d.flat;
            site.blend = site_blend;
            site.dwellings = d.dwellings;
            site.centre.y = pad_level(site.centre, d.flat);
            P.sites.push_back(site);
        }
        // The tracks, parents first (the order they're listed in).
        const float rise = max_grade * step;
        for (const auto& d : track_defs) {
            Track track;
            track.name = d.name;
            track.from = d.from;
            track.parent = d.parent;
            std::vector<Vector2> bends;
            float start_y = 0;
            if (d.from == From::Mountain) {
                const Vector3 j = mountain::road().at(d.mountain_s);
                bends.push_back({j.x, j.z});
                start_y = j.y;
            }
            for (int i = 0; i < d.bend_count; ++i) bends.push_back(d.bends[static_cast<std::size_t>(i)]);
            if (d.from == From::Homesteads) start_y = base(bends[0].x, bends[0].y);
            if (d.from == From::Village) start_y = v.centre.y;
            if (d.from == From::Track) {
                const auto& parent = P.tracks[static_cast<std::size_t>(d.parent)];
                float best = 1e9F;
                const Vector2 want = bends[0];
                for (const auto& q : parent.points) {
                    const float dd = std::hypot(q.x - want.x, q.z - want.y);
                    if (dd < best) {best = dd; start_y = q.y; bends[0] = {q.x, q.z};}
                }
            }
            const auto pts = spline(bends);
            const std::size_t n = pts.size();
            std::vector<float> s(n, 0), nat(n, 0), h(n, 0);
            for (std::size_t i = 0; i < n; ++i) {
                if (i) s[i] = s[i - 1] + std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
                nat[i] = base(pts[i].x, pts[i].y);
            }
            constexpr int k = 8;
            for (std::size_t i = 0; i < n; ++i) {
                float sum = 0;
                for (int dd = -k; dd <= k; ++dd) sum += nat[static_cast<std::size_t>(std::clamp(static_cast<long>(i) + dd, 0L, static_cast<long>(n - 1)))];
                h[i] = sum / (2 * k + 1);
            }
            // Pinned: the first 10 m level with what it leaves; anything on the village's or a
            // home's pad at the pad's level; the end at its home's (or the village's) level.
            std::vector<bool> fixed(n, false);
            const float level_run = d.from == From::Track ? 4.0F : 10.0F;
            for (std::size_t i = 0; i < n && s[i] <= level_run; ++i) {h[i] = start_y; fixed[i] = true;}
            for (std::size_t i = 0; i < n; ++i) {
                const Vector3 q{pts[i].x, 0, pts[i].y};
                if (flat(q, v.centre) < v.flat) {h[i] = v.centre.y; fixed[i] = true;}
                for (const auto& site : P.sites) if (flat(q, site.centre) < site.flat) {h[i] = site.centre.y; fixed[i] = true;}
            }
            const float end_y = d.end == to_village ? v.centre.y : P.sites[static_cast<std::size_t>(d.end)].centre.y;
            for (std::size_t i = n; i-- > 0 && s.back() - s[i] <= 6;) {h[i] = end_y; fixed[i] = true;}
            for (int pass = 0; pass < 4; ++pass) {
                for (std::size_t i = 1; i < n; ++i) if (!fixed[i]) h[i] = std::clamp(h[i], h[i - 1] - rise, h[i - 1] + rise);
                for (std::size_t i = n - 1; i-- > 0;) if (!fixed[i]) h[i] = std::clamp(h[i], h[i + 1] - rise, h[i + 1] + rise);
            }
            track.s = s;
            for (std::size_t i = 0; i < n; ++i) track.points.push_back({pts[i].x, h[i], pts[i].y});
            std::vector<float> band(n, 6);
            for (std::size_t i = 0; i < n; ++i) band[i] = std::clamp(6 + 1.5F * std::abs(h[i] - nat[i]), 6.0F, 16.0F);
            P.band.push_back(band);
            P.tracks.push_back(std::move(track));
        }
        // The grid.
        float x0 = 1e9F, z0 = 1e9F, x1 = -1e9F, z1 = -1e9F;
        const auto grow = [&](Vector3 p, float r) {x0 = std::min(x0, p.x - r); z0 = std::min(z0, p.z - r); x1 = std::max(x1, p.x + r); z1 = std::max(z1, p.z + r);};
        for (const auto& t : P.tracks) for (const auto& p : t.points) grow(p, Plan::reach + Plan::cell);
        grow(v.centre, v.flat + v.blend + Plan::cell);
        for (const auto& site : P.sites) grow(site.centre, site.flat + site.blend + Plan::cell);
        P.x0 = x0; P.z0 = z0;
        P.cols = static_cast<int>(std::ceil((x1 - x0) / Plan::cell)) + 1;
        P.rows = static_cast<int>(std::ceil((z1 - z0) / Plan::cell)) + 1;
        P.cells.assign(static_cast<std::size_t>(P.cols * P.rows), {});
        for (std::size_t t = 0; t < P.tracks.size(); ++t) {
            const auto& pts = P.tracks[t].points;
            for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                const Vector3 a = pts[i], b = pts[i + 1];
                const float r = Plan::reach;
                const int cx0 = static_cast<int>(std::floor((std::min(a.x, b.x) - r - x0) / Plan::cell));
                const int cx1 = static_cast<int>(std::floor((std::max(a.x, b.x) + r - x0) / Plan::cell));
                const int cz0 = static_cast<int>(std::floor((std::min(a.z, b.z) - r - z0) / Plan::cell));
                const int cz1 = static_cast<int>(std::floor((std::max(a.z, b.z) + r - z0) / Plan::cell));
                for (int cz = std::max(0, cz0); cz <= std::min(P.rows - 1, cz1); ++cz)
                    for (int cx = std::max(0, cx0); cx <= std::min(P.cols - 1, cx1); ++cx)
                        P.cells[static_cast<std::size_t>(cz * P.cols + cx)].push_back({static_cast<int>(t), static_cast<int>(i)});
            }
        }
        return P;
    }();
    return built;
}
} // namespace

Vector3 Track::at(float metres) const {
    if (points.empty()) return {};
    if (metres <= 0) return points.front();
    if (metres >= s.back()) return points.back();
    const std::size_t i = std::min(static_cast<std::size_t>(std::upper_bound(s.begin(), s.end(), metres) - s.begin()) - 1, s.size() - 2);
    const float t = (metres - s[i]) / std::max(1e-4F, s[i + 1] - s[i]);
    return {points[i].x + (points[i + 1].x - points[i].x) * t, points[i].y + (points[i + 1].y - points[i].y) * t,
            points[i].z + (points[i + 1].z - points[i].z) * t};
}
Vector3 Track::heading(float metres) const {
    const Vector3 a = at(metres - step), b = at(metres + step);
    return unit(b.x - a.x, b.z - a.z);
}
Vector3 Site::at(float right, float forward) const {
    const Vector3 r{-front.z, 0, front.x};
    return {centre.x + r.x * right + front.x * forward, centre.y, centre.z + r.z * right + front.z * forward};
}
Vector3 Village::at(float a, float b) const {
    return {centre.x + along.x * a + across.x * b, centre.y, centre.z + along.z * a + across.z * b};
}

const std::vector<Track>& tracks() { return plan().tracks; }
const std::vector<Site>& sites() { return plan().sites; }
const Village& village() { return plan().village; }

float graded(float x, float z, float height) {
    const Plan& P = plan();
    const int cx = static_cast<int>(std::floor((x - P.x0) / Plan::cell)), cz = static_cast<int>(std::floor((z - P.z0) / Plan::cell));
    if (cx < 0 || cz < 0 || cx >= P.cols || cz >= P.rows) return height;
    const Vector3 p{x, 0, z};
    float num = height, den = 1;
    const auto add = [&](float w, float y) {
        if (w <= 0) return;
        const float omega = w / (1 - w + 1e-5F);
        num += omega * y; den += omega;
    };
    const auto weight = [](float d, float bed, float band) { return d <= bed ? 1.0F : 1 - smooth((d - bed) / band); };
    // Each track: its nearest stretch.
    std::array<float, 8> best_d;
    std::array<int, 8> best_i;
    best_d.fill(1e9F); best_i.fill(-1);
    std::array<float, 8> best_t{};
    for (const auto& [t, i] : P.cells[static_cast<std::size_t>(cz * P.cols + cx)]) {
        const auto& pts = P.tracks[static_cast<std::size_t>(t)].points;
        float u = 0;
        const float d = distance_to_segment(p, pts[static_cast<std::size_t>(i)], pts[static_cast<std::size_t>(i) + 1], u);
        if (d < best_d[static_cast<std::size_t>(t)]) {best_d[static_cast<std::size_t>(t)] = d; best_i[static_cast<std::size_t>(t)] = i; best_t[static_cast<std::size_t>(t)] = u;}
    }
    for (std::size_t t = 0; t < P.tracks.size() && t < best_d.size(); ++t) {
        if (best_i[t] < 0) continue;
        const auto& pts = P.tracks[t].points;
        const auto i = static_cast<std::size_t>(best_i[t]);
        const float y = pts[i].y + (pts[i + 1].y - pts[i].y) * best_t[t];
        add(weight(best_d[t], bed_half, P.band[t][i]), y);
    }
    const Village& v = P.village;
    if (const float d = flat(p, v.centre); d < v.flat + v.blend) add(weight(d, v.flat, v.blend), v.centre.y);
    for (const auto& site : P.sites)
        if (const float d = flat(p, site.centre); d < site.flat + site.blend) add(weight(d, site.flat, site.blend), site.centre.y);
    if (den <= 1) return height;
    // The mountain road's own bed is its own: the county's shaping fades out toward it.
    const float clear = smooth((mountain::distance(x, z) - mountain::bed_half - .5F) / 7);
    return height + (num / den - height) * clear;
}

bool near(float x, float z, float margin) {
    const Plan& P = plan();
    if (x < P.x0 - margin || z < P.z0 - margin || x > P.x0 + static_cast<float>(P.cols) * Plan::cell + margin ||
        z > P.z0 + static_cast<float>(P.rows) * Plan::cell + margin) return false;
    const Vector3 p{x, 0, z};
    if (flat(p, P.village.centre) < P.village.flat + margin) return true;
    for (const auto& site : P.sites) if (flat(p, site.centre) < site.flat + margin) return true;
    float t = 0;
    for (const auto& track : P.tracks)
        for (std::size_t i = 0; i + 1 < track.points.size(); i += 1)
            if (distance_to_segment(p, track.points[i], track.points[i + 1], t) < bed_half + margin) return true;
    return false;
}

} // namespace outland::world::county
