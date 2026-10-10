#pragma once
#include <array>
#include <cstddef>

namespace outland::game::combat {

enum class WeaponId : std::size_t { Pistol, Rifle };

struct WeaponDefinition {
    const char* name;
    int magazine;
    int reserve;
    bool automatic;
    float interval;
    float reload_seconds;
    float muzzle_speed;
    float damage;
    float drag;
    float hip_spread;       // radians, first hip-fire shot
    float aimed_spread;     // radians, first aimed shot
    float recoil;           // radians of muzzle climb per shot (before growth and stance)
    // Battle-royale gunplay: recoil climbs through a spray and the player pulls it down; part of
    // it settles back once the trigger is released. Hip fire blooms; aimed fire stays tight.
    float recoil_growth;    // extra climb per consecutive shot (fraction of `recoil`, up to 10 shots)
    float recoil_side;      // radians of random sideways kick per shot
    float recoil_recovery;  // share of the climb that settles back after the burst
    float bloom;            // hip spread growth per consecutive shot
    float ads_fov;          // vertical field of view while aiming (degrees)
    float head_multiplier;
    float limb_multiplier;  // legs
};

// Gameplay values in metres, seconds and radians; tune against device playtests.
inline constexpr std::array<WeaponDefinition, 2> weapons{{
    // Semi-automatic sidearm: quick to bring up, soft-hitting, gentle recoil.
    {"V9 PISTOL", 15, 60, false, 0.135F, 1.9F, 380.0F, 35.0F, 0.12F, 0.030F, 0.0030F, 0.0110F,
     0.04F, 0.0050F, 0.60F, 0.06F, 55.0F, 2.10F, 0.90F},
    // 5.56 assault rifle: 3 body hits without armour, a climbing spray to control, a holographic sight.
    {"VR30 RIFLE", 30, 120, true, 0.086F, 2.1F, 880.0F, 41.0F, 0.08F, 0.045F, 0.0012F, 0.0085F,
     0.07F, 0.0045F, 0.55F, 0.09F, 46.0F, 2.35F, 0.90F}
}};

inline const WeaponDefinition& definition(WeaponId id) {
    return weapons[static_cast<std::size_t>(id)];
}

} // namespace outland::game::combat
