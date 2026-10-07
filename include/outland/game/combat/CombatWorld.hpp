#pragma once

#include "outland/world/VerdaRegion.hpp"
#include <array>

namespace outland::game::combat {

enum class HitKind { None, Ground, Building, Tree, Structure, Target };
struct BulletHit {
    HitKind kind{HitKind::None};
    float fraction{1.0F};
    Vector3 position{};
    Vector3 normal{};
    int target{-1};
    bool headshot{false};
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
    [[nodiscard]] BulletHit trace_segment(Vector3 start, Vector3 end) const;
    void damage_target(int index, float damage);
    void update(float dt, bool respawn_targets);
    void reset_targets();
    [[nodiscard]] const std::array<RangeTarget, 9>& targets() const { return targets_; }
private:
    const world::VerdaRegion& region_;
    std::array<RangeTarget, 9> targets_{};
};

} // namespace outland::game::combat
