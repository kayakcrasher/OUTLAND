#include "outland/world/MountainRoad.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace outland::world::mountain {
namespace {
constexpr float pi = 3.14159265358979F;
float smooth(float v) { v = std::clamp(v, 0.0F, 1.0F); return v * v * (3 - 2 * v); }
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }

// The same deterministic pseudo-noise as the land's (TerrainHeight), for the range's crags.
float pseudo(float x, float z) {
    const float a = std::sin(x * .0173F + z * .0117F);
    const float b = std::sin(x * .0091F - z * .0153F + 1.73F);
    const float c = std::cos(x * .0237F + z * .0191F + .81F);
    return (a + b * .65F + c * .35F) / 2;
}
float ridge(float x, float z) { return 1 - std::fabs(pseudo(x, z)); }

// The range: peaks along the coast, each a smooth mound laid along the shore (bearing from the
// island's centre in degrees, its distance out, its height, and its spread along and across the
// coast in metres).
struct Peak { float bearing, radius, height, along, across; };
constexpr std::array<Peak, 4> peaks{{
    {222, 1880, 300, 230, 190},   // Sovaĝa Monto, the summit
    {206, 1870, 120, 200, 170},   // the ridge behind La Ringo Okcidento
    {192, 1860, 70, 160, 150},    // the south shoulder above Sovaĝejo
    {238, 1865, 115, 230, 170},   // the north ridge
}};

// The road's line, laid out over the range (each bend placed where the land is the road's own
// height): Sovaĝejo's north gate to the ring road's verge. Smoothed through these points.
constexpr std::array<Vector2, 57> waypoints{{
    {-1790.0F, 40.0F}, {-1790.0F, 0.0F}, {-1822.9F, -62.7F}, {-1837.1F, -119.6F}, {-1828.5F, -164.1F},
    {-1801.0F, -201.2F}, {-1761.9F, -240.1F}, {-1726.3F, -288.5F}, {-1702.7F, -343.6F}, {-1698.1F, -403.5F},
    {-1722.2F, -461.4F}, {-1763.4F, -506.2F}, {-1774.8F, -553.6F}, {-1755.1F, -592.9F}, {-1716.1F, -619.9F},
    {-1667.0F, -647.1F}, {-1627.8F, -691.9F}, {-1605.2F, -743.8F}, {-1587.6F, -792.0F}, {-1568.2F, -837.6F},
    {-1547.1F, -883.3F}, {-1531.0F, -928.5F}, {-1516.2F, -966.7F}, {-1486.8F, -994.2F}, {-1441.9F, -1015.8F},
    {-1389.5F, -1034.7F}, {-1340.2F, -1057.1F}, {-1299.2F, -1087.4F}, {-1264.7F, -1124.3F}, {-1233.3F, -1165.0F},
    {-1205.1F, -1205.8F}, {-1181.7F, -1249.3F}, {-1163.8F, -1298.0F}, {-1149.8F, -1349.7F}, {-1129.0F, -1395.1F},
    {-1095.3F, -1424.5F}, {-1048.1F, -1433.1F}, {-997.3F, -1441.5F}, {-947.6F, -1454.5F}, {-899.2F, -1470.0F},
    {-839.2F, -1526.5F}, {-800.8F, -1571.7F}, {-768.0F, -1610.2F}, {-730.8F, -1618.7F}, {-718.2F, -1598.0F},
    {-738.6F, -1555.5F}, {-780.5F, -1505.7F}, {-830.9F, -1456.7F}, {-869.8F, -1413.9F}, {-888.4F, -1378.5F},
    {-874.6F, -1362.4F}, {-835.2F, -1368.2F}, {-776.7F, -1389.7F}, {-710.1F, -1405.8F}, {-641.9F, -1408.6F},
    {-608.9F, -1400.4F}, {-602.5F, -1385.7F},
}};
// Where the pullouts stand (metres along) - the lookout under the summit among them - and the fork.
constexpr std::array<float, 6> pullout_at{330, 1250, 1550, 1800, 2050, 2780};
constexpr float lookout_at = 1550;
constexpr float fork_at = 680;
constexpr float hamlet_out = 62;          // the pad's centre, out from the fork
// The roadbed's banks: this wide where the road sits on the land, wider where it is cut deep
// into it or built up over it.
constexpr float band_min = 14, band_max = 34;
constexpr float reach_max = bed_half + pullout_bench + band_max;
// Another stretch of the road counts as another leg (a hairpin's other side) this far along.
constexpr float leg_apart = 30;

