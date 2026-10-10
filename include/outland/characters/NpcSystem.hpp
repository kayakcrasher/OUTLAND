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
    // Life-driven actors: owned by game::life, kept across marker reconciliation.
    int resident{-1};
    bool directed{false}, travelling{false}, hurry{false};
    float anchor_radius{6}, idle_hold{1};
};
struct ResidentSpawn {
    int resident{-1};
    std::string character_id;
    CharacterPool pool{CharacterPool::Civilian};
    Vector3 position{}, anchor{};
    float yaw_degrees{0}, health{100}, anchor_radius{2};
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
    std::size_t spawn_resident(const ResidentSpawn& spawn);
    bool despawn_resident(int resident);
    int find_resident(int resident) const;
    // Moves the schedule anchor; Idle/Wander actors walk there, threat states are untouched.
    void direct_resident(int resident,Vector3 anchor,float radius,bool hurry);
    const NpcEvents& events() const { return events_; }
    const std::vector<NpcInstance>& actors() const { return actors_; }
private:
    std::array<NpcTuning,5> tuning_{};
    std::vector<NpcInstance> actors_;
    NpcEvents events_;
    float player_threat_timer_{0};
};
}
