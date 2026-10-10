#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace outland::world::roads {
namespace {
constexpr float merge = 1.5F; // metres: ends this close to a road join it
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
// Where the right-hand lane runs: a quarter of the way across a two-lane road; wide streets keep
// a parking lane at the kerb, so traffic runs nearer the middle.
float lane_of(float width) { return width >= 10 ? width * .15F : width * .25F; }
Vector3 flat_dir(Vector3 a, Vector3 b) {
    const float l = flat(a, b);
    return l < 1e-4F ? Vector3{0, 0, 1} : Vector3{(b.x - a.x) / l, 0, (b.z - a.z) / l};
}
// Parameter of the point on segment ab nearest p (unclamped) and the distance to the clamped point.
float project(Vector3 p, Vector3 a, Vector3 b, float& distance) {
    const float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
    const float t = l2 < 1e-8F ? 0 : ((p.x - a.x) * dx + (p.z - a.z) * dz) / l2;
    const float c = std::clamp(t, 0.0F, 1.0F);
    distance = std::hypot(p.x - (a.x + dx * c), p.z - (a.z + dz * c));
    return t;
}
}

int RoadGraph::node(Vector3 p) {
    for (std::size_t i = 0; i < nodes_.size(); ++i) if (flat(nodes_[i], p) < merge) return static_cast<int>(i);
    nodes_.push_back({p.x, 0, p.z});
    adjacent_.emplace_back();
    return static_cast<int>(nodes_.size() - 1);
}

void RoadGraph::build(const VerdaRegion& region) {
    nodes_.clear(); edges_.clear(); adjacent_.clear();
    struct Segment { Vector3 a, b; float width; std::vector<float> cuts; };
    std::vector<Segment> segments;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads)
        if (flat(road.start, road.end) > .5F && std::isfinite(road.start.x) && std::isfinite(road.end.x))
            segments.push_back({road.start, road.end, std::max(2.0F, road.width), {0, 1}});
    for (std::size_t i = 0; i < segments.size(); ++i) for (std::size_t j = 0; j < segments.size(); ++j) {
        if (i == j) continue;
        auto& s = segments[i];
        const auto& o = segments[j];
        // The other road's ends stop on this one: a T-junction, or overlapping segments.
        for (const Vector3 end : {o.a, o.b}) {
            float d = 0;
            const float t = project(end, s.a, s.b, d);
            if (d < merge && t > 0 && t < 1) s.cuts.push_back(t);
        }
        // A proper crossing.
        const float rx = s.b.x - s.a.x, rz = s.b.z - s.a.z, qx = o.b.x - o.a.x, qz = o.b.z - o.a.z;
        const float denominator = rx * qz - rz * qx;
        if (std::abs(denominator) < 1e-6F) continue;
        const float t = ((o.a.x - s.a.x) * qz - (o.a.z - s.a.z) * qx) / denominator;
        const float u = ((o.a.x - s.a.x) * rz - (o.a.z - s.a.z) * rx) / denominator;
        if (t > 0 && t < 1 && u >= 0 && u <= 1) s.cuts.push_back(t);
    }
    for (auto& s : segments) {
        std::sort(s.cuts.begin(), s.cuts.end());
        int previous = -1;
        for (const float t : s.cuts) {
            const int n = node(Vector3Lerp(s.a, s.b, t));
            if (previous >= 0 && n != previous) {
                const bool known = std::any_of(adjacent_[static_cast<std::size_t>(n)].begin(), adjacent_[static_cast<std::size_t>(n)].end(),
                    [&](int e) {return edges_[static_cast<std::size_t>(e)].a == previous || edges_[static_cast<std::size_t>(e)].b == previous;});
                if (!known) {
                    edges_.push_back({previous, n, flat(nodes_[static_cast<std::size_t>(previous)], nodes_[static_cast<std::size_t>(n)]), s.width});
                    adjacent_[static_cast<std::size_t>(previous)].push_back(static_cast<int>(edges_.size() - 1));
                    adjacent_[static_cast<std::size_t>(n)].push_back(static_cast<int>(edges_.size() - 1));
                }
            }
            previous = n;
        }
    }
    // A road that stops just short of another (a lane laid to the end of a street, not onto it)
    // still joins it: dead ends within 15 m of another node get a short link.
    const auto link = [&](int a, int b) {
        edges_.push_back({a, b, flat(nodes_[static_cast<std::size_t>(a)], nodes_[static_cast<std::size_t>(b)]),
                          edges_[static_cast<std::size_t>(adjacent_[static_cast<std::size_t>(a)].front())].width});
        adjacent_[static_cast<std::size_t>(a)].push_back(static_cast<int>(edges_.size() - 1));
        adjacent_[static_cast<std::size_t>(b)].push_back(static_cast<int>(edges_.size() - 1));
    };
    for (std::size_t n = 0; n < nodes_.size(); ++n) {
        if (adjacent_[n].size() != 1) continue;
        const auto& own = edges_[static_cast<std::size_t>(adjacent_[n].front())];
        const int neighbour = own.a == static_cast<int>(n) ? own.b : own.a;
        int best = -1;
        float best_distance = 15;
        for (std::size_t m = 0; m < nodes_.size(); ++m) {
            if (m == n || static_cast<int>(m) == neighbour) continue;
            const float d = flat(nodes_[n], nodes_[m]);
            if (d < best_distance) {best_distance = d; best = static_cast<int>(m);}
        }
        if (best >= 0) link(static_cast<int>(n), best);
    }
}