// Centripetal Catmull-Rom through the waypoints, resampled every `step` metres.
std::vector<Vector2> spline() {
    std::vector<Vector2> p(waypoints.begin(), waypoints.end());
    const auto sub = [](Vector2 a, Vector2 b) { return Vector2{a.x - b.x, a.y - b.y}; };
    const auto len = [](Vector2 a) { return std::hypot(a.x, a.y); };
    std::vector<Vector2> padded;
    padded.push_back({2 * p[0].x - p[1].x, 2 * p[0].y - p[1].y});
    padded.insert(padded.end(), p.begin(), p.end());
    padded.push_back({2 * p.back().x - p[p.size() - 2].x, 2 * p.back().y - p[p.size() - 2].y});
    std::vector<Vector2> dense;
    for (std::size_t i = 1; i + 2 < padded.size(); ++i) {
        const Vector2 p0 = padded[i - 1], p1 = padded[i], p2 = padded[i + 1], p3 = padded[i + 2];
        const float t0 = 0, t1 = t0 + std::sqrt(len(sub(p1, p0))), t2 = t1 + std::sqrt(len(sub(p2, p1))), t3 = t2 + std::sqrt(len(sub(p3, p2)));
        const int n = std::max(2, static_cast<int>(len(sub(p2, p1))));
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
    for (std::size_t i = 1; i < dense.size(); ++i) s[i] = s[i - 1] + len(sub(dense[i], dense[i - 1]));
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

// Everything the road is laid out from, built once.
struct Plan {
    Line line;
    std::vector<float> band;                  // per sample
    std::vector<Pullout> pullouts;
    Hamlet hamlet;
    Vector3 summit{};
    // A grid over the road's reach: which samples' segments come near each cell.
    float x0{0}, z0{0};
    int cols{0}, rows{0};
    static constexpr float cell = 16;
    std::vector<std::vector<int>> cells;
};

float land(float x, float z) { return terrain::TerrainHeight::ungraded(x, z); }

const Plan& plan() {
    static const Plan built = [] {
        Plan L;
        const auto points = spline();
        const std::size_t n = points.size();
        std::vector<float> s(n, 0), nat(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            if (i > 0) s[i] = s[i - 1] + std::hypot(points[i].x - points[i - 1].x, points[i].y - points[i - 1].y);
            nat[i] = land(points[i].x, points[i].y);
        }
        const auto index = [&](float metres) { return static_cast<std::size_t>(std::clamp(std::lround(metres / step), 0L, static_cast<long>(n - 1))); };
        const auto heading = [&](std::size_t i) {
            const auto a = points[i > 0 ? i - 1 : 0], b = points[std::min(i + 1, n - 1)];
            const float l = std::max(1e-4F, std::hypot(b.x - a.x, b.y - a.y));
            return Vector3{(b.x - a.x) / l, 0, (b.y - a.y) / l};
        };
        // Level stretches: each pullout, the fork, and the hairpins (their inside is one level).
        struct Run { std::size_t from, to; };
        std::vector<Run> level;
        for (const float at : pullout_at) level.push_back({index(at - pullout_core - 12), index(at + pullout_core + 12)});
        level.push_back({index(fork_at - 24), index(fork_at + 24)});
        {
            // The hairpins: the tightest bends past the climb.
            std::vector<float> radius(n, 1e9F);
            for (std::size_t i = 2; i + 2 < n; ++i) {
                const Vector2 a = points[i - 2], b = points[i], c = points[i + 2];
                const float ab = std::hypot(b.x - a.x, b.y - a.y), bc = std::hypot(c.x - b.x, c.y - b.y), ca = std::hypot(a.x - c.x, a.y - c.y);
                const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
                radius[i] = ab * bc * ca / (2 * std::abs(cross) + 1e-6F);
            }
            for (std::size_t i = 3; i + 3 < n; ++i)
                if (radius[i] < 26 && radius[i] <= radius[i - 1] && radius[i] <= radius[i + 1] && s[i] > 2000 && s[i] < s.back() - 60)
                    level.push_back({index(s[i] - 18), index(s[i] + 18)});
        }
        // The profile: the land along the line, evened out over 40 m either way, the level
        // stretches made level, then eased to the grade limit (both ways, a few times over).
        std::vector<float> h(n, 0);
        constexpr int k = 10;
        for (std::size_t i = 0; i < n; ++i) {
            float sum = 0;
            for (int d = -k; d <= k; ++d) sum += nat[static_cast<std::size_t>(std::clamp(static_cast<long>(i) + d, 0L, static_cast<long>(n - 1)))];
            h[i] = sum / (2 * k + 1);
        }
        // Overlapping stretches are one; each is set at its mean height, then (with the two ends)
        // kept within reach of its neighbours at the grade limit, so the road can always join them.
        const float rise = max_grade * step;
        std::sort(level.begin(), level.end(), [](const Run& a, const Run& b) { return a.from < b.from; });
        std::vector<Run> runs{{0, 1}};
        for (const auto& run : level) {
            if (run.from <= runs.back().to) runs.back().to = std::max(runs.back().to, run.to);
            else runs.push_back(run);
        }
        runs.push_back({n - 1, n});
        std::vector<float> height(runs.size(), 0);
        for (std::size_t r = 0; r < runs.size(); ++r) {
            float sum = 0;
            for (std::size_t i = runs[r].from; i < runs[r].to; ++i) sum += h[i];
            height[r] = sum / static_cast<float>(runs[r].to - runs[r].from);
        }
        height.front() = nat.front(); height.back() = nat.back();
        for (int pass = 0; pass < 2; ++pass) {
            for (std::size_t r = 1; r + 1 < runs.size(); ++r) {
                const float gap = rise * static_cast<float>(runs[r].from - runs[r - 1].to + 1);
                height[r] = std::clamp(height[r], height[r - 1] - gap, height[r - 1] + gap);
            }
            for (std::size_t r = runs.size() - 1; r-- > 1;) {
                const float gap = rise * static_cast<float>(runs[r + 1].from - runs[r].to + 1);
                height[r] = std::clamp(height[r], height[r + 1] - gap, height[r + 1] + gap);
            }
        }
        std::vector<bool> fixed(n, false);
        for (std::size_t r = 0; r < runs.size(); ++r)
            for (std::size_t i = runs[r].from; i < runs[r].to; ++i) {h[i] = height[r]; fixed[i] = true;}
        for (int pass = 0; pass < 3; ++pass) {
            for (std::size_t i = 1; i < n; ++i) if (!fixed[i]) h[i] = std::clamp(h[i], h[i - 1] - rise, h[i - 1] + rise);
            for (std::size_t i = n - 1; i-- > 0;) if (!fixed[i]) h[i] = std::clamp(h[i], h[i + 1] - rise, h[i + 1] + rise);
        }
        L.line.s = s;
        for (std::size_t i = 0; i < n; ++i) L.line.points.push_back({points[i].x, h[i], points[i].y});
        // The banks: wider where the road is cut deep or built up (across the road too, 14 m out).
        std::vector<float> band(n, band_min);
        for (std::size_t i = 0; i < n; ++i) {
            const Vector3 t = heading(i), left{-t.z, 0, t.x};
            const float a = land(points[i].x + left.x * 14, points[i].y + left.z * 14), b = land(points[i].x - left.x * 14, points[i].y - left.z * 14);
            const float deep = std::max({std::abs(h[i] - nat[i]), .6F * std::abs(h[i] - a), .6F * std::abs(h[i] - b)});
            band[i] = std::clamp(10 + 1.2F * deep, band_min, band_max);
        }
        L.band.assign(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            float most = 0;
            for (int d = -5; d <= 5; ++d) most = std::max(most, band[static_cast<std::size_t>(std::clamp(static_cast<long>(i) + d, 0L, static_cast<long>(n - 1)))]);
            L.band[i] = most;
        }
        // The pullouts, each on the side where the land lies nearer the roadbed (less to cut or fill).
        for (const float at : pullout_at) {
            const std::size_t i = index(at);
            Pullout p;
            p.s = s[i]; p.lookout = at == lookout_at;
            p.centre = L.line.points[i];
            p.along = heading(i);
            const Vector3 left{-p.along.z, 0, p.along.x};
            const float l = std::abs(land(p.centre.x + left.x * 16, p.centre.z + left.z * 16) - p.centre.y);
            const float r = std::abs(land(p.centre.x - left.x * 16, p.centre.z - left.z * 16) - p.centre.y);
            p.side = l <= r ? 1.0F : -1.0F;
            p.out = Vector3Scale(left, p.side);
            L.pullouts.push_back(p);
        }
        // Supra Sovaĝejo, seaward of the fork (away from the island's middle).
        {
            const std::size_t i = index(fork_at);
            Hamlet& m = L.hamlet;
            m.fork_s = s[i];
            m.fork = L.line.points[i];
            const Vector3 t = heading(i);
            Vector3 out{-t.z, 0, t.x};
            if (out.x * m.fork.x + out.z * m.fork.z < 0) out = Vector3Scale(out, -1);
            m.lane = out;
            m.centre = {m.fork.x + out.x * hamlet_out, m.fork.y, m.fork.z + out.z * hamlet_out};
        }
        // The summit: the range's highest ground near the main peak.
        {
            const Peak& top = peaks[0];
            const float b = top.bearing * pi / 180;
            const Vector3 guess{top.radius * std::cos(b), 0, top.radius * std::sin(b)};
            float best = -1e9F;
            for (float dx = -160; dx <= 160; dx += 4) for (float dz = -160; dz <= 160; dz += 4) {
                const float y = land(guess.x + dx, guess.z + dz);
                if (y > best) {best = y; L.summit = {guess.x + dx, y, guess.z + dz};}
            }
        }
        // The grid.
        float x0 = 1e9F, z0 = 1e9F, x1 = -1e9F, z1 = -1e9F;
        const auto grow = [&](Vector3 p, float r) {x0 = std::min(x0, p.x - r); z0 = std::min(z0, p.z - r); x1 = std::max(x1, p.x + r); z1 = std::max(z1, p.z + r);};
        for (const auto& p : L.line.points) grow(p, reach_max + Plan::cell);
        grow(L.hamlet.centre, hamlet_flat + hamlet_blend + Plan::cell);
        L.x0 = x0; L.z0 = z0;
        L.cols = static_cast<int>(std::ceil((x1 - x0) / Plan::cell)) + 1;
        L.rows = static_cast<int>(std::ceil((z1 - z0) / Plan::cell)) + 1;
        L.cells.assign(static_cast<std::size_t>(L.cols * L.rows), {});
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const Vector3 a = L.line.points[i], b = L.line.points[i + 1];
            const float r = reach_max;
            const int cx0 = static_cast<int>(std::floor((std::min(a.x, b.x) - r - x0) / Plan::cell));
            const int cx1 = static_cast<int>(std::floor((std::max(a.x, b.x) + r - x0) / Plan::cell));
            const int cz0 = static_cast<int>(std::floor((std::min(a.z, b.z) - r - z0) / Plan::cell));
            const int cz1 = static_cast<int>(std::floor((std::max(a.z, b.z) + r - z0) / Plan::cell));
            for (int cz = std::max(0, cz0); cz <= std::min(L.rows - 1, cz1); ++cz)
                for (int cx = std::max(0, cx0); cx <= std::min(L.cols - 1, cx1); ++cx)
                    L.cells[static_cast<std::size_t>(cz * L.cols + cx)].push_back(static_cast<int>(i));
        }
        return L;
    }();
    return built;
}

float profile_at(const Plan& L, float metres) {
    const auto& s = L.line.s;
    if (metres <= 0) return L.line.points.front().y;
    if (metres >= s.back()) return L.line.points.back().y;
    const std::size_t i = std::min(static_cast<std::size_t>(metres / step), s.size() - 2);
    const float t = std::clamp((metres - s[i]) / std::max(1e-4F, s[i + 1] - s[i]), 0.0F, 1.0F);
    return L.line.points[i].y + (L.line.points[i + 1].y - L.line.points[i].y) * t;
}
float band_at(const Plan& L, float metres) {
    const std::size_t i = std::min(static_cast<std::size_t>(std::max(0.0F, metres) / step), L.band.size() - 1);
    return L.band[i];
}
// The roadbed's half-width at `metres` on `side` (wider into a pullout's bench).
float bed_at(const Plan& L, float metres, float side) {
    float wide = 0;
    for (const auto& p : L.pullouts) {
        if (p.side != side) continue;
        const float off = std::abs(metres - p.s);
        if (off < pullout_core + 10) wide = std::max(wide, pullout_bench * (1 - smooth((off - pullout_core) / 10)));
    }
    return bed_half + wide;
}
float weight(float d, float bed, float band) {
    if (d <= bed) return 1;
    return 1 - smooth((d - bed) / band);
}
float distance_to_segment(Vector3 p, Vector3 a, Vector3 b, float& t) {
    const float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
    t = l2 > 1e-8F ? std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / l2, 0.0F, 1.0F) : 0;
    return std::hypot(p.x - a.x - dx * t, p.z - a.z - dz * t);
}
} // namespace

