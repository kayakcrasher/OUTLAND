#include "outland/game/combat/WeaponSystem.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game::combat {
namespace {
Vector3 add(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 sub(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 mul(Vector3 v,float s) { return {v.x*s,v.y*s,v.z*s}; }
float length(Vector3 v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
Vector3 normalized(Vector3 v) { const float l=length(v);return l>.000001F ? mul(v,1/l) : Vector3{0,0,-1}; }
Vector3 cross(Vector3 a,Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
}

WeaponSystem::WeaponSystem() { reset(false); }
void WeaponSystem::bind_inventory(std::function<bool(WeaponId)> owned,
                                  std::function<int(WeaponId)> reserve,
                                  std::function<int(WeaponId, int)> consume) {
    if (!owned || !reserve || !consume) {
        unbind_inventory();
        return;
    }
    owned_ = std::move(owned);
    reserve_ = std::move(reserve);
    consume_ = std::move(consume);
    refresh_reserves();
}
void WeaponSystem::unbind_inventory() {
    owned_ = {};
    reserve_ = {};
    consume_ = {};
}
bool WeaponSystem::available() const { return !holstered_ && (!owned_ || owned_(selected_)); }
void WeaponSystem::holster() {
    holstered_ = true;
    reload_remaining_ = 0;
    spray_ = 0;
}
void WeaponSystem::refresh_reserves() {
    if (reserve_)
        for (std::size_t i = 0; i < ammo_.size(); ++i)
            ammo_[i].reserve = std::max(0, reserve_(static_cast<WeaponId>(i)));
}
bool WeaponSystem::select(WeaponId id) {
    if (static_cast<std::size_t>(id) >= ammo_.size() || (owned_ && !owned_(id)))
        return false;
    selected_ = id;
    holstered_ = false;
    reload_remaining_ = 0;
    cooldown_ = std::max(cooldown_, .2F);
    return true;
}
bool WeaponSystem::set_loaded(WeaponId id, int rounds) {
    const auto index = static_cast<std::size_t>(id);
    if (index >= ammo_.size() || rounds < 0 || rounds > definition(id).magazine)
        return false;
    ammo_[index].loaded = rounds;
    if (id == selected_)
        reload_remaining_ = 0;
    return true;
}
void WeaponSystem::request_reload() {
    refresh_reserves();
    start_reload();
}
int WeaponSystem::collect_ammo() {
    if (unlimited_ || reserve_) return 0;
    auto& reserve = ammo_[static_cast<std::size_t>(selected_)].reserve;
    const int added = std::max(0, std::min(weapon().magazine, weapon().reserve - reserve));
    reserve += added;
    return added;
}
void WeaponSystem::reset(bool unlimited) {
    unbind_inventory();
    unlimited_=unlimited;selected_=WeaponId::Rifle;holstered_=false;previous_fire_=false;single_={};spray_=0;since_shot_=10;
    cooldown_=reload_remaining_=muzzle_flash_=hit_marker_=last_damage_=0;
    last_headshot_=false;shot_number_=impact_index_=0;random_state_=0x51f7349aU;
    events_={};bullets_={};impacts_={};
    for(std::size_t i=0;i<weapons.size();++i) ammo_[i]={weapons[i].magazine,weapons[i].reserve};
}
float WeaponSystem::random() {
    random_state_^=random_state_<<13;random_state_^=random_state_>>17;random_state_^=random_state_<<5;
    return static_cast<float>(random_state_>>8)/16777216.0F;
}
void WeaponSystem::start_reload() {
    if(!available() || unlimited_ || reload_remaining_>0 || ammo().loaded==weapon().magazine || ammo().reserve==0) return;
    reload_remaining_=weapon().reload_seconds;events_.reload_started=true;
}
bool WeaponSystem::shoot(const WeaponInput& input, ShotPose pose) {
    const auto& def=weapon();
    const int pellets=def.launcher ? 0 : std::max(1,def.pellets);
    if(std::count_if(bullets_.begin(),bullets_.end(),[](const Bullet& b){return !b.active;})<pellets)return false;
    const auto forward=normalized(pose.direction);
    const auto right=normalized(cross(std::abs(forward.y)>.95F ? Vector3{1,0,0} : Vector3{0,1,0},forward));
    const auto up=normalized(cross(forward,right));
    // A burst: shots fired close together. Aimed first shots are near perfect; hip fire blooms.
    if(since_shot_>def.interval*2.5F) spray_=0;
    const float burst=static_cast<float>(std::min(spray_,10));
    const float movement=std::clamp(input.movement,0.0F,1.0F);
    const float spread=(input.aim ? def.aimed_spread*(spray_==0 ? .3F : 1+burst*.12F) : def.hip_spread*(1+burst*def.bloom))*
        (1+movement*(input.aim ? 1.5F : 2.5F))*(input.grounded ? 1 : 3)*(input.crouched ? .8F : 1);
    const float radius=std::sqrt(random())*spread, angle=random()*2*PI;
    const auto aim=normalized(add(forward,add(mul(right,std::cos(angle)*radius),mul(up,std::sin(angle)*radius))));
    const bool tracer=(shot_number_++%3)==0;
    if(def.launcher) events_.launches.push_back({pose.origin,mul(aim,def.muzzle_speed)});
    for(int pellet=0;pellet<pellets;++pellet) {
        // Shotgun pellets spread in a fixed cone round the aim point, whatever the stance.
        auto direction=aim;
        if(def.pattern>0) {
            const float r=std::sqrt(random())*def.pattern, a=random()*2*PI;
            direction=normalized(add(aim,add(mul(right,std::cos(a)*r),mul(up,std::sin(a)*r))));
        }
        auto slot=std::find_if(bullets_.begin(),bullets_.end(),[](const Bullet& b){return !b.active;});
        *slot={true,tracer && pellet==0,pose.origin,pose.origin,mul(direction,def.muzzle_speed),
               0,0,def.damage,def.muzzle_speed,def.drag,def.head_multiplier,def.limb_multiplier};
    }
    if(!unlimited_) --ammo_[static_cast<std::size_t>(selected_)].loaded;
    cooldown_=def.interval;muzzle_flash_=.035F;
    // Recoil climbs through the burst; crouching steadies it, moving and jumping make it worse.
    const float stance=(input.crouched ? .8F : 1)*(input.aim ? .9F : 1)*(1+movement*.15F)*(input.grounded ? 1 : 1.4F);
    ++events_.shots;events_.pitch_kick+=def.recoil*(1+def.recoil_growth*burst)*stance;
    events_.yaw_kick+=(random()*2-1)*def.recoil_side*(1+burst*.05F)*stance;
    ++spray_;since_shot_=0;
    return true;
}
void WeaponSystem::simulate(float dt, CombatWorld& world) {
    for(auto& bullet:bullets_) {
        if(!bullet.active)continue;
        bullet.previous=bullet.position;
        const auto end=add(bullet.position,add(mul(bullet.velocity,dt),Vector3{0,-4.905F*dt*dt,0}));
        bullet.velocity.y-=9.81F*dt;
        bullet.velocity=mul(bullet.velocity,std::exp(-bullet.drag*dt));
        bullet.age+=dt;bullet.travelled+=length(sub(end,bullet.position));
        Vector3 segment_start=bullet.position;
        bool stopped=false;
        for(int contact=0;contact<8;++contact) {
            const auto hit=world.trace_segment(segment_start,end);
            if(!hit.hit())break;
            impacts_[impact_index_++%impacts_.size()]={hit.position,hit.normal,hit.kind,.4F};
            const float speed=length(bullet.velocity);
            // Body zones: heads take the weapon's multiplier, legs less (zone 2 on people).
            const float zone=hit.headshot ? bullet.head : hit.kind==HitKind::Npc && hit.zone==2 ? bullet.limb : 1.0F;
            const float damage=bullet.damage*std::clamp(speed*speed/(bullet.initial_speed*bullet.initial_speed),.35F,1.0F)*zone;
            world.damage_hit(hit,damage,bullet.previous);
            if(hit.kind==HitKind::Target || hit.kind==HitKind::Npc || hit.kind==HitKind::Vehicle || hit.kind==HitKind::VehicleOccupant) {
                ++events_.target_hits;last_damage_=damage;hit_marker_=.22F;last_headshot_=hit.headshot;
                events_.hits.push_back({hit.position,damage,hit.headshot});
            }
            if(hit.penetration>0 && hit.penetration<1) {
                bullet.damage*=hit.penetration;
                const auto remaining=sub(end,hit.position);const float distance=length(remaining);
                if(distance<.03F){segment_start=end;break;}
                segment_start=add(hit.position,mul(remaining,.03F/distance));
                if(contact==7){stopped=true;bullet.position=hit.position;}
                continue;
            }
            stopped=true;bullet.position=hit.position;break;
        }
        if(stopped)bullet.active=false;
        else {bullet.position=end;if(bullet.age>4.0F || bullet.travelled>900.0F)bullet.active=false;}

    }
}

void WeaponSystem::update(float dt,const WeaponInput& input,ShotPose pose,CombatWorld& world) {
    events_={};
    refresh_reserves();
    dt=std::clamp(dt,0.0F,.1F);
    muzzle_flash_=std::max(0.0F,muzzle_flash_-dt);hit_marker_=std::max(0.0F,hit_marker_-dt);
    for(auto& impact:impacts_)impact.life=std::max(0.0F,impact.life-dt);
    if(input.next_weapon) {
        // The guns in order, then empty hands, then round again from the first gun.
        bool drawn=false;
        const std::size_t from=holstered_ ? 0 : static_cast<std::size_t>(selected_)+1;
        for(std::size_t id=from;id<ammo_.size() && !drawn;++id) drawn=select(static_cast<WeaponId>(id));
        if(!drawn) holster();
    }
    if(input.reload)start_reload();
    if(input.fire_mode && weapon().automatic) single_[static_cast<std::size_t>(selected_)]=!single_[static_cast<std::size_t>(selected_)];
    since_shot_+=dt;
    bool pressed=input.fire && !previous_fire_;
    bool dry_announced=false;
    // Small ballistic timesteps plus swept tests avoid frame-rate dependent tunnelling.
    float remaining=dt;
    while(remaining>.000001F) {
        const float step=std::min(remaining,.005F);
        cooldown_=std::max(0.0F,cooldown_-step);
        if(reload_remaining_>0) {
            reload_remaining_=std::max(0.0F,reload_remaining_-step);
            if(reload_remaining_==0) {
                auto& magazine=ammo_[static_cast<std::size_t>(selected_)];
                const int wanted=std::min(weapon().magazine-magazine.loaded,magazine.reserve);
                const int rounds=consume_ ? std::clamp(consume_(selected_,wanted),0,wanted):wanted;
                magazine.loaded+=rounds;magazine.reserve-=rounds;events_.reload_finished=true;
            }
        }
        const bool trigger=available() && input.fire && (!single_fire() || pressed);
        if(trigger && !input.next_weapon && !input.sprint && reload_remaining_==0 && cooldown_<=.000001F) {
            if(unlimited_ || ammo().loaded>0) {
                if(shoot(input,pose))pressed=false;
            } else if(!dry_announced) {
                events_.dry_fire=pressed;dry_announced=true;
            }
        }
        simulate(step,world);
        remaining-=step;
    }
    previous_fire_=input.fire;
}

} // namespace outland::game::combat
