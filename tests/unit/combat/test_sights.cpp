// Sights: what fits which gun, the zoom each gives, and mounting optics from the bag.
#include "outland/game/combat/Sights.hpp"
#include <cassert>
#include <cmath>
#include <map>

using namespace outland::game::combat;

int main() {
    // Fits: a pistol takes a red dot only, a shotgun no 4x, the sniper keeps its own scope.
    assert(sight_fits(WeaponId::Pistol,Sight::RedDot) && !sight_fits(WeaponId::Pistol,Sight::Scope2x));
    assert(sight_fits(WeaponId::Shotgun,Sight::Scope2x) && !sight_fits(WeaponId::Shotgun,Sight::Scope4x));
    for(const Sight s:attachments) assert(sight_fits(WeaponId::Rifle,s) && sight_fits(WeaponId::Smg,s) && !sight_fits(WeaponId::Sniper,s));
    // Zoom: 2x and 4x magnify the 70-degree view; a red dot keeps the gun's own aim zoom.
    const auto zoom=[](float fov){return std::tan(35.0F*3.14159265F/180)/std::tan(fov*.5F*3.14159265F/180);};
    assert(std::abs(zoom(sight_fov(WeaponId::Rifle,Sight::Scope2x))-2)<.01F);
    assert(std::abs(zoom(sight_fov(WeaponId::Smg,Sight::Scope4x))-4)<.01F);
    assert(sight_fov(WeaponId::Rifle,Sight::RedDot)==definition(WeaponId::Rifle).ads_fov);
    assert(sight_fov(WeaponId::Rifle,Sight::Scope4x)<sight_fov(WeaponId::Rifle,Sight::Scope2x));
    // Optics sit above the rail, a scope higher than a red dot.
    assert(sight_height(WeaponId::Rifle,Sight::Scope4x)>sight_height(WeaponId::Rifle,Sight::RedDot));
    Sight parsed{};
    assert(sight_from_item("scope_4x",parsed) && parsed==Sight::Scope4x && !sight_from_item("rifle_ammo",parsed));
    assert(sight_info(Sight::Scope2x).scope && !sight_info(Sight::RedDot).scope);

    std::map<Sight,int> bag;
    const SightMounts::Owned owned=[&](Sight s){return bag[s];};
    const SightMounts::HasGun guns=[](WeaponId w){return w==WeaponId::Rifle || w==WeaponId::Pistol;};
    SightMounts mounts;
    assert(mounts.mounted(WeaponId::Rifle)==Sight::Iron && mounts.mounted(WeaponId::Sniper)==Sight::Sniper);
    assert(!mounts.cycle(WeaponId::Rifle,owned)); // nothing in the bag: irons only
    // A found red dot goes on the held gun; a second one on the other gun still wearing irons.
    bag[Sight::RedDot]=1;
    assert(mounts.acquire(Sight::RedDot,WeaponId::Rifle,guns,owned) && mounts.mounted(WeaponId::Rifle)==Sight::RedDot);
    bag[Sight::RedDot]=2;
    assert(mounts.acquire(Sight::RedDot,WeaponId::Rifle,guns,owned) && mounts.mounted(WeaponId::Pistol)==Sight::RedDot);
    // A 4x does not fit the held pistol, so it waits for a gun with irons (none): stays in the bag.
    bag[Sight::Scope4x]=1;
    assert(!mounts.acquire(Sight::Scope4x,WeaponId::Pistol,guns,owned));
    // Cycling the rifle: red dot -> 4x (no 2x owned) -> irons -> red dot.
    assert(mounts.cycle(WeaponId::Rifle,owned) && mounts.mounted(WeaponId::Rifle)==Sight::Scope4x);
    assert(mounts.cycle(WeaponId::Rifle,owned) && mounts.mounted(WeaponId::Rifle)==Sight::Iron);
    assert(mounts.cycle(WeaponId::Rifle,owned) && mounts.mounted(WeaponId::Rifle)==Sight::RedDot);
    // One copy, one gun: with one red dot left the pistol keeps it and the rifle skips it.
    bag[Sight::RedDot]=1;
    assert(mounts.validate(owned));
    int on=0;for(const auto w:{WeaponId::Rifle,WeaponId::Pistol}) on+=mounts.mounted(w)==Sight::RedDot;
    assert(on==1 && mounts.free(Sight::RedDot,owned,WeaponId::Smg)==0);
    bag[Sight::RedDot]=0;bag[Sight::Scope4x]=0;
    mounts.validate(owned);
    assert(mounts.mounted(WeaponId::Rifle)==Sight::Iron && mounts.mounted(WeaponId::Pistol)==Sight::Iron);
    assert(!mounts.cycle(WeaponId::Sniper,owned) && mounts.mounted(WeaponId::Sniper)==Sight::Sniper);
}
