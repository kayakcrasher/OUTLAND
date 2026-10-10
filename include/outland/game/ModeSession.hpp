#pragma once
#include "outland/game/GameMode.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/combat/Health.hpp"
#include "outland/world/VerdaRegion.hpp"
namespace outland::game {
enum class SessionPhase { Playing, Won, Dead };
struct MatchResult { int placement{0}, kills{0}, participants{0}; ai::BotLevel difficulty{ai::BotLevel::Medium}; };
struct CircleRules { float opening_seconds{60}, shrink_seconds{600}, start_radius{2450}, end_radius{25}, damage_per_second{6}; };
// Owns match rules/results, never NPC brains, equipment, world state, or rendering.
class ModeSession {
public:
    void start(GameMode mode, int bots, ai::BotLevel difficulty);
    void update(float dt, Vector3 feet, Health&, ai::BattleRoyaleBots&, bool paused);
    GameMode mode() const { return mode_; }
    SessionPhase phase() const { return phase_; }
    bool terminal() const { return phase_ != SessionPhase::Playing; }
    const MatchResult& result() const { return result_; }
    float radius() const;
    float distance(Vector3 feet) const;
    float elapsed() const { return elapsed_; }
    const char* objective() const;
    CircleRules circle;
private:
    GameMode mode_{GameMode::Home}; SessionPhase phase_{SessionPhase::Playing};
    MatchResult result_{}; float elapsed_{0};
};
// Feet coordinates, resolved against the same terrain/buildings as runtime movement.
Vector3 session_spawn(const world::VerdaRegion&, GameMode, std::uint64_t seed);
}
