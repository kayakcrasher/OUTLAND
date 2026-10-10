#pragma once
#include "outland/game/combat/WeaponDefinition.hpp"
#include "outland/world/navigation/NavGrid.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <vector>

// Battle Royale combat intelligence. A CombatBot is a tactical brain: it perceives other combatants
// only through sight (field of view + line of sight) and sound, remembers where it last saw them,
// and chooses between engaging, taking cover, reloading, healing, pushing, flanking, retreating,
// investigating and roaming. It never reads positions it has not perceived.
//
// Difficulty changes how fast and how well a bot thinks and shoots - reaction time, aim error and
// settle, decision rate and quality, aggression - never its health, damage, ammunition or knowledge.
namespace outland::game::ai {

enum class BotLevel : std::uint8_t { Easy, Medium, Hard };
const char* bot_level_name(BotLevel level);

struct BotDifficulty {
    float reaction{.55F};          // seconds of continuous sight before a new enemy is acted on
    float aim_error_deg{3.5F};     // spread when first aiming at a target
    float aim_floor_deg{.9F};      // best spread after settling
    float settle_seconds{1.2F};    // tracking time from first error to floor
    float decision_interval{.4F};  // seconds between tactical decisions
    float decision_quality{.8F};   // chance of picking the best-scored option (else the runner-up)
    float aggression{.5F};         // 0 cautious .. 1 reckless
    float view_distance{170};      // metres; sight still needs a clear line
    float fov_deg{120};
    float hearing_scale{1};
    bool lead_targets{false};      // aim ahead of moving targets
};
BotDifficulty bot_difficulty(BotLevel level);

// Everything that fights: the player and every bot. Positions are feet.
struct BotAgent {
    int id{0};
    Vector3 position{}, velocity{};
    bool alive{true};
};
struct BotSound { Vector3 position{}; float radius{0}; int source{-1}; };
struct BotShot { int shooter{0}; Vector3 origin{}, direction{}; float damage{0}; combat::WeaponId weapon{}; };

struct BotEnvironment {
    std::function<bool(Vector3, Vector3)> line_of_sight;       // eye to eye, true if clear
    std::function<Vector3(Vector3 from, Vector3 to)> move;     // collision-resolved walk, returns feet
    world::navigation::PathFinder find_path;                   // optional routes through doors, stairs, round buildings
};

enum class BotIntent : std::uint8_t { Roam, Investigate, Engage, TakeCover, Reload, Heal, Push, Flank, Retreat, Dead };
const char* bot_intent_name(BotIntent intent);

struct BotContact {
    int agent{-1};
    Vector3 last_seen{}, velocity{};
    double seen_at{-1e9}, checked_at{-1e9};
    bool visible{false};
    float awareness{0};   // 0..1; reaches 1 after the reaction time of continuous sight
    float tracking{0};    // seconds of steady aim on this contact
};

class CombatBot {
public:
    CombatBot(int id, BotLevel level, combat::WeaponId weapon, std::uint64_t seed);

    // One frame. `agents` includes this bot (ignored) and the player. Shots fired are appended.
    void tick(float dt, double now, const std::vector<BotAgent>& agents, const std::vector<BotSound>& sounds,
              const BotEnvironment& environment, std::vector<BotShot>& shots);
    // Damage taken (applied to health by the owner); the bot now knows roughly where it came from.
    void on_damage(float amount, Vector3 from, double now);
    // One of this bot's shots connected; confident bots press the advantage.
    void on_hit_given(double now) { last_hit_given_at_ = now; }

    int id() const { return id_; }
    BotLevel level() const { return level_; }
    const BotDifficulty& difficulty() const { return difficulty_; }
    BotIntent intent() const { return intent_; }
    Vector3 position() const { return position_; }
    void set_position(Vector3 feet) { position_ = feet; }
    Vector3 velocity() const { return velocity_; }
    float yaw() const { return yaw_; }
    void set_yaw(float degrees) { yaw_ = degrees; }
    float health() const { return health_; }
    void set_health(float health) { health_ = health; }
    bool alive() const { return health_ > 0; }
    bool moving() const { return moving_; }
    bool reloading() const { return reload_left_ > 0; }
    bool healing() const { return heal_left_ > 0; }
    int loaded() const { return loaded_; }
    int reserve() const { return reserve_; }
    int medkits() const { return medkits_; }
    void give_medkits(int count) { medkits_ += count; }
    combat::WeaponId weapon() const { return weapon_; }
    Vector3 destination() const { return destination_; }
    const world::navigation::PathFollower& follower() const { return follower_; }
    const BotContact* target() const;
    const std::vector<BotContact>& contacts() const { return contacts_; }
    void set_roam_center(Vector3 center, float radius) { roam_center_ = center; roam_radius_ = radius; }

    static constexpr float eye_height = 1.55F;
    static constexpr float walk_speed = 2.2F, run_speed = 4.6F; // same for every difficulty
    static constexpr float heal_seconds = 4.0F, heal_amount = 45.0F;

private:
    float random();
    void perceive(float dt, double now, const std::vector<BotAgent>& agents, const std::vector<BotSound>& sounds, const BotEnvironment& environment);
    void decide(double now, const BotEnvironment& environment);
    bool find_cover(Vector3 threat, const BotEnvironment& environment, Vector3& out);
    bool find_flank(Vector3 threat, const BotEnvironment& environment, Vector3& out);
    void act(float dt, double now, const BotEnvironment& environment, std::vector<BotShot>& shots);
    float preferred_range() const;
    float effective_range() const;

    int id_;
    BotLevel level_;
    BotDifficulty difficulty_;
    combat::WeaponId weapon_;
    std::uint64_t rng_;
    Vector3 position_{}, previous_{}, velocity_{}, destination_{};
    float yaw_{0}, health_{100};
    BotIntent intent_{BotIntent::Roam};
    std::vector<BotContact> contacts_;
    int target_{-1};
    bool moving_{false}, has_destination_{false}, sound_pending_{false};
    Vector3 heard_{};
    double last_damage_at_{-1e9}, last_hit_given_at_{-1e9}, decided_at_{-1e9}, intent_since_{0};
    Vector3 damage_from_{};
    float decision_clock_{0}, stuck_time_{0};
    int loaded_{0}, reserve_{0}, medkits_{1}, burst_{0};
    float cooldown_{0}, reload_left_{0}, heal_left_{0};
    world::navigation::PathFollower follower_;
    bool repathed_{false};
    Vector3 roam_center_{};
    float roam_radius_{120};
};
}
