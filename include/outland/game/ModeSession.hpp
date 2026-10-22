#pragma once
#include "outland/game/GameMode.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/combat/Health.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cstdint>
#include <functional>
#include <vector>
namespace outland::game {
enum class SessionPhase { Playing, Won, Dead };
struct MatchResult { int placement{0}, kills{0}, participants{0}; ai::BotLevel difficulty{ai::BotLevel::Medium}; };
// The Battle Royale zone, as in PUBG: a sequence of circles. Each phase waits (the next circle is
// shown, white, somewhere inside the current one), then the wall closes on it; the gas bites
// harder every phase. The first circle covers the island while the helicopters fly.
struct ZonePhase { float wait{0}, shrink{0}, radius{0}, damage{0}; };
struct CircleRules {
    float start_radius{2450};
    float damage_per_second{1};   // multiplies every phase's damage (tests raise it)
    std::vector<ZonePhase> phases{
        {120, 90, 1300, 1}, {75, 70, 750, 2}, {60, 55, 420, 4}, {50, 45, 220, 6},
        {40, 35, 100, 9}, {30, 30, 35, 13}, {30, 40, 0, 18}};
};
struct ZoneState {
    Vector2 center{}, next_center{};    // x, z
    float radius{0}, next_radius{0};
    int phase{0};                       // index into CircleRules::phases (== size once closed)
    bool shrinking{false};
    float remaining{0};                 // seconds until the wall moves, or until it stops
    float damage{0};                    // per second outside
};
// Owns match rules/results, never NPC brains, equipment, world state, or rendering.
class ModeSession {
public:
    void start(GameMode mode, int bots, ai::BotLevel difficulty);
    // Pick where each circle will close (random, each wholly inside the last, centred on land
    // when `on_land` says so). Without it the circles all close on the capital.
    void plan_circles(std::uint64_t seed, const std::function<bool(float x, float z)>& on_land = {});
    void update(float dt, Vector3 feet, Health&, ai::BattleRoyaleBots&, bool paused);
    // A LAN match: how many other people still alive are against the player (the last side standing
    // wins), whether the bots are run here (the host) or only mirrored (a client: the zone does not
    // hurt them here), and how many of the player's own side are still up (playing as a side).
    void set_lan(int others_alive, bool run_bots, int team_alive = 0) { others_alive_ = others_alive; run_bots_ = run_bots; team_alive_ = team_alive; }
    // Out, but the player's side fights on (the game can follow them); and the side won without them.
    bool squad_fighting() const { return phase_ == SessionPhase::Dead && mode_ == GameMode::BattleRoyale && team_alive_ > 0; }
    bool squad_won() const { return squad_won_; }
    void add_participants(int people) { result_.participants += people; }
    GameMode mode() const { return mode_; }
    SessionPhase phase() const { return phase_; }
    bool terminal() const { return phase_ != SessionPhase::Playing; }
    const MatchResult& result() const { return result_; }
    float radius() const { return zone().radius; }
    // Damage per second outside the circle: it bites harder every phase.
    float zone_damage() const { return zone().damage; }
    // From the circle's centre (which moves as the zone closes), on the ground plane.
    float distance(Vector3 feet) const;
    ZoneState zone() const;
    Vector2 center() const { return zone().center; }
    float elapsed() const { return elapsed_; }
    const char* objective() const;
    CircleRules circle;
private:
    GameMode mode_{GameMode::Home}; SessionPhase phase_{SessionPhase::Playing};
    MatchResult result_{}; int others_alive_{0}, team_alive_{0};
    bool squad_won_{false};
    bool run_bots_{true};
    float elapsed_{0};
    std::vector<Vector2> centers_; // centers_[k]: where phase k's circle closes
};
// Feet coordinates, resolved against the same terrain/buildings as runtime movement.
Vector3 session_spawn(const world::VerdaRegion&, GameMode, std::uint64_t seed);
}
