#include "outland/game/combat/WeaponSystem.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/assets/VerdaGeometry.hpp"
#include <algorithm>
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
    assert(gun.select(WeaponId::Pistol) && gun.selected()==WeaponId::Pistol);
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
    // SWITCH WEAPON goes through the guns, then to empty hands, then back to the first gun.
    input.next_weapon=false;
    assert(gun.select(WeaponId::Rocket) && !gun.holstered());
    gun.update(.05F,input,high,world);   // (the switch cooldown)
    input.next_weapon=true;gun.update(.05F,input,high,world);
    assert(gun.holstered() && !gun.available());
    input.next_weapon=false;input.fire=true;
    for(int i=0;i<10;++i) {gun.update(.05F,input,high,world);assert(gun.events().shots==0);}   // nothing to fire
    input.fire=false;input.next_weapon=true;gun.update(.05F,input,high,world);
    assert(!gun.holstered() && gun.available() && gun.selected()==WeaponId::Pistol);
    // Putting the gun in hand away (the bag's EQUIP on it), and drawing one again.
    gun.holster();assert(!gun.available());
    assert(gun.select(WeaponId::Rifle) && gun.available());
    input.next_weapon=false;

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

    // The wider armoury: an SMG out-fires the rifle, a shotgun throws a pellet cone, the bolt-action
    // sniper waits on its bolt, and each weapon has its own item key and hold.
    {
        WeaponSystem armoury;armoury.reset(true);WeaponInput hold;hold.fire=true;hold.aim=true;
        const auto count=[&](WeaponId id,float seconds) {
            armoury.reset(true);assert(armoury.select(id));
            for(int i=0;i<10;++i){WeaponInput idle;armoury.update(.05F,idle,high,world);}
            int shots=0;for(float t=0;t<seconds;t+=.02F){armoury.update(.02F,hold,high,world);shots+=armoury.events().shots;}
            return shots;
        };
        assert(count(WeaponId::Smg,1.5F)>count(WeaponId::Rifle,1.5F));
        assert(count(WeaponId::Sniper,3)==1);   // bolt-action: holding the trigger never fires again
        armoury.reset(true);assert(armoury.select(WeaponId::Shotgun));
        for(int i=0;i<10;++i){WeaponInput idle;armoury.update(.05F,idle,high,world);}
        armoury.update(.001F,hold,high,world);
        assert(armoury.events().shots==1);
        std::vector<Vector3> pellets;
        for(const auto& bullet:armoury.bullets()) if(bullet.active) pellets.push_back(bullet.velocity);
        assert(pellets.size()==9);
        float widest=0;
        for(const auto& v:pellets) for(const auto& w:pellets) {
            const float c=(v.x*w.x+v.y*w.y+v.z*w.z)/std::sqrt((v.x*v.x+v.y*v.y+v.z*v.z)*(w.x*w.x+w.y*w.y+w.z*w.z));
            widest=std::max(widest,std::acos(std::min(1.0F,c)));
        }
        assert(widest>.02F && widest<.12F);
        // The other shotguns: a double barrel empties both barrels in a moment (twelve pellets each);
        // the drum-fed automatic out-fires the pump; all three load the same shells.
        assert(count(WeaponId::AutoShotgun,1.5F)>count(WeaponId::Shotgun,1.5F)+3);
        armoury.reset(true);assert(armoury.select(WeaponId::DoubleBarrel));
        for(int i=0;i<10;++i){WeaponInput idle;armoury.update(.05F,idle,high,world);}
        armoury.update(.001F,hold,high,world);
        assert(armoury.events().shots==1 && std::count_if(armoury.bullets().begin(),armoury.bullets().end(),[](const auto& b){return b.active;})==12);
        assert(definition(WeaponId::DoubleBarrel).magazine==2 && definition(WeaponId::DoubleBarrel).interval<.3F);
        assert(ammo_of(WeaponId::DoubleBarrel)==WeaponId::Shotgun && ammo_of(WeaponId::AutoShotgun)==WeaponId::Shotgun && ammo_of(WeaponId::Rocket)==WeaponId::Rocket);
        // The rocket launcher fires no bullets: it hands a rocket to the caller to fly.
        armoury.reset(true);assert(armoury.select(WeaponId::Rocket));
        for(int i=0;i<10;++i){WeaponInput idle;armoury.update(.05F,idle,high,world);}
        armoury.update(.001F,hold,high,world);
        assert(armoury.events().shots==1 && armoury.events().launches.size()==1);
        assert(std::none_of(armoury.bullets().begin(),armoury.bullets().end(),[](const auto& b){return b.active;}));
        const auto& launched=armoury.events().launches.front().velocity;
        assert(std::abs(std::sqrt(launched.x*launched.x+launched.y*launched.y+launched.z*launched.z)-definition(WeaponId::Rocket).muzzle_speed)<.01F);
        const auto& sniper=definition(WeaponId::Sniper);
        assert(sniper.damage*sniper.head_multiplier>=200 && sniper.ads_fov<20);
        for(std::size_t i=0;i<weapon_count;++i) {
            WeaponId id{};assert(weapon_from_item(weapons[i].item,id) && id==static_cast<WeaponId>(i));
            assert(weapons[i].length>.08F && weapons[i].length<.9F);
        }
        assert(definition(WeaponId::Pistol).hold==WeaponHold::Pistol && definition(WeaponId::Sniper).hold==WeaponHold::Shouldered);
    }

    // Extra drill targets (the police training camp): traced as targets from drill_base, a bullet's
    // hit is passed on with whether it was a head shot.
    {
        int drilled=-1;bool head=false;
        world.bind_drill([](Vector3 a,Vector3 b) {
            BulletHit hit;
            if(a.z<500 && b.z>500 && std::abs(a.x-1000)<1) {hit.kind=HitKind::Target;hit.fraction=(500-a.z)/(b.z-a.z);hit.position={1000,a.y,500};hit.target=2;hit.headshot=a.y>50;}
            return hit;
        },[&](int target,bool headshot){drilled=target;head=headshot;});
        const float y=outland::world::terrain::TerrainHeight::sample(1000,495)+60;
        const auto hit=world.trace_segment({1000,y,495},{1000,y,505});
        assert(hit.kind==HitKind::Target && hit.target==CombatWorld::drill_base+2);
        world.damage_hit(hit,30,{1000,y,495});
        assert(drilled==2 && head);
        world.bind_drill({},{});
    }
    world.damage_target(6,150);assert(world.targets()[6].health==0);
    world.update(10,false);assert(world.targets()[6].health==0);
    world.update(4.1F,true);assert(world.targets()[6].health==100);
    std::cout<<"[PASS] Swept collision, nearest cover, roofs/rotated walls, drop/travel time, damage/headshots, firing cadence, magazine/reload/switching, sprint lockout, unlimited Dev ammo and target reset\n";
}