Vector3 Line::at(float metres) const {
    if (points.empty()) return {};
    if (metres <= 0) return points.front();
    if (metres >= s.back()) return points.back();
    const std::size_t i = std::min(static_cast<std::size_t>(std::upper_bound(s.begin(), s.end(), metres) - s.begin()) - 1, s.size() - 2);
    const float t = (metres - s[i]) / std::max(1e-4F, s[i + 1] - s[i]);
    return {points[i].x + (points[i + 1].x - points[i].x) * t, points[i].y + (points[i + 1].y - points[i].y) * t,
            points[i].z + (points[i + 1].z - points[i].z) * t};
}
Vector3 Line::heading(float metres) const {
    const Vector3 a = at(metres - step), b = at(metres + step);
    const float l = std::max(1e-4F, std::hypot(b.x - a.x, b.z - a.z));
    return {(b.x - a.x) / l, 0, (b.z - a.z) / l};
}
Vector3 Pullout::at(float a, float lateral) const {
    return {centre.x + along.x * a + out.x * lateral, centre.y, centre.z + along.z * a + out.z * lateral};
}
Vector3 Hamlet::at(float a, float lateral) const {
    const Vector3 left{-lane.z, 0, lane.x};
    return {centre.x + lane.x * a + left.x * lateral, centre.y, centre.z + lane.z * a + left.z * lateral};
}

