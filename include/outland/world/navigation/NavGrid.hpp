#pragma once
#include <raylib.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace outland::world { class VerdaRegion; }

// Walkable-space graph for everything that moves on foot: bots, NPCs, residents, later police and
// zombies. Verda is too large to grid up front, so the graph is a lazily evaluated 2.5D grid:
// every 0.5 m cell knows the surfaces a body can stand on there (terrain, building floors, stairs,
// roofs), computed from WorldCollision the first time a search touches it and cached afterwards.
// Two cells connect when both are standable and their surfaces are within a step of each other,
// so doorways, stairwells and upper floors are found from the real geometry.
namespace outland::world::navigation {

struct NavSettings {
    float cell{.5F};        // metres; small enough that a 1 m doorway always has a cell centre in it
    float radius{.3F};      // body clearance used to decide a cell is standable
    float step{.5F};        // matches WorldCollision's step height
    float max_height{40};   // highest surface considered above the terrain
    int max_expansions{40000};
    // New cells one search may evaluate (0 = unlimited). Cells are cached, so a search that hits
    // the budget returns a partial path and the next search continues from where it stopped:
    // the cost of exploring new ground is spread over frames instead of stalling one.
    int max_new_cells{0};
};

struct NavPath {
    std::vector<Vector3> points; // feet positions from start to goal (or toward it), smoothed
    bool complete{false};        // false: the goal was unreachable or too far; points lead toward it
    bool budget_hit{false};      // stopped early to stay within max_new_cells; ask again soon
    int expansions{0};
};

class NavGrid {
public:
    // `region` must outlive the grid. Call clear() after the world changes (Creator edits).
    explicit NavGrid(const VerdaRegion& region, NavSettings settings = {});
    void clear();

    // Shortest walkable route between two feet positions. Returns false only when no progress is
    // possible at all; a partial path (complete=false) ends at the reachable cell nearest the goal.
    bool find_path(Vector3 from, Vector3 to, NavPath& path);
    // Is a body standing at `feet` on a walkable surface (within a step)?
    bool walkable(Vector3 feet);
    // The standable surface nearest `feet` within `search` metres, if any.
    bool nearest_walkable(Vector3 feet, float search, Vector3& out);

    const NavSettings& settings() const { return settings_; }
    void set_max_new_cells(int cells) { settings_.max_new_cells = cells > 0 ? cells : 0; }
    void set_max_expansions(int expansions) { settings_.max_expansions = expansions > 100 ? expansions : 100; }
    std::size_t evaluated_cells() const { return evaluated_; }
    std::size_t cached_tiles() const { return tiles_.size(); }

    static constexpr int max_levels = 4;
    static constexpr float goal_snap = 6;   // metres searched for standing room near a blocked goal
    static constexpr int tile_cells = 32;

private:
    struct Cell { std::uint8_t evaluated{0}, count{0}; std::array<float, max_levels> y{}; };
    struct Tile { std::array<Cell, tile_cells * tile_cells> cells{}; };
    const Cell& cell(int x, int z);
    void evaluate(int x, int z, Cell& out);
    int level_near(int x, int z, float y);      // level within a step of y, or -1
    bool clear_line(Vector3 a, Vector3 b);      // straight walk stays on connected cells
    Vector3 centre(int x, int z, float y) const;
    void build_index();
    bool open_ground(float x, float z) const;   // nothing collidable near: terrain only

    // Coarse occupancy of buildings and assets (8 m buckets), so open countryside skips the
    // collision queries entirely. Built lazily after clear(); moving vehicles are not tracked.
    std::unordered_set<std::uint64_t> occupied_;
    bool indexed_{false};

    const VerdaRegion& region_;
    NavSettings settings_;
    std::unordered_map<std::uint64_t, std::unique_ptr<Tile>> tiles_;
    std::size_t evaluated_{0};
};

// Shared path following for anything that walks. Routing is on demand: a goal on about the same
// level is walked straight (open ground needs no search), and a route is planned when the goal is
// on another floor or the mover reports it is stuck (repath_soon). Plans again when the goal
// moves or the route runs out, and hands back the point to steer toward this frame.
using PathFinder = std::function<bool(Vector3 from, Vector3 to, NavPath& path)>;
class PathFollower {
public:
    // Where to walk now to reach `goal`. Without a finder (or when planning fails) it is the goal.
    Vector3 steer(Vector3 feet, Vector3 goal, double now, const PathFinder& finder);
    void reset() { path_.points.clear(); next_ = 0; planned_ = false; routing_ = false; }
    // The straight walk is blocked: route from here (and keep routing to this goal).
    void repath_soon() { planned_at_ = -1e9; routing_ = true; }
    bool routing() const { return routing_; }
    const NavPath& path() const { return path_; }
    std::size_t next() const { return next_; }
    bool following() const { return planned_ && next_ < path_.points.size(); }

    static constexpr float arrive = .45F;       // waypoint reached within this (xz)
    static constexpr float goal_moved = 1.5F;   // re-plan when the goal moves further than this
    static constexpr double replan_interval = .75; // never re-plan more often than this
    static constexpr double continue_interval = .2; // ...unless the last search ran out of budget
    static constexpr float direct_range = 2.5F;   // closer than this, just walk straight
    static constexpr float lookahead = .6F;       // pure-pursuit distance along the current segment
    static constexpr float other_floor = 1.5F;    // goals this far above/below always need a route

private:
    NavPath path_;
    std::size_t next_{0};
    Vector3 goal_{};
    Vector3 direct_goal_{};
    double planned_at_{-1e9};
    bool planned_{false}, routing_{false};
};
}
