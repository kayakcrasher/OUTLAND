#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace outland::game::combat {

// Saves store weapons by index: append new weapons, never reorder.
enum class WeaponId : std::size_t { Pistol, Rifle, Smg, Shotgun, Sniper, DoubleBarrel, AutoShotgun, Rocket, MachinePistol };
inline constexpr std::size_t weapon_count = 9;

// How the body holds it: a pistol in both hands at arm's length, long guns shouldered.
enum class WeaponHold : unsigned char { Pistol, Shouldered };

struct WeaponDefinition {
    const char* name;
    int magazine;
    int reserve;
    bool automatic;
    float interval;
    float reload_seconds;
    float muzzle_speed;
    float damage;           // per projectile
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
    int pellets;            // projectiles per shot (shotguns); each takes its own spread
    float pattern;          // radians: pellet cone, independent of aim (0 = single projectile)
    std::string_view item;  // weapon key in items.tsv ("pistol", "rifle", ...)
    WeaponHold hold;
    float length;           // metres, grip to muzzle (the model's origin is the pistol grip)
    float hearing;          // metres a shot carries (SoundBus)
    float bot_range;        // metres a bot will engage at
    int sound;              // 0 pistol report, 1 rifle report
    float sound_pitch;
    bool launcher{false};   // fires a rocket (Grenades) instead of bullets; `muzzle_speed` is its speed
};

// Gameplay values in metres, seconds and radians; tune against device playtests.
inline constexpr std::array<WeaponDefinition, weapon_count> weapons{{
    // Semi-automatic sidearm: quick to bring up, soft-hitting, gentle recoil.
    {"V9 PISTOL", 15, 60, false, 0.135F, 1.9F, 380.0F, 35.0F, 0.12F, 0.030F, 0.0030F, 0.0110F,
     0.04F, 0.0050F, 0.60F, 0.06F, 55.0F, 2.10F, 0.90F,
     1, 0, "pistol", WeaponHold::Pistol, 0.12F, 180, 45, 0, 1.0F},
    // 5.56 assault rifle: 3 body hits without armour, a climbing spray to control, a holographic sight.
    {"VR30 RIFLE", 30, 120, true, 0.086F, 2.1F, 880.0F, 41.0F, 0.08F, 0.045F, 0.0012F, 0.0085F,
     0.07F, 0.0045F, 0.55F, 0.09F, 46.0F, 2.35F, 0.90F,
     1, 0, "rifle", WeaponHold::Shouldered, 0.52F, 300, 140, 1, 1.0F},
    // .45 submachine gun: fast and steady up close, falls off with range, forgiving from the hip.
    {"VK45 SMG", 25, 100, true, 0.075F, 2.3F, 360.0F, 32.0F, 0.16F, 0.026F, 0.0022F, 0.0058F,
     0.04F, 0.0035F, 0.60F, 0.045F, 52.0F, 1.80F, 1.00F,
     1, 0, "smg", WeaponHold::Shouldered, 0.30F, 200, 55, 0, 1.25F},
    // 12-gauge pump: nine pellets in a fixed cone, devastating inside 15 m, useless at 60.
    {"VS12 SHOTGUN", 5, 25, false, 0.80F, 4.2F, 380.0F, 22.0F, 2.50F, 0.010F, 0.004F, 0.0450F,
     0.0F, 0.0080F, 0.30F, 0.0F, 60.0F, 1.50F, 0.90F,
     9, 0.055F, "shotgun", WeaponHold::Shouldered, 0.55F, 280, 22, 1, 0.70F},
    // 7.62 bolt-action: one body hit nearly kills, a head shot always does; 4x scope, slow bolt.
    {"VP7 SNIPER", 5, 20, false, 1.55F, 3.8F, 760.0F, 82.0F, 0.03F, 0.070F, 0.0002F, 0.0420F,
     0.0F, 0.0060F, 0.20F, 0.0F, 15.0F, 2.50F, 0.90F,
     1, 0, "sniper", WeaponHold::Shouldered, 0.72F, 420, 260, 1, 0.80F},
    // Sawn-off double barrel: both barrels as fast as you can pull, twelve pellets each, then a
    // slow break-open reload. The hardest hit in the game inside 8 m, nothing past 30.
    {"VD2 DOUBLE BARREL", 2, 20, false, 0.24F, 2.6F, 360.0F, 22.0F, 3.20F, 0.012F, 0.006F, 0.0650F,
     0.0F, 0.0110F, 0.30F, 0.0F, 62.0F, 1.50F, 0.90F,
     12, 0.075F, "double_barrel", WeaponHold::Shouldered, 0.50F, 300, 16, 1, 0.62F},
    // Drum-fed automatic 12-gauge: eight shells as fast as a rifle can fire, a tighter pattern
    // than the pump but lighter pellets; a big drum to refill.
    {"VA12 AUTO SHOTGUN", 8, 32, true, 0.20F, 3.4F, 380.0F, 14.0F, 2.60F, 0.014F, 0.005F, 0.0300F,
     0.05F, 0.0120F, 0.40F, 0.05F, 60.0F, 1.50F, 0.90F,
     8, 0.050F, "auto_shotgun", WeaponHold::Shouldered, 0.58F, 270, 20, 1, 0.85F},
    // Shoulder-fired rocket: one round, a slow reload, a blast that wrecks cars and anyone
    // standing near the impact (Grenades::rocket_damage_at); the rocket flies straight at 60 m/s.
    {"VR7 ROCKET LAUNCHER", 1, 4, false, 1.0F, 3.6F, 60.0F, 0.0F, 0.0F, 0.020F, 0.0030F, 0.0600F,
     0.0F, 0.0100F, 0.60F, 0.0F, 50.0F, 1.0F, 1.0F,
     1, 0, "rocket_launcher", WeaponHold::Shouldered, 0.55F, 450, 0, 1, 0.50F, true},
    // Compact 9 mm machine pistol: a 30-round stick, a frantic cyclic rate, one hand - the gun that
    // makes a drive-by. Shares the pistol's 9 mm rounds; sprays wide, hits soft, falls off fast.
    {"VM9 MACHINE PISTOL", 30, 90, true, 0.058F, 1.8F, 340.0F, 23.0F, 0.18F, 0.036F, 0.0045F, 0.0068F,
     0.06F, 0.0065F, 0.55F, 0.06F, 56.0F, 1.80F, 0.90F,
     1, 0, "machine_pistol", WeaponHold::Pistol, 0.20F, 170, 40, 0, 1.35F},
}};

inline const WeaponDefinition& definition(WeaponId id) {
    return weapons[static_cast<std::size_t>(id)];
}

// The ammunition a weapon loads: every shotgun takes the same 12-gauge shells.
inline constexpr WeaponId ammo_of(WeaponId id) {
    return id == WeaponId::DoubleBarrel || id == WeaponId::AutoShotgun ? WeaponId::Shotgun : id == WeaponId::MachinePistol ? WeaponId::Pistol : id;
}
inline constexpr bool is_shotgun(WeaponId id) { return ammo_of(id) == WeaponId::Shotgun; }

// The weapon an items.tsv key names, if any.
inline bool weapon_from_item(std::string_view key, WeaponId& out) {
    for (std::size_t i = 0; i < weapons.size(); ++i)
        if (weapons[i].item == key) { out = static_cast<WeaponId>(i); return true; }
    return false;
}

} // namespace outland::game::combat
