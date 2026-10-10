#include "outland/game/combat/WeaponSystem.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/assets/VerdaGeometry.hpp"
#include <cassert>
#include <vector>
#include <cmath>
#include <iostream>

// Only fixture construction/access is needed; no renderer or graphics linkage.
namespace outland::world {
VerdaRegion::VerdaRegion() = default;
const std::vector<Settlement>& VerdaRegion::settlements() const { return settlements_; }
}

int main() {
    using namespace outland::game::combat;
    using namespace outland::world;
    VerdaRegion region;
    CombatWorld world(region);
    WeaponSystem gun;
    const ShotPose high{{70,100,70},{0,0,1}};
    WeaponInput input;input.aim=true;

    // Fast rounds sweep through a target instead of teleporting past it.
    const auto target=world.targets()[6].center; // X=8
    auto hit=world.trace_segment({8,target.y,-12},{8,target.y,-40});
    assert(hit.kind==HitKind::Target && hit.target==6 && !hit.headshot && hit.fraction<1);
    hit=world.trace_segment({8,target.y+.75F,-12},{8,target.y+.75F,-40});
    assert(hit.headshot && hit.target==6);
    const float pad=terrain::TerrainHeight::sample(0,0);
    hit=world.trace_segment({0,pad+1.5F,8},{0,pad+1.5F,-30});
    assert(hit.kind==HitKind::Structure); // Cover wins over the target behind it.
    const float floor=terrain::TerrainHeight::sample(70,70);
    hit=world.trace_segment({70,floor+2,70},{70,floor-2,70});
    assert(hit.kind==HitKind::Ground && std::abs(hit.fraction-.5F)<.001F);

    // Rotated walls and pitched roof are actual bullet cover.
    auto& settlements=const_cast<std::vector<Settlement>&>(region.settlements());
    settlements.emplace_back();
    Building building;building.position={60,0,0};building.size={8,4,2};building.rotation_y=90;
    settlements[0].buildings.push_back(building);
    const float y=terrain::TerrainHeight::sample(60,0);
    // Local front-wall x = -world dz here: x=1.2 is solid wall between the doorway and a window.
    hit=world.trace_segment({56,y+2,-1.2F},{64,y+2,-1.2F});
    assert(hit.kind==HitKind::Building && hit.position.x<60);
    // The doorway (x=0) and the window openings (x=+-2.32, 1.1-2.4 m) let bullets and sight through
    // to the back wall; the window sill below them does not.
    for(const float z:{0.0F,-2.32F,2.32F}) {
        hit=world.trace_segment({56,y+2,z},{64,y+2,z});
        assert(hit.kind==HitKind::Building && hit.position.x>60.5F);
    }
    hit=world.trace_segment({56,y+.8F,-2.32F},{64,y+.8F,-2.32F});
    assert(hit.kind==HitKind::Building && hit.position.x<60);
    hit=world.trace_segment({60,y+10,0},{60,y+4,0});
    assert(hit.kind==HitKind::Building && std::abs(hit.position.y-(y+6.2F))<.001F);
    settlements.clear();

    // Projectile travel takes time; body and head damage differ.
    input.fire=true;
    gun.update(.001F,input,{{8,target.y,-12},{0,0,-1}},world);
    assert(gun.events().shots==1 && world.targets()[6].health==100);
    gun.update(.02F,input,{{8,target.y,-12},{0,0,-1}},world);
    assert(world.targets()[6].health<100 && world.targets()[6].health>55); // 41 damage per rifle round
    assert(gun.hit_marker()>0 && !gun.last_headshot());
    const float body_damage=gun.last_damage();
    gun.reset(false);world.reset_targets();
    gun.update(.02F,input,{{8,target.y+.75F,-12},{0,0,-1}},world);
    assert(gun.last_headshot() && gun.last_damage()>body_damage*2.2F); // the rifle's 2.35x head multiplier

    // Automatic fire has a consistent cadence and finite magazines.
    gun.reset(false);
    int fired=0;
    for(int i=0;i<60;++i) { gun.update(.05F,input,high,world);fired+=gun.events().shots; }
    assert(fired==30 && gun.ammo().loaded==0 && gun.ammo().reserve==120);
    for(float frame_time : {.01F,.1F}) {
        WeaponSystem cadence;
        int count=0;
        for(int i=0;i<static_cast<int>(std::round(3.0F/frame_time));++i) {
            cadence.update(frame_time,input,high,world);
            count+=cadence.events().shots;
        }
        assert(count==30);
    }
    gun.update(.05F,input,high,world);assert(gun.events().shots==0);
    input.fire=false;gun.update(.05F,input,high,world);
    input.fire=true;gun.update(.05F,input,high,world);assert(gun.events().dry_fire);
    input.reload=true;
    gun.update(.05F,input,high,world);assert(gun.events().reload_started && gun.events().shots==0);
    input.reload=false;input.fire=false;
    for(int i=0;i<45;++i) gun.update(.05F,input,high,world);
    assert(gun.ammo().loaded==30 && gun.ammo().reserve==90 && gun.reload_remaining()==0);
    assert(gun.collect_ammo()==30 && gun.ammo().reserve==120);
    assert(gun.collect_ammo()==0 && gun.ammo().reserve==120);
    input.reload=true;gun.update(.05F,input,high,world);
    assert(!gun.events().reload_started);input.reload=false;

    // Pistol is semiautomatic, even if fire is held for several seconds.
    input.next_weapon=true;gun.update(.05F,input,high,world);
    assert(gun.selected()==WeaponId::Pistol);input.next_weapon=false;
    for(int i=0;i<6;++i)gun.update(.05F,input,high,world);
    input.fire=true;gun.update(.05F,input,high,world);assert(gun.events().shots==1);
    fired=0;
    for(int i=0;i<80;++i) { gun.update(.05F,input,high,world);fired+=gun.events().shots; }
    assert(fired==0 && gun.ammo().loaded==14); // 15-round pistol magazine
    input.fire=false;gun.update(.05F,input,high,world);
    input.fire=true;gun.update(.05F,input,high,world);assert(gun.events().shots==1);
    input.fire=false;input.reload=true;gun.update(.05F,input,high,world);
    assert(gun.reload_remaining()>0);input.reload=false;input.next_weapon=true;
    gun.update(.05F,input,high,world);assert(gun.reload_remaining()==0 && gun.selected()==WeaponId::Rifle);

    // Gravity changes position, and sprinting cannot produce a shot.
    gun.reset(false);input={};input.fire=true;input.aim=true;input.sprint=true;
    gun.update(.05F,input,high,world);assert(gun.events().shots==0 && gun.ammo().loaded==30);
    input.sprint=false;gun.update(.05F,input,high,world);
    const auto initial=gun.bullets()[0];
    assert(initial.active);
    input.fire=false;for(int i=0;i<4;++i)gun.update(.05F,input,high,world);
    const auto falling=gun.bullets()[0];
    assert(falling.active && falling.velocity.y<initial.velocity.y-1.8F);
    assert(initial.position.y+initial.velocity.y*.2F-falling.position.y>.18F);

    // Dev Lab can fire hundreds of rounds without depletion or forced reloads.
    gun.reset(true);assert(gun.collect_ammo()==0);input={};input.fire=true;input.aim=true;
    fired=0;
    for(int i=0;i<600;++i) { gun.update(.05F,input,high,world);fired+=gun.events().shots; }
    // 30 s at the rifle's 0.086 s cycle (fired on the 5 ms simulation step that follows it).
    const float cycle=definition(WeaponId::Rifle).interval;
    assert(fired>=static_cast<int>(30/(cycle+.005F)) && fired<=static_cast<int>(30/cycle)+1 && gun.ammo().loaded==30 && gun.ammo().reserve==120);
    input.reload=true;gun.update(.05F,input,high,world);
    assert(gun.reload_remaining()==0 && !gun.events().reload_started);
    int active=0;for(const auto& bullet:gun.bullets())if(bullet.active)++active;
    assert(active<96);
    gun.reset(false);assert(!gun.unlimited() && gun.ammo().loaded==30);

    // Battle-royale gunplay: recoil climbs through a spray, crouching steadies it, a tap of the
    // fire-mode switch makes the rifle fire single shots, and aimed first shots are tighter.
    {
        WeaponSystem spray;spray.reset(true);WeaponInput hold;hold.fire=true;hold.aim=true;
        std::vector<float> kicks;
        for(int i=0;i<120 && kicks.size()<12;++i) {spray.update(.02F,hold,high,world);if(spray.events().shots) kicks.push_back(spray.events().pitch_kick);}
        assert(kicks.size()==12 && kicks.back()>kicks.front()*1.5F);
        WeaponSystem steady;steady.reset(true);WeaponInput crouched=hold;crouched.crouched=true;
        steady.update(.02F,crouched,high,world);
        assert(steady.events().shots==1 && steady.events().pitch_kick<kicks.front()*.85F);
        WeaponSystem single;single.reset(true);WeaponInput toggle;toggle.fire_mode=true;
        single.update(.02F,toggle,high,world);assert(single.single_fire());
        int shots=0;for(int i=0;i<50;++i){single.update(.02F,hold,high,world);shots+=single.events().shots;}
        assert(shots==1);
        const auto& rifle=definition(WeaponId::Rifle);
        assert(rifle.aimed_spread<rifle.hip_spread*.1F && rifle.head_multiplier>2.3F && rifle.limb_multiplier<1);
        assert(rifle.ads_fov<60 && definition(WeaponId::Pistol).ads_fov>rifle.ads_fov);
    }

    world.damage_target(6,150);assert(world.targets()[6].health==0);
    world.update(10,false);assert(world.targets()[6].health==0);
    world.update(4.1F,true);assert(world.targets()[6].health==100);
    std::cout<<"[PASS] Swept collision, nearest cover, roofs/rotated walls, drop/travel time, damage/headshots, firing cadence, magazine/reload/switching, sprint lockout, unlimited Dev ammo and target reset\n";
}
