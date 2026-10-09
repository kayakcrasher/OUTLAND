#pragma once
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/AnimationController.hpp"
#include <raylib.h>
#include <functional>
namespace outland::characters {
enum class NpcState { Idle, Wander, Alert, Chase, Attack, Flee, Dead };
struct NpcTuning {
    float detection_radius{18}, attack_range{1.6F}, movement_speed{1.5F}, run_multiplier{1.7F};
    float reaction_delay{.6F}, health{100}, activation_distance{65}, despawn_distance{100};
    float attack_damage{12}, attack_interval{1.0F}, wander_radius{6}, memory_seconds{4};
    void sanitize();
};
struct NpcContext {
    Vector3 player_position{};
    bool player_alive{true}, threatening{false}, paused{false};
    std::function<bool(Vector3,Vector3)> visible;
};
struct NpcEnvironment {
    std::function<Vector3(Vector3,Vector3)> move;
};
struct NpcInstance;
class NpcBehavior {
public:
    static float tick(NpcInstance& actor,const NpcTuning& tuning,float dt,
        const NpcContext& context,const NpcEnvironment& environment);
    static AnimationAction animation(NpcState state);
};
}
