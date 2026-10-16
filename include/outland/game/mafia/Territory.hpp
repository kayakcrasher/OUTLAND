#pragma once
#include <raylib.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Who runs Verda (Explore): every town and every ranch is turf, and every turf has an owner and a
// HOLD (0-100) on it. The five gangs start with the towns of their own county (Verda City and
// its county are La Verda Mano's, green); each ranching family holds its own ranch and is a
// faction in its own right.
//
// Hold moves with what happens there: a rival's kills on the owner's crew, a rival's tags on its
// walls, raids from other gangs, all wear it down; the owner's own tags and contraband run home
// shore it up, and it creeps back on its own while nobody contests it. When the owner's hold
// runs out the town goes to whoever pushed hardest, and the crew there turns its colours.
//
// The families: some ride with a gang (allied), some are fiercely independent, one or two will
// deal. A gang can lean on a family for protection money (extortion): the fierce fight first,
// the rest pay. A ranch allied with your gang, or paying it, counts as yours.
//
// The goal for a gangster: every town your gang's, every family allied with you or paying you.
namespace outland::game::mafia {

inline constexpr int gang_count = 5;
inline constexpr int family_count = 5;
inline constexpr int faction_count = gang_count + family_count;
inline constexpr int family_faction(int ranch) { return gang_count + ranch; }
inline constexpr bool is_family(int faction) { return faction >= gang_count; }

enum class Stance : std::uint8_t { Independent, Allied, Extorted };

struct Turf {
    std::string id, name;
    Vector3 centre{};
    float radius{150};
    int ranch{-1};                 // a ranch (its family's index) or -1 for a town
    int crew{-1};                  // the Mafio crew posted here (towns), -1 if none
    int owner{0};                  // faction
    float hold{100};               // the owner's grip, 0..100
    std::array<float, faction_count> pressure{};   // what each challenger has put in since the last flip
};

struct Family {
    std::string name;              // "Garza"
    Stance stance{Stance::Independent};
    int gang{-1};                  // allied with / paying this gang
    bool fierce{false};            // fights before it pays
};

struct Tag {
    Vector3 position{}, normal{};  // on a wall
    int gang{0};
    int turf{-1};
};

struct Flip { int turf{-1}, from{-1}, to{-1}; };
struct Raid { int attacker{-1}, turf{-1}; };

class Territory {
public:
    static constexpr float regain_per_second = 1.0F / 30;   // a point every half minute, uncontested
    static constexpr float kill_wear = 7, tag_wear = 12, own_tag = 6, delivery = 25, raid_wear = 9;
    static constexpr float tag_spacing = 2.5F;               // a tag within this of another paints over it
    static constexpr double raid_every = 150;                 // seconds between AI gangs' pushes

    // Towns (with the gang that holds each: the county's) and ranches (one per family).
    void add_town(const std::string& id, const std::string& name, Vector3 centre, float radius, int gang, int crew);
    void add_ranch(const std::string& id, const std::string& name, Vector3 centre, float radius, int ranch,
                   const std::string& family, int county_gang, std::uint64_t seed);
    void clear() { *this = Territory{}; }

    [[nodiscard]] int turf_at(Vector3 p) const;          // the turf `p` is in (nearest centre in reach), or -1
    [[nodiscard]] const std::vector<Turf>& turfs() const { return turfs_; }
    [[nodiscard]] const std::array<Family, family_count>& families() const { return families_; }
    [[nodiscard]] const std::vector<Tag>& tags() const { return tags_; }

    // What happened. `by` is the gang responsible (-1 nobody's).
    void killed(int victim_faction, Vector3 where, int by);
    // A tag painted at `position` on a wall facing `normal`; it paints over any within tag_spacing.
    // Returns the turf it counted for (-1 outside every turf).
    int tag(Vector3 position, Vector3 normal, int gang);
    void delivered(int turf, int gang);                  // contraband run home to `gang`'s turf
    // Lean on a family for `gang`. True if they pay now; false if they fight first (fierce, or
    // allied with someone else) - beat them (`subdued`) and they pay.
    bool extort(int ranch, int gang);
    void subdued(int ranch, int gang);
    void ally(int ranch, int gang);

    // Time: holds creep back; now and then an AI gang pushes into a neighbour's town (not the
    // player's gang, which plays its own moves). Returns the raid started this tick, if any.
    Raid update(float dt, double now, int player_gang);
    // Turfs that changed hands since last asked.
    std::vector<Flip> take_flips() { auto f = std::move(flips_); flips_.clear(); return f; }

    // Standing: towns held (ranches count for a gang when allied or paying).
    [[nodiscard]] int towns_of(int faction) const;
    [[nodiscard]] int ranches_with(int gang) const;
    [[nodiscard]] bool controls(int turf, int gang) const;   // a town it owns or a ranch with it
    [[nodiscard]] bool rules_island(int gang) const;
    // Gangs and families, biggest first (towns, then ranches).
    [[nodiscard]] std::vector<int> ranking() const;

    [[nodiscard]] std::string serialise() const;
    bool parse(const std::string& text);                 // over a built map (same turfs)

private:
    void wear(Turf& t, int by, float amount);
    void flip(int index, int to);
    float random();

    std::vector<Turf> turfs_;
    std::array<Family, family_count> families_{};
    std::vector<Tag> tags_;
    std::vector<Flip> flips_;
    double next_raid_{-1};
    std::uint64_t rng_{0x6c8e9cf570932bd5ULL};
};

} // namespace outland::game::mafia