const Line& road() { return plan().line; }
const std::vector<Pullout>& pullouts() { return plan().pullouts; }
const Hamlet& hamlet() { return plan().hamlet; }
Vector3 summit() { return plan().summit; }

float massif(float x, float z) {
    const float r = std::hypot(x, z);
    if (r < 1560) return 0;
    const float bearing = std::atan2(z, x) * 180 / pi;
    if (std::abs(std::remainder(bearing - 215, 360.0F)) > 50) return 0;
    float h = 0;
    for (const auto& p : peaks) {
        const float along = std::remainder(bearing - p.bearing, 360.0F) * pi / 180 * p.radius, across = r - p.radius;
        h += p.height * std::exp(-(along / p.along) * (along / p.along) - (across / p.across) * (across / p.across));
    }
    if (h < .01F) return 0;
    h = h * (1 + .07F * pseudo(x * 1.3F + 33, z * 1.3F - 71)) + std::min(h, 60.0F) / 60 * (3 * ridge(x * 3.1F, z * 3.1F) + 1.5F * pseudo(x * 6.7F, z * 6.7F));
    // Down to the land at the ring road's verge, and clear of Sovaĝejo's and La Ringo's pads.
    const auto& c = layout::sovagejo_center;
    static const Vector3 ringo = highways::brotherhood_home().centre;
    const float mask = smooth((r - 1560) / 130) * smooth((std::hypot(x - c.x, z - c.z) - 150) / 200) *
                       smooth((std::hypot(x - ringo.x, z - ringo.z) - 110) / 130);
    return h * mask;
}

