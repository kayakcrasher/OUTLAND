#pragma once
#include "outland/characters/NpcBehavior.hpp"
#include <array>
#include <string>
#include <vector>
namespace outland::world { class VerdaRegion; }
namespace outland::characters {
struct NpcInstance {
    std::string spawn_key, character_id;
    Vector3 position{}, spawn_position{}, waypoint{}, threat_position{};
    float yaw_degrees{0}, spawn_yaw{0}, health{100};
    CharacterPool pool{CharacterPool::Civilian};
    NpcState state{NpcState::Idle};
    bool active{false}, visible{false};
    float state_time{0}, decision_clock{0}, reaction_clock{0}, threat_timer{0}, attack_clock{0};
    std::uint64_t random_state{1};
    AnimationController animation;
};
struct NpcHit {
    int actor{-1}; float fraction{1}; Vector3 position{}, normal{}; bool headshot{false};
};
struct NpcEvents { float player_damage{0}; int attacks{0}; };
class NpcSystem {
public:
    NpcSystem();
    void reconcile(const world::VerdaRegion& region,const CharacterRegistry& registry);
    void reset_session();
    void configure(CharacterPool category,NpcTuning tuning);
    const NpcTuning& tuning(CharacterPool category) const;
    void update(float dt,const NpcContext& context,const world::VerdaRegion& region);
    bool damage(std::size_t actor,float amount,Vector3 attacker);
    NpcHit trace_segment(Vector3 start,Vector3 end) const;
    const NpcEvents& events() const { return events_; }
    const std::vector<NpcInstance>& actors() const { return actors_; }
private:
    std::array<NpcTuning,5> tuning_{};
    std::vector<NpcInstance> actors_;
    NpcEvents events_;
    float player_threat_timer_{0};
};
}
