#include "outland/game/combat/WeaponSystem.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/assets/VerdaGeometry.hpp"
#include <cassert>
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
    assert(world.targets()[6].health<100 && world.targets()[6].health>65);
    assert(gun.hit_marker()>0 && !gun.last_headshot());
    const float body_damage=gun.last_damage();
    gun.reset(false);world.reset_targets();
    gun.update(.02F,input,{{8,target.y+.75F,-12},{0,0,-1}},world);
    assert(gun.last_headshot() && gun.last_damage()>body_damage*1.9F);

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
    assert(fired==0 && gun.ammo().loaded==11);
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
    assert(fired==300 && gun.ammo().loaded==30 && gun.ammo().reserve==120);
    input.reload=true;gun.update(.05F,input,high,world);
    assert(gun.reload_remaining()==0 && !gun.events().reload_started);
    int active=0;for(const auto& bullet:gun.bullets())if(bullet.active)++active;
    assert(active<96);
    gun.reset(false);assert(!gun.unlimited() && gun.ammo().loaded==30);

    world.damage_target(6,150);assert(world.targets()[6].health==0);
    world.update(10,false);assert(world.targets()[6].health==0);
    world.update(4.1F,true);assert(world.targets()[6].health==100);
    std::cout<<"[PASS] Swept collision, nearest cover, roofs/rotated walls, drop/travel time, damage/headshots, firing cadence, magazine/reload/switching, sprint lockout, unlimited Dev ammo and target reset\n";
}
