#pragma once
#include "outland/game/combat/WeaponSystem.hpp"

namespace outland::game::combat {
class CombatRenderer {
public:
    static void draw_world(const WeaponSystem& weapons, const CombatWorld& world);
    static void draw_gun(Vector3 muzzle, Vector3 direction, WeaponId id, float reload_fraction, float flash);
};
}
