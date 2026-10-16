#include "outland/game/combat/Sights.hpp"
#include <cmath>

namespace outland::game::combat {

namespace {
constexpr float degree=3.14159265F/180.0F;
constexpr std::array<SightInfo,5> infos{{
    {"IRON SIGHTS", "", 0, false},
    {"RED DOT", "red_dot_sight", 0, false},
    {"2X SCOPE", "scope_2x", 2, true},
    {"4X SCOPE", "scope_4x", 4, true},
    {"SNIPER SCOPE", "", 0, true},
}};
// Top of the rail (or slide) each optic sits on, and the iron sight line.
float rail(WeaponId weapon) {
    switch(weapon) {
        case WeaponId::Pistol: return .023F;
        case WeaponId::Smg: return .041F;
        case WeaponId::Shotgun: case WeaponId::DoubleBarrel: return .030F;
        case WeaponId::AutoShotgun: return .046F;
        case WeaponId::Rocket: return .058F;
        default: return .037F;
    }
}
float iron(WeaponId weapon) {
    switch(weapon) {
        case WeaponId::Pistol: return .029F;
        case WeaponId::Smg: return .058F;
        case WeaponId::Shotgun: return .036F;
        case WeaponId::DoubleBarrel: return .030F;
        case WeaponId::AutoShotgun: return .066F;
        case WeaponId::Rocket: return .078F;
        case WeaponId::Sniper: return .06F;
        default: return .062F;
    }
}
}

const SightInfo& sight_info(Sight sight) { return infos[static_cast<std::size_t>(sight)]; }

bool sight_from_item(std::string_view key, Sight& out) {
    for(const Sight sight:attachments)
        if(sight_info(sight).item==key) { out=sight; return true; }
    return false;
}

bool sight_fits(WeaponId weapon, Sight sight) {
    if(sight==Sight::Iron) return weapon!=WeaponId::Sniper;
    if(sight==Sight::Sniper) return weapon==WeaponId::Sniper;
    switch(weapon) {
        case WeaponId::Pistol: return sight==Sight::RedDot;
        case WeaponId::Shotgun: case WeaponId::AutoShotgun: case WeaponId::Rocket: return sight!=Sight::Scope4x;
        case WeaponId::DoubleBarrel: return sight==Sight::RedDot;
        case WeaponId::Sniper: return false;
        default: return true;
    }
}

float sight_fov(WeaponId weapon, Sight sight) {
    const float magnification=sight_info(sight).magnification;
    if(magnification<=0) return definition(weapon).ads_fov;
    // The same view as the 70-degree hip camera, magnified.
    return 2.0F*std::atan(std::tan(35.0F*degree)/magnification)/degree;
}

float sight_height(WeaponId weapon, Sight sight) {
    switch(sight) {
        case Sight::RedDot: return rail(weapon)+(weapon==WeaponId::Pistol ? .016F : .026F);
        case Sight::Scope2x: return rail(weapon)+.032F;
        case Sight::Scope4x: return rail(weapon)+.036F;
        default: return iron(weapon);
    }
}

Sight SightMounts::mounted(WeaponId weapon) const {
    if(weapon==WeaponId::Sniper) return Sight::Sniper;
    return mounts_[static_cast<std::size_t>(weapon)];
}

int SightMounts::free(Sight sight, const Owned& owned, WeaponId except) const {
    int left=owned(sight);
    for(std::size_t i=0;i<weapon_count;++i)
        if(static_cast<WeaponId>(i)!=except && mounts_[i]==sight) --left;
    return left;
}

bool SightMounts::cycle(WeaponId weapon, const Owned& owned) {
    if(weapon==WeaponId::Sniper) return false;
    constexpr std::array<Sight,4> order{Sight::Iron, Sight::RedDot, Sight::Scope2x, Sight::Scope4x};
    const auto now=mounted(weapon);
    std::size_t at=0;
    while(order[at]!=now) ++at;
    for(std::size_t step=1;step<order.size();++step) {
        const Sight next=order[(at+step)%order.size()];
        if(!sight_fits(weapon,next) || (next!=Sight::Iron && free(next,owned,weapon)<=0)) continue;
        set(weapon,next);
        return true;
    }
    return false;
}

bool SightMounts::acquire(Sight sight, WeaponId held, const HasGun& has, const Owned& owned) {
    const auto try_mount=[&](WeaponId weapon) {
        if(!has(weapon) || mounted(weapon)!=Sight::Iron || !sight_fits(weapon,sight) || free(sight,owned,weapon)<=0) return false;
        set(weapon,sight);
        return true;
    };
    if(try_mount(held)) return true;
    for(std::size_t i=0;i<weapon_count;++i)
        if(try_mount(static_cast<WeaponId>(i))) return true;
    return false;
}

bool SightMounts::validate(const Owned& owned) {
    bool changed=false;
    for(const Sight sight:attachments) {
        int left=owned(sight);
        for(auto& mount:mounts_)
            if(mount==sight && left--<=0) { mount=Sight::Iron; changed=true; }
    }
    return changed;
}

} // namespace outland::game::combat
