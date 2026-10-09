#pragma once

#include "outland/world/VerdaRegion.hpp"
#include <array>
#include <functional>

namespace outland::game::combat {

enum class HitKind { None, Ground, Building, Tree, Structure, Target, Npc, Vehicle, VehicleWindow, VehicleOccupant };
struct BulletHit {
    HitKind kind{HitKind::None};
    float fraction{1.0F};
    Vector3 position{};
    Vector3 normal{};
    int target{-1};
    bool headshot{false};
    int zone{0};
    float penetration{0};
    [[nodiscard]] bool hit() const { return kind != HitKind::None; }
};

struct RangeTarget {
    Vector3 center{};
    float health{100.0F};
    float reset_timer{0.0F};
};

class CombatWorld {
public:
    explicit CombatWorld(const world::VerdaRegion& region);
    [[nodiscard]] BulletHit trace_segment(Vector3 start, Vector3 end, bool actors=true, bool targets=true, bool vehicles=true) const;
    void bind_actors(std::function<BulletHit(Vector3,Vector3)> trace, std::function<void(int,float,Vector3)> damage) {
        actor_trace_=std::move(trace);actor_damage_=std::move(damage);
    }
    void bind_vehicles(std::function<BulletHit(Vector3,Vector3,bool)> trace,std::function<void(const BulletHit&,float)> damage) {vehicle_trace_=std::move(trace);vehicle_damage_=std::move(damage);}
    void damage_hit(const BulletHit& hit,float damage,Vector3 attacker);
    void damage_target(int index, float damage);
    void update(float dt, bool respawn_targets);
    void reset_targets();
    [[nodiscard]] const std::array<RangeTarget, 9>& targets() const { return targets_; }
private:
    std::function<BulletHit(Vector3,Vector3,bool)> vehicle_trace_;
    std::function<void(const BulletHit&,float)> vehicle_damage_;
    std::function<BulletHit(Vector3,Vector3)> actor_trace_;
    std::function<void(int,float,Vector3)> actor_damage_;
    const world::VerdaRegion& region_;
    std::array<RangeTarget, 9> targets_{};
};

} // namespace outland::game::combat
