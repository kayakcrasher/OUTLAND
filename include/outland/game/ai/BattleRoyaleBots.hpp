#pragma once
#include "outland/game/ai/CombatBot.hpp"
#include "outland/game/combat/CombatWorld.hpp"
#include "outland/game/sound/SoundBus.hpp"
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::characters { class NpcSystem; class CharacterRegistry; }

// A Battle Royale match: the player (agent 0) against a field of CombatBots that also fight each
// other. Bot shots are hitscan against the same world the player's bullets use; bot bodies live in
// NpcSystem so they are drawn, animated and hit by the player's weapon like any other character.
namespace outland::game::ai {

struct BotWorld {
    BotEnvironment environment;                                    // sight and walking
    std::function<combat::BulletHit(Vector3, Vector3)> trace;     // world + vehicles, no characters
    std::function<void(const combat::BulletHit&, float, Vector3)> world_damage; // vehicles etc.
    // The island's shared sound bus. When set, bots hear everything on it (and add their own
    // gunfire and footsteps), and the owner emits the player's sounds. When null the match keeps
    // a private bus and emits the player's sounds from the frame.
    sound::SoundBus* sounds{nullptr};
};

struct BotTracer { Vector3 start{}, end{}; float life{0}; };

struct BattleRoyaleFrame {
    float dt{0};
    double now{0};
    Vector3 player_feet{}, player_velocity{};
    bool player_alive{true};
    bool player_fired{false};
    combat::WeaponId player_weapon{combat::WeaponId::Rifle};
    bool player_in_vehicle{false}; // the vehicle trace handles occupants
};

struct BattleRoyaleEvents {
    float player_damage{0};
    Vector3 player_damage_from{};
    int player_damage_by{-1};
    int shots{0}, hits{0}, kills{0};
    // Rounds that missed the player narrowly: where each passed closest to the head (a crack
    // past the ear), and where each struck the ground or a wall close by (a ricochet).
    std::vector<Vector3> near_misses, ricochets;
};

class BattleRoyaleBots {
public:
    static constexpr int player_id = 0;
    static constexpr float gunshot_radius_rifle = 300, gunshot_radius_pistol = 180, footstep_radius = 14;

    static constexpr float drop_clearance = 150; // no bot lands within this of the player
    // Places `count` bots on land around Verda's towns, away from the player.
    void start(const world::VerdaRegion& region, BotLevel level, int count, std::uint64_t seed, Vector3 player_feet);
    // Explicit placement (tests, scripted fights).
    CombatBot& add_bot(Vector3 feet, combat::WeaponId weapon, BotLevel level, std::uint64_t seed);
    void clear();

    void update(const BattleRoyaleFrame& frame, const BotWorld& world);
    // The player's weapon hit a bot body. Returns true if it killed the bot.
    bool damage_bot(int bot, float amount, Vector3 from, double now);
    // The owner applies player damage; it reports the death back for the kill feed.
    void report_player_death(int killer);
    void apply_circle(float radius, float damage);
    // A bot at the wheel (game::ai::BotDrivers) is mounted: its car carries it, it does not think,
    // shoot or get hit (bullets hit the car), and its body is hidden.
    void set_mounted(int bot, bool mounted);
    [[nodiscard]] bool mounted(int bot) const;
    // An asleep bot (a Mafio crew far from the player) stands where it is: it does not think,
    // move or shoot, but it is drawn and can be hit.
    void set_asleep(int bot, bool asleep);
    [[nodiscard]] bool asleep(int bot) const;
    // Back on its feet at `feet`, full health and a fresh mind (a crew member's respawn).
    void respawn(int bot, Vector3 feet, float yaw, std::uint64_t seed);

    // Keep bot bodies in NpcSystem in step with the brains (spawns missing bodies).
    void sync_bodies(characters::NpcSystem& npcs, const characters::CharacterRegistry* registry) const;

    [[nodiscard]] bool active() const { return !bots_.empty(); }
    [[nodiscard]] BotLevel level() const { return level_; }
    [[nodiscard]] int alive_bots() const;
    [[nodiscard]] const std::vector<CombatBot>& bots() const { return bots_; }
    [[nodiscard]] std::vector<CombatBot>& bots() { return bots_; }
    [[nodiscard]] CombatBot* find(int bot);
    [[nodiscard]] const BattleRoyaleEvents& events() const { return events_; }
    [[nodiscard]] const std::vector<BotTracer>& tracers() const { return tracers_; }
    [[nodiscard]] const std::vector<std::string>& feed() const { return feed_; }
    [[nodiscard]] double elapsed() const { return elapsed_; }
    [[nodiscard]] int player_kills() const { return player_kills_; }

private:
    void resolve(const BotShot& shot, const BattleRoyaleFrame& frame, const BotWorld& world);

    std::vector<CombatBot> bots_;
    std::vector<BotAgent> agents_;
    std::vector<BotSound> sounds_;
    sound::SoundBus own_bus_;
    std::uint64_t heard_up_to_{0};
    std::vector<BotShot> shots_;
    std::vector<BotTracer> tracers_;
    std::vector<std::string> feed_;
    std::vector<int> mounted_, asleep_;
    BattleRoyaleEvents events_;
    BotLevel level_{BotLevel::Medium};
    Vector3 last_player_{};
    double elapsed_{0};
    int player_kills_{0};
};
}
