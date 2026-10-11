#pragma once
#include "outland/game/combat/Sights.hpp"
#include "outland/game/combat/WeaponSystem.hpp"

namespace outland::game::combat {
// Where a gun sits in the hands, in metres from its model origin: the trigger, on the bore line,
// with +z toward the muzzle and +y up.
struct GunLayout {
    float stock;   // origin back to the butt (0: a pistol)
    float support; // origin forward to the support hand
    float sight;   // bore up to the sight line
    float length;  // origin forward to the muzzle
};
GunLayout gun_layout(WeaponId id,Sight sight=Sight::Iron);

class CombatRenderer {
public:
    static void draw_world(const WeaponSystem& weapons, const CombatWorld& world);
    // Real-scale guns (a rifle is about 0.84 m end to end). `grip` is the model origin; `aim` and
    // `up` orient it; `pump` (0..1) works a pump or bolt. Hands are drawn for the first-person view.
    // `sight` is the optic on its rail (iron sights draw none).
    static void draw_gun(Vector3 grip, Vector3 aim, Vector3 up, WeaponId id, float reload_fraction, float flash,
                         bool draw_hands = true, float pump = 0, Sight sight = Sight::Iron);
    // Aiming through an optic, drawn over the 3D view: a scope's tube and reticle filling the
    // screen (2x, 4x, sniper), or a red dot's dot. `aimed` (0..1) fades the red dot in as the gun comes up.
    static void draw_sight_view(Sight sight, int width, int height, float aimed);
    static Vector3 muzzle(Vector3 grip, Vector3 aim, WeaponId id);
};
}
