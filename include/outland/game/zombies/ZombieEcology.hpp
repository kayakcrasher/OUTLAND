#pragma once
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::characters { class NpcSystem; class CharacterRegistry; }
namespace outland::game::sound { class SoundBus; }

// Zombie Survival's infected, island-wide. Two tiers like island life: every zombie is a cheap
// abstract record (where it is, what it is doing); the ones near the player become NpcSystem
// creature bodies that see, chase and bite with the existing creature behaviour, and are steered
// by this ecology when they are not hunting.
//
// - Sources: authored Zombie Spawn markers, nests round every town, roadside nests between towns.
// - Day: many lie dormant, the rest shamble near their nest. Night: all awake, faster, hearing
//   further and seeing less; hordes form and walk the roads between towns (sometimes toward the
//   player's town) and break up at dawn.
// - Noise (SoundBus): gunshots, sprinting, engines and horns send whoever hears them to look;
//   a gunshot can turn a horde.
// - The dead stay down; a nest sends a replacement after a while, never in front of the player.
namespace outland::game::zombies {

struct ZombieConfig {
    int population{180};
    int max_bodies{14};
    float materialize_radius{85};
    float dematerialize_radius{120};
    float respawn_seconds{90};
    float respawn_clearance{90};     // a nest refills only when the player is further than this
    int max_hordes{2};
    float horde_interval{25};        // seconds between attempts to start a horde at night
    float horde_speed{1.3F};         // m/s along the roads
    float wander_speed{.5F}, night_wander_speed{.9F};
    float investigate_speed{1.4F}, night_investigate_speed{2.4F};
    float night_hearing{1.4F};       // sound radius multiplier at night
    float day{.5F};                  // daylight above this is day
    float night{.3F};                // daylight below this is night
    int capital_nests{8};            // nests round Verda City (the outbreak's ground zero has more)
    int town_nests{4};               // ...round every other town (more if the town is marked infected)
};

enum class ZombieState : std::uint8_t { Dormant, Wander, Investigate, Horde, Dead };
const char* zombie_state_name(ZombieState state);

struct ZombieSource { Vector3 position{}; int settlement{-1}; float radius{12}; bool authored{false}; bool roadside{false}; bool story{false}; };

struct Zombie {
    int id{0};
    int source{0};
    int horde{-1};
    ZombieState state{ZombieState::Wander};
    Vector3 position{}, target{};
    double think_at{0}, until{0}, died_at{-1e9};
    bool physical{false};
    std::string character_id;
    Vector3 offset{};                // place in the horde
};

struct Horde {
    int id{0};
    std::vector<int> members;
    world::roads::Route route;
    float progress{0};
    Vector3 position{};
    int destination{-1};             // settlement, or -1 when chasing a noise
    bool turned{false};              // diverted by a gunshot
};

struct ZombieFrame {
    Vector3 player{};
    float daylight{1};
    double now{0};
};

struct ZombieStats { int alive{0}, physical{0}, dormant{0}, investigating{0}, in_hordes{0}, hordes{0}, killed{0}, near{0}; };

class ZombieEcology {
public:
    static constexpr int body_id_base = 1000000; // NpcSystem resident ids used for zombie bodies

    void configure(ZombieConfig config) { config_ = config; }
    const ZombieConfig& config() const { return config_; }
    // Sources and the starting population (deterministic for a region and seed).
    void build(const world::VerdaRegion& region, const characters::CharacterRegistry* registry, std::uint64_t seed);
    void reset(characters::NpcSystem& npcs);
    // A safe start: infected within `radius` of where the player begins move to other nests.
    void clear_area(Vector3 around, float radius);
    void clear(characters::NpcSystem& npcs);
    bool empty() const { return zombies_.empty(); }
    // One of the story's infected, here and now (a rancher getting back up): awake, never
    // replaced once dead. Returns its id.
    int spawn_at(Vector3 at, const std::string& character = {});
    [[nodiscard]] bool dead(int id) const;
    // Places the living hold (a hideout, a holdout's walls): none of the dead are sent there - the
    // nests inside move out, and nothing is replaced inside.
    void set_safe(std::vector<std::pair<Vector3, float>> zones);
    [[nodiscard]] bool safe(Vector3 at) const;

    void hear(const sound::SoundBus& sounds);
    void update(float dt, const ZombieFrame& frame, characters::NpcSystem& npcs);

    const std::vector<ZombieSource>& sources() const { return sources_; }
    const std::vector<Zombie>& zombies() const { return zombies_; }
    const std::vector<Horde>& hordes() const { return hordes_; }
    ZombieStats stats() const;
    // A body id belongs to the ecology (for damage and kill attribution).
    static bool is_zombie(int resident) { return resident >= body_id_base; }

private:
    float random();
    float speed(ZombieState state) const;
    bool day() const { return daylight_ > config_.day; }
    bool night() const { return daylight_ < config_.night; }
    void wander_target(Zombie& zombie);
    void step_abstract(Zombie& zombie, float dt);
    void manage_hordes(float dt);
    void disband(std::size_t horde);
    void sync_bodies(characters::NpcSystem& npcs);
    void tune(characters::NpcSystem& npcs);

    const world::VerdaRegion* region_{nullptr};
    world::roads::RoadGraph roads_;
    std::vector<Vector3> towns_;
    ZombieConfig config_{};
    std::vector<ZombieSource> sources_;
    std::vector<Zombie> zombies_;
    std::vector<Horde> hordes_;
    std::vector<std::pair<Vector3, float>> safe_;
    std::vector<std::string> looks_;
    std::uint64_t rng_{0x9e3779b97f4a7c15ULL}, seed_{1};
    std::uint64_t heard_up_to_{0};
    double now_{0}, horde_clock_{0};
    float daylight_{1}, bridge_clock_{0};
    int next_horde_{0}, killed_{0}, tuned_{-1};
    Vector3 player_{};
};
}