float graded(float x, float z, float height) {
    const Plan& L = plan();
    const int cx = static_cast<int>(std::floor((x - L.x0) / Plan::cell)), cz = static_cast<int>(std::floor((z - L.z0) / Plan::cell));
    if (cx < 0 || cz < 0 || cx >= L.cols || cz >= L.rows) return height;
    const Vector3 p{x, 0, z};
    const auto& pts = L.line.points;
    float num = height, den = 1;
    const auto add = [&](float w, float y) {
        if (w <= 0) return;
        const float omega = w / (1 - w + 1e-5F);
        num += omega * y; den += omega;
    };
    // The road: its nearest stretch, and the nearest of any other leg (a hairpin's other side).
    const auto& list = L.cells[static_cast<std::size_t>(cz * L.cols + cx)];
    if (!list.empty()) {
        struct Near { float d{1e9F}, s{0}, side{1}; };
        Near best;
        const auto measure = [&](int i, Near& n) {
            float t = 0;
            const Vector3 a = pts[static_cast<std::size_t>(i)], b = pts[static_cast<std::size_t>(i) + 1];
            const float d = distance_to_segment(p, a, b, t);
            n.d = d;
            n.s = L.line.s[static_cast<std::size_t>(i)] + t * (L.line.s[static_cast<std::size_t>(i) + 1] - L.line.s[static_cast<std::size_t>(i)]);
            n.side = (b.x - a.x) * (z - a.z) - (b.z - a.z) * (x - a.x) >= 0 ? 1.0F : -1.0F;   // (+1: to the left, as Pullout::side)
        };
        for (const int i : list) {
            Near n;
            measure(i, n);
            if (n.d < best.d) best = n;
        }
        Near before, after;
        for (const int i : list) {
            const float s0 = L.line.s[static_cast<std::size_t>(i)], s1 = L.line.s[static_cast<std::size_t>(i) + 1];
            if (s1 > best.s - leg_apart && s0 < best.s + leg_apart) continue;
            Near n;
            measure(i, n);
            if (n.s < best.s - leg_apart) {if (n.d < before.d) before = n;}
            else if (n.s > best.s + leg_apart && n.d < after.d) after = n;
        }
        for (const Near* n : {&best, &before, &after})
            if (n->d < reach_max) add(weight(n->d, bed_at(L, n->s, n->side), band_at(L, n->s)), profile_at(L, n->s));
    }
    // Supra Sovaĝejo: the lane from the fork and the pad.
    const Hamlet& m = L.hamlet;
    float t = 0;
    const float lane = distance_to_segment(p, m.fork, m.centre, t);
    if (lane < 4 + 14) add(weight(lane, 4, 14), m.fork.y);
    const float pad = flat(p, m.centre);
    if (pad < hamlet_flat + hamlet_blend) add(weight(pad, hamlet_flat, hamlet_blend), m.centre.y);
    return num / den;
}

bool near(float x, float z, float margin) {
    const Plan& L = plan();
    const Vector3 p{x, 0, z};
    if (flat(p, L.hamlet.centre) < hamlet_flat + margin) return true;
    float t = 0;
    if (distance_to_segment(p, L.hamlet.fork, L.hamlet.centre, t) < lane_width * .5F + margin) return true;
    const auto& pts = L.line.points;
    // (The grid lists every stretch within reach_max of a cell; further than that, scan them all.)
    if (bed_half + margin <= reach_max) {
        const int cx = static_cast<int>(std::floor((x - L.x0) / Plan::cell)), cz = static_cast<int>(std::floor((z - L.z0) / Plan::cell));
        if (cx < 0 || cz < 0 || cx >= L.cols || cz >= L.rows) return false;
        for (const int i : L.cells[static_cast<std::size_t>(cz * L.cols + cx)])
            if (distance_to_segment(p, pts[static_cast<std::size_t>(i)], pts[static_cast<std::size_t>(i) + 1], t) < bed_half + margin) return true;
        return false;
    }
    for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        if (distance_to_segment(p, pts[i], pts[i + 1], t) < bed_half + margin) return true;
    return false;
}

} // namespace outland::world::mountain
