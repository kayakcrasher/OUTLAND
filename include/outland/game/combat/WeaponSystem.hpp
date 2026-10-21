#pragma once

#include "outland/game/combat/WeaponDefinition.hpp"
#include "outland/game/combat/CombatWorld.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace outland::game::combat {

struct WeaponInput {
    bool fire{false}, aim{false}, reload{false}, next_weapon{false}, sprint{false}, grounded{true};
    bool crouched{false}, fire_mode{false}; // fire_mode: toggle automatic / single shots
    float movement{0};
};
struct ShotPose { Vector3 origin{}, direction{0,0,-1}; };
struct Bullet {
    bool active{false}, tracer{false};
    Vector3 position{}, previous{}, velocity{};
    float age{0}, travelled{0}, damage{0}, initial_speed{0}, drag{0};
    float head{2}, limb{1};
};
struct BulletImpact {
    Vector3 position{}, normal{};
    HitKind kind{HitKind::None};
    float life{0};
};
struct HitNote { Vector3 position{}; float damage{0}; bool headshot{false}; };
struct Launch { Vector3 origin{}, velocity{}; };   // a rocket leaving the launcher (the caller flies it)
struct WeaponEvents {
    int shots{0};
    std::vector<HitNote> hits; // what this update's bullets struck (people, targets, vehicles)
    std::vector<Launch> launches;
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
    // Optional inventory bridge: legacy standalone combat keeps its existing reserves.
    // Bound reserves mirror the bag; only the consume callback may spend loose rounds.
    void bind_inventory(std::function<bool(WeaponId)> owned,
                        std::function<int(WeaponId)> reserve,
                        std::function<int(WeaponId,int)> consume);
    void unbind_inventory();
    bool select(WeaponId id);
    // Unarmed: the gun stays in the bag. SWITCH WEAPON steps through the guns owned and then to
    // empty hands (and from there back to the first gun); select() draws one again.
    void holster();
    [[nodiscard]] bool holstered() const { return holstered_; }
    bool available() const;
    bool set_loaded(WeaponId id,int rounds);
    void request_reload();
    [[nodiscard]] float reload_remaining() const { return reload_remaining_; }
    [[nodiscard]] float muzzle_flash() const { return muzzle_flash_; }
    [[nodiscard]] float cooldown() const { return cooldown_; } // seconds until the next shot may fire
    [[nodiscard]] float hit_marker() const { return hit_marker_; }
    [[nodiscard]] bool last_headshot() const { return last_headshot_; }
    [[nodiscard]] float last_damage() const { return last_damage_; }
    [[nodiscard]] const WeaponEvents& events() const { return events_; }
    [[nodiscard]] const std::array<Bullet,96>& bullets() const { return bullets_; }
    [[nodiscard]] const std::array<BulletImpact,64>& impacts() const { return impacts_; }
    // An automatic weapon can be switched to single shots (per weapon).
    [[nodiscard]] bool single_fire() const { return single_[static_cast<std::size_t>(selected_)] || !weapon().automatic; }
    [[nodiscard]] int spray() const { return spray_; } // consecutive shots in the current burst
private:
    void start_reload();
    void refresh_reserves();
    bool shoot(const WeaponInput& input, ShotPose pose);
    void simulate(float dt, CombatWorld& world);
    float random();
    WeaponId selected_{WeaponId::Rifle};
    std::array<bool,weapon_count> single_{};
    int spray_{0};
    float since_shot_{10};
    std::array<Magazine,weapon_count> ammo_{};
    std::array<Bullet,96> bullets_{};
    std::array<BulletImpact,64> impacts_{};
    WeaponEvents events_{};
    bool unlimited_{false}, previous_fire_{false}, last_headshot_{false}, holstered_{false};
    float cooldown_{0}, reload_remaining_{0}, muzzle_flash_{0}, hit_marker_{0}, last_damage_{0};
    std::uint32_t random_state_{0x51f7349aU};
    unsigned int shot_number_{0}, impact_index_{0};
    std::function<bool(WeaponId)> owned_;
    std::function<int(WeaponId)> reserve_;
    std::function<int(WeaponId,int)> consume_;
};

} // namespace outland::game::combat
