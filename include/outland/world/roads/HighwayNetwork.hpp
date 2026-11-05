#pragma once
#include <raylib.h>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace outland::world { class VerdaRegion; }

// The highways (RoadType::Highway) among a region's roads, worked out once for drawing and
// collision: how consecutive pieces join (mitred, so curves are seamless), where junctions are
// (plain aprons where highways cross or end; median openings where side roads join, and a
// crossover every few hundred metres) and where the Jersey barrier runs down the median. The last
// stretch into a town tapers to a street's width.
namespace outland::world::highways {

struct Span { float from, to; };   // metres along a road from its start
// A guardrail beside a road: steel on posts, `offset` metres out to the right (negative: the left),
// over [from, to] metres along it - where the land falls away beside the roadbed (an embankment).
struct Rail { float offset, from, to; };

// The paved apron in front of a drive-in (a filling station's forecourt, a workshop's bays, the
// car lot): from the front edge of its lot (`lot_left`, `lot_right`) out to the road's edge in
// front of it (`road_left`, `road_right`), so the way in is never across grass. `lot_y`: the lot's
// level; `lot_depth`: front to back (a concrete skirt round it meets the land).
struct Apron { Vector3 lot_left, lot_right, road_left, road_right; float lot_y{0}, lot_depth{0}; };

struct RoadInfo {
    bool highway{false};
    Vector3 start_cross{}, end_cross{}; // unit, to the road's right at each end (mitred where the highway runs on)
    std::vector<Span> plain;            // junction aprons: plain asphalt, no lines, no barrier
    std::vector<Span> barrier;          // where the median barrier stands
    float start_half{0}, end_half{0};   // paved half-width at each end (narrower where it tapers into a town)
    std::vector<Rail> rails;            // (also kept for slip roads, which are not highways)
};

class Network {
public:
    void build(const VerdaRegion& region);
    [[nodiscard]] const RoadInfo* info(std::size_t settlement, std::size_t road) const;
    // The guardrails beside a highway or a slip road (none for any other road).
    [[nodiscard]] const std::vector<Rail>& rails(std::size_t settlement, std::size_t road) const;
    [[nodiscard]] std::size_t rail_count() const { return rail_count_; }
    [[nodiscard]] const std::vector<Apron>& aprons() const { return aprons_; }
    [[nodiscard]] bool on_apron(float x, float z) const;
    // Would a vehicle (a circle of `radius` at p, its wheels at p.y) touch the median barrier, a
    // bridge's parapet, or a guardrail?
    [[nodiscard]] bool barrier_hit(Vector3 p, float radius) const;
    // Would a person (a circle of `radius` at p, feet at p.y) walk into a wall: an interchange's
    // abutments, piers and retaining walls (not the median barrier or a guardrail: people climb
    // over those)?
    [[nodiscard]] bool wall_hit(Vector3 p, float radius) const;
    [[nodiscard]] std::size_t barrier_runs() const { return runs_.size(); }
    [[nodiscard]] std::size_t junctions() const { return junctions_; }

private:
    // A run of barrier. Over and under an interchange's bridge a run counts only at its own level
    // (`level`: the roadway it stands on): a car on the ring's deck passes over the spoke's.
    struct Run { Vector2 a, b; float y{0}; bool level{false}; bool wall{false}; };
    [[nodiscard]] bool hit(Vector3 p, float radius, bool walls) const;
    void add_run(Run run);
    void islands();
    void guardrails(const VerdaRegion& region);
    void drive_ins(const VerdaRegion& region);
    std::vector<Apron> aprons_;
    std::vector<std::vector<RoadInfo>> roads_;
    std::vector<Run> runs_;
    std::unordered_map<long long, std::vector<int>> grid_;
    std::size_t junctions_{0}, rail_count_{0};
};

// The region's network, rebuilt when its roads change (count or storage).
const Network& network(const VerdaRegion& region);

// The point and offset to its right `cross` direction for a highway road at `distance` metres:
// the right vector blends from the start mitre to the end mitre and is scaled so that an offset
// of o metres is o metres square to the road.
Vector3 cross_at(const RoadInfo& info, Vector3 direction, float distance, float length);

}
