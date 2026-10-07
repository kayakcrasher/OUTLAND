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
    float hip_spread;
    float aimed_spread;
    float recoil;
};

// Gameplay values in metres, seconds and radians; tune against device playtests.
inline constexpr std::array<WeaponDefinition, 2> weapons{{
    {"V9 PISTOL", 12, 60, false, 0.24F, 1.6F, 350.0F, 34.0F, 0.12F, 0.016F, 0.0025F, 0.025F},
    {"VR30 RIFLE", 30, 120, true, 0.10F, 2.1F, 760.0F, 28.0F, 0.08F, 0.021F, 0.0018F, 0.015F}
}};

inline const WeaponDefinition& definition(WeaponId id) {
    return weapons[static_cast<std::size_t>(id)];
}

} // namespace outland::game::combat