RoadSpot RoadGraph::nearest(Vector3 point) const {
    RoadSpot best;
    best.distance = std::numeric_limits<float>::max();
    for (std::size_t e = 0; e < edges_.size(); ++e) {
        const auto& edge = edges_[e];
        const Vector3 a = nodes_[static_cast<std::size_t>(edge.a)], b = nodes_[static_cast<std::size_t>(edge.b)];
        float d = 0;
        const float t = std::clamp(project(point, a, b, d), 0.0F, 1.0F);
        if (d < best.distance) best = {Vector3Lerp(a, b, t), flat_dir(a, b), edge.width, d, static_cast<int>(e)};
    }
    return best;
}

Route RoadGraph::route(Vector3 from, Vector3 to) const {
    Route route;
    const auto add = [&](Vector3 p, float lane) {
        p.y = 0;
        if (!route.points.empty() && flat(route.points.back(), p) < .5F) return;
        if (!route.points.empty()) {route.lane.push_back(lane); route.length += flat(route.points.back(), p);}
        route.points.push_back(p);
    };
    if (edges_.empty()) {add(from, 0); add(to, 0); route.complete = true; return route;}
    const auto start = nearest(from), goal = nearest(to);
    const auto& se = edges_[static_cast<std::size_t>(start.edge)];
    const auto& ge = edges_[static_cast<std::size_t>(goal.edge)];
    add(from, 0);
    if (start.edge == goal.edge) {
        add(start.point, 0); add(goal.point, lane_of(se.width)); add(to, 0);
        route.complete = true;
        return route;
    }
    std::vector<float> cost(nodes_.size(), std::numeric_limits<float>::max());
    std::vector<int> via(nodes_.size(), -1);
    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    for (const int n : {se.a, se.b}) {
        const float c = flat(start.point, nodes_[static_cast<std::size_t>(n)]);
        if (c < cost[static_cast<std::size_t>(n)]) {cost[static_cast<std::size_t>(n)] = c; open.emplace(c, n);}
    }
    while (!open.empty()) {
        const auto [c, n] = open.top(); open.pop();
        if (c > cost[static_cast<std::size_t>(n)]) continue;
        for (const int e : adjacent_[static_cast<std::size_t>(n)]) {
            const auto& edge = edges_[static_cast<std::size_t>(e)];
            const int m = edge.a == n ? edge.b : edge.a;
            if (c + edge.length < cost[static_cast<std::size_t>(m)]) {
                cost[static_cast<std::size_t>(m)] = c + edge.length; via[static_cast<std::size_t>(m)] = n;
                open.emplace(cost[static_cast<std::size_t>(m)], m);
            }
        }
    }
    int last = -1;
    float best = std::numeric_limits<float>::max();
    for (const int n : {ge.a, ge.b}) {
        if (cost[static_cast<std::size_t>(n)] == std::numeric_limits<float>::max()) continue;
        const float c = cost[static_cast<std::size_t>(n)] + flat(nodes_[static_cast<std::size_t>(n)], goal.point);
        if (c < best) {best = c; last = n;}
    }
    if (last < 0) {add(start.point, 0); add(to, 0); return route;} // islands of road: nothing joins them
    std::vector<int> chain;
    for (int n = last; n >= 0; n = via[static_cast<std::size_t>(n)]) chain.push_back(n);
    std::reverse(chain.begin(), chain.end());
    add(start.point, 0);
    float width = se.width;
    for (std::size_t k = 0; k < chain.size(); ++k) {
        if (k > 0) for (const int e : adjacent_[static_cast<std::size_t>(chain[k])]) {
            const auto& edge = edges_[static_cast<std::size_t>(e)];
            if (edge.a == chain[k - 1] || edge.b == chain[k - 1]) width = edge.width;
        }
        add(nodes_[static_cast<std::size_t>(chain[k])], lane_of(width));
    }
    add(goal.point, lane_of(ge.width));
    add(to, 0);
    route.complete = true;
    return route;
}

Vector3 route_point(const Route& route, float distance, float* yaw_degrees, bool keep_lane) {
    if (route.points.empty()) return {};
    if (route.points.size() == 1) return route.points.front();
    distance = std::clamp(distance, 0.0F, route.length);
    float walked = 0;
    for (std::size_t i = 0; i + 1 < route.points.size(); ++i) {
        const Vector3 a = route.points[i], b = route.points[i + 1];
        const float l = flat(a, b);
        if (walked + l >= distance || i + 2 == route.points.size()) {
            const Vector3 d = flat_dir(a, b);
            if (yaw_degrees) *yaw_degrees = std::atan2(d.x, d.z) * RAD2DEG;
            const float t = l < 1e-4F ? 1 : std::clamp((distance - walked) / l, 0.0F, 1.0F);
            const float lane = keep_lane && i < route.lane.size() ? route.lane[i] : 0;
            const Vector3 p = Vector3Lerp(a, b, t);
            return {p.x - d.z * lane, 0, p.z + d.x * lane};
        }
        walked += l;
    }
    return route.points.back();
}

float route_progress(const Route& route, Vector3 p, float hint) {
    float walked = 0, best = std::numeric_limits<float>::max(), at = hint;
    for (std::size_t i = 0; i + 1 < route.points.size(); ++i) {
        const Vector3 a = route.points[i], b = route.points[i + 1];
        const float l = flat(a, b);
        if (walked + l >= hint - 15 && walked <= hint + 80) {
            float d = 0;
            const float t = std::clamp(project(p, a, b, d), 0.0F, 1.0F);
            if (d < best) {best = d; at = walked + t * l;}
        }
        walked += l;
    }
    return at;
}
}
