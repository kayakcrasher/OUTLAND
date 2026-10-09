#pragma once

#include "outland/game/combat/WeaponDefinition.hpp"
#include "outland/game/combat/CombatWorld.hpp"
#include <array>
#include <cstdint>

namespace outland::game::combat {

struct WeaponInput {
    bool fire{false}, aim{false}, reload{false}, next_weapon{false}, sprint{false}, grounded{true};
    float movement{0};
};
struct ShotPose { Vector3 origin{}, direction{0,0,-1}; };
struct Bullet {
    bool active{false}, tracer{false};
    Vector3 position{}, previous{}, velocity{};
    float age{0}, travelled{0}, damage{0}, initial_speed{0}, drag{0};
};
struct BulletImpact {
    Vector3 position{}, normal{};
    HitKind kind{HitKind::None};
    float life{0};
};
struct WeaponEvents {
    int shots{0};
    int target_hits{0};
    bool reload_started{false}, reload_finished{false}, dry_fire{false};
    float pitch_kick{0}, yaw_kick{0};
};
struct Magazine { int loaded{0}, reserve{0}; };

class WeaponSystem {
public:
    WeaponSystem();
    void reset(bool unlimited);
    void update(float dt, const WeaponInput& input, ShotPose pose, CombatWorld& world);
    [[nodiscard]] WeaponId selected() const { return selected_; }
    [[nodiscard]] const WeaponDefinition& weapon() const { return definition(selected_); }
    [[nodiscard]] const Magazine& ammo() const { return ammo_[static_cast<std::size_t>(selected_)]; }
    [[nodiscard]] bool unlimited() const { return unlimited_; }
    [[nodiscard]] const Magazine& ammo(WeaponId id) const { return ammo_[static_cast<std::size_t>(id)]; }
    // Collect one magazine of reserve rounds, capped at the weapon's starting reserve.
    int collect_ammo();
    [[nodiscard]] float reload_remaining() const { return reload_remaining_; }
    [[nodiscard]] float muzzle_flash() const { return muzzle_flash_; }
    [[nodiscard]] float hit_marker() const { return hit_marker_; }
    [[nodiscard]] bool last_headshot() const { return last_headshot_; }
    [[nodiscard]] float last_damage() const { return last_damage_; }
    [[nodiscard]] const WeaponEvents& events() const { return events_; }
    [[nodiscard]] const std::array<Bullet,96>& bullets() const { return bullets_; }
    [[nodiscard]] const std::array<BulletImpact,64>& impacts() const { return impacts_; }
private:
    void start_reload();
    bool shoot(const WeaponInput& input, ShotPose pose);
    void simulate(float dt, CombatWorld& world);
    float random();
    WeaponId selected_{WeaponId::Rifle};
    std::array<Magazine,2> ammo_{};
    std::array<Bullet,96> bullets_{};
    std::array<BulletImpact,64> impacts_{};
    WeaponEvents events_{};
    bool unlimited_{false}, previous_fire_{false}, last_headshot_{false};
    float cooldown_{0}, reload_remaining_{0}, muzzle_flash_{0}, hit_marker_{0}, last_damage_{0};
    std::uint32_t random_state_{0x51f7349aU};
    unsigned int shot_number_{0}, impact_index_{0};
};

} // namespace outland::game::combat
