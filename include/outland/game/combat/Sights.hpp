#pragma once
#include "outland/game/combat/WeaponDefinition.hpp"
#include <array>
#include <functional>
#include <string_view>

namespace outland::game::combat {

// Optics. A gun wears iron sights until a red dot, 2x or 4x scope from the bag is mounted on it;
// the sniper always looks through its own fixed scope.
enum class Sight : unsigned char { Iron, RedDot, Scope2x, Scope4x, Sniper };
inline constexpr std::array<Sight,3> attachments{Sight::RedDot, Sight::Scope2x, Sight::Scope4x};

struct SightInfo {
    const char* name;       // HUD label
    std::string_view item;  // items.tsv key ("" for built-in sights)
    float magnification;    // 0: the gun's own aim-down-sights zoom
    bool scope;             // aiming looks through a tube (the gun model is not drawn)
};
const SightInfo& sight_info(Sight sight);
bool sight_from_item(std::string_view key, Sight& out);
// Pistols take a red dot, shotguns a red dot or 2x, rifles and SMGs any optic.
bool sight_fits(WeaponId weapon, Sight sight);
// Vertical field of view while aiming through `sight` on `weapon` (degrees).
float sight_fov(WeaponId weapon, Sight sight);
// Bore axis up to the sight line, metres in the gun's frame.
float sight_height(WeaponId weapon, Sight sight);

class SightMounts {
public:
    using Owned = std::function<int(Sight)>;   // copies of an optic in the bag
    using HasGun = std::function<bool(WeaponId)>;
    [[nodiscard]] Sight mounted(WeaponId weapon) const;
    void set(WeaponId weapon, Sight sight) { mounts_[static_cast<std::size_t>(weapon)] = sight; }
    void clear() { mounts_.fill(Sight::Iron); }
    // Copies of `sight` not already on a gun other than `except`.
    [[nodiscard]] int free(Sight sight, const Owned& owned, WeaponId except) const;
    // Next optic for the held gun (iron, red dot, 2x, 4x, round again), skipping what does not
    // fit or is already on another gun. Returns false when nothing else is available.
    bool cycle(WeaponId weapon, const Owned& owned);
    // A newly found optic goes on the held gun if it still wears irons, else on another gun
    // that does, as battle royales attach a pickup to an empty slot.
    bool acquire(Sight sight, WeaponId held, const HasGun& has, const Owned& owned);
    // Take optics off guns when the bag no longer holds enough of them (sold or dropped).
    bool validate(const Owned& owned);
private:
    std::array<Sight,weapon_count> mounts_{};
};

} // namespace outland::game::combat
