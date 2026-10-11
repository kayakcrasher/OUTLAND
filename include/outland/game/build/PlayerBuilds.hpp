#pragma once
#include <raylib.h>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; struct Building; }

// What a player builds in Explore: roads, houses, bus stops, lamps and trees, laid into the
// living island. Player homes stand empty until a bus brings newcomers to one of the player's
// stops; then a family moves in (game::life::LifeSimulation::move_in). Saved per profile.
namespace outland::game::build {

enum class Piece : unsigned char { Road, Cottage, House, BusStop, Lamp, Tree };
inline constexpr int piece_count = 6;

struct PieceInfo {
    const char* name;     // "COTTAGE"
    const char* hint;     // one line for the build bar
    Vector3 size;         // footprint (x across, z deep) and height
    int beds;             // homes: how many can live there
};
const PieceInfo& piece_info(Piece piece);

struct Placed {
    Piece piece{Piece::Cottage};
    int serial{0};          // unique per save: ids are player_<kind>_<serial>
    Vector3 position{};     // centre on the ground (a road: its start)
    Vector3 end{};          // a road's end
    float yaw{0};           // degrees; a house's front (door) faces local -z
    int residents{0};       // homes: how many have moved in (0 = empty)
    std::string id() const;
    bool home() const { return piece == Piece::Cottage || piece == Piece::House; }
};

class PlayerBuilds {
public:
    const std::vector<Placed>& placed() const { return placed_; }
    // Why `candidate` cannot go where it is, or "" when it can. Houses need clear, gently sloping,
    // dry ground off the roads; a bus stop needs a road beside it and no other stop within 60 m.
    std::string check(const world::VerdaRegion& region, const Placed& candidate) const;
    // A bus stop snaps to the road beside `near`: the shelter on the verge, facing the road.
    // False when there is no road within 14 m.
    static bool snap_bus_stop(const world::VerdaRegion& region, Vector3 near, Vector3& shelter, float& yaw);
    Placed& add(Placed piece);
    // Takes away the most recent piece within `radius` of `near` (any piece when radius <= 0).
    // A home people have moved into stays.
    enum class Removal { Nothing, Removed, Occupied };
    Removal remove(Vector3 near, float radius);
    // Writes every piece into the region's towns (each into the nearest), replacing what an
    // earlier apply() put there. Call after loading, and after each change.
    void apply(world::VerdaRegion& region);
    // Player homes nearest `stop` first, empty ones only, within `radius`.
    int vacant_home_near(Vector3 stop, float radius) const;
    Placed* find(const std::string& id);
    world::Building building(const Placed& home) const;
    int empty_homes() const;

    bool save(const std::string& path, std::string& error) const;
    bool load(const std::string& path, std::string& error);
    void clear() { placed_.clear(); next_serial_ = 1; }
    // Next to the Explore loot profile: <dir>/explore.builds
    static std::string path_beside(const std::string& loot_profile);
private:
    std::vector<Placed> placed_;
    int next_serial_{1};
    struct AppliedRoad { Vector3 start, end; };
    std::vector<AppliedRoad> applied_roads_;
};

} // namespace outland::game::build
