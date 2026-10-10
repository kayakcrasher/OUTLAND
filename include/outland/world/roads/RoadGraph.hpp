#pragma once
#include <raylib.h>
#include <vector>

namespace outland::world { class VerdaRegion; }

// The island's roads as a graph for anything that drives. Built from the map's Road segments:
// crossings and T-junctions (an end that stops on another road, like a side street) are split
// into shared nodes, so downtown's grid, the radials and the ring all connect.
namespace outland::world::roads {

struct RoadSpot {
    Vector3 point{};      // on the road's centre line
    Vector3 direction{};  // unit, along the road (start -> end of its edge)
    float width{0};
    float distance{0};    // from the query point
    int edge{-1};
};

struct Route {
    std::vector<Vector3> points; // from start to goal
    std::vector<float> lane;     // per segment: metres to keep right of the line (0 off-road)
    float length{0};
    bool complete{false};
};

class RoadGraph {
public:
    RoadGraph() = default;
    explicit RoadGraph(const VerdaRegion& region) { build(region); }
    void build(const VerdaRegion& region);
    bool empty() const { return edges_.empty(); }

    // Nearest point on any road centre line.
    RoadSpot nearest(Vector3 point) const;
    // Off-road start, onto the nearest road, along the shortest way, off to the goal. Driving is on
    // the right: road segments carry a lane offset (a quarter of a two-lane road's width; wide
    // downtown streets keep a kerbside parking lane, so 15%).
    Route route(Vector3 from, Vector3 to) const;

    struct Edge { int a, b; float length, width; };
    const std::vector<Vector3>& nodes() const { return nodes_; }
    const std::vector<Edge>& edges() const { return edges_; }

private:
    int node(Vector3 p);
    std::vector<Vector3> nodes_;
    std::vector<Edge> edges_;
    std::vector<std::vector<int>> adjacent_; // edge indices per node
};

// Point and heading `distance` metres along a route, kept `lane` metres to the right.
Vector3 route_point(const Route& route, float distance, float* yaw_degrees = nullptr, bool keep_lane = true);
// Distance along the route of the point nearest `p`, searching from `hint` forward (and a little back).
float route_progress(const Route& route, Vector3 p, float hint);
}
