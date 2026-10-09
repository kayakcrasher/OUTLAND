#include "outland/characters/NpcSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/GameplayMarker.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
namespace outland::characters {
NpcSystem::NpcSystem() {
    tuning_[static_cast<std::size_t>(CharacterPool::Emergency)].health=120;
    auto& hostile=tuning_[static_cast<std::size_t>(CharacterPool::Hostile)];
    hostile.movement_speed=2;hostile.detection_radius=25;hostile.attack_damage=15;
    auto& creature=tuning_[static_cast<std::size_t>(CharacterPool::Creature)];
    creature.movement_speed=2.8F;creature.detection_radius=30;creature.reaction_delay=.3F;
    creature.health=140;creature.attack_range=1.8F;creature.attack_damage=18;
}
const NpcTuning& NpcSystem::tuning(CharacterPool pool) const {return tuning_.at(static_cast<std::size_t>(pool));}
void NpcSystem::configure(CharacterPool pool,NpcTuning value) {
    value.sanitize();tuning_.at(static_cast<std::size_t>(pool))=value;
    for(auto& actor:actors_) if(actor.pool==pool) actor.health=std::min(actor.health,value.health);
}
void NpcSystem::reset_session() {
    events_={};player_threat_timer_=0;
    std::erase_if(actors_,[](const NpcInstance& actor){return actor.resident>=0;});
    for(auto& actor:actors_) {
        actor.position=actor.spawn_position;actor.yaw_degrees=actor.spawn_yaw;
        actor.health=tuning(actor.pool).health;actor.state=NpcState::Idle;
        actor.active=actor.visible=false;actor.state_time=actor.decision_clock=actor.reaction_clock=actor.threat_timer=actor.attack_clock=0;
        actor.random_state=character_seed(actor.spawn_key)|1;
        actor.animation={};
    }
}
void NpcSystem::reconcile(const world::VerdaRegion& region,const CharacterRegistry& registry) {
    std::unordered_map<std::string,std::size_t> previous;
    for(std::size_t i=0;i<actors_.size();++i) previous.emplace(actors_[i].spawn_key,i);
    std::vector<NpcInstance> next;
    std::unordered_set<std::string> seen;
    for(auto& actor:actors_) if(actor.resident>=0) next.push_back(std::move(actor));
    for(const auto& settlement:region.settlements()) for(const auto& marker:settlement.gameplay_markers) {
        if(!marker.enabled || marker.type!=world::GameplayMarkerType::NpcSpawn) continue;
        if(!std::isfinite(marker.position.x) || !std::isfinite(marker.position.y) ||
            !std::isfinite(marker.position.z) || !std::isfinite(marker.rotation_y)) continue;
        const std::string key=std::to_string(settlement.id.size())+":"+settlement.id+marker.id;
        if(!seen.insert(key).second) continue;
        const auto pool=npc_pool_for_marker(marker.id);
        const auto* definition=registry.choose(pool,character_seed(key));
        if(!definition) continue;
        NpcInstance actor;
        const auto old=previous.find(key);
        if(old!=previous.end() && actors_[old->second].resident<0 && actors_[old->second].character_id==definition->id) actor=std::move(actors_[old->second]);
        else {
            actor.spawn_key=key;actor.character_id=definition->id;actor.pool=pool;
            actor.health=tuning(pool).health;actor.random_state=character_seed(key)|1;
            actor.position=marker.position;actor.yaw_degrees=marker.rotation_y;
        }
        if(Vector3DistanceSqr(actor.spawn_position,marker.position)>.000001F || actor.spawn_yaw!=marker.rotation_y) {
            actor.position=marker.position;actor.yaw_degrees=marker.rotation_y;actor.waypoint=marker.position;
            if(actor.state!=NpcState::Dead) {actor.state=NpcState::Idle;actor.state_time=0;}
        }
        actor.spawn_position=marker.position;actor.spawn_yaw=marker.rotation_y;
        next.push_back(std::move(actor));
    }
    actors_=std::move(next);
}
void NpcSystem::update(float dt,const NpcContext& input,const world::VerdaRegion& region) {
    events_={};
    if(!std::isfinite(dt) || dt<=0 || !std::isfinite(input.player_position.x) ||
        !std::isfinite(input.player_position.y) || !std::isfinite(input.player_position.z)) return;
    dt=std::min(dt,.25F);
    NpcContext context=input;
    if(context.threatening) player_threat_timer_=1.5F;
    else player_threat_timer_=std::max(0.0F,player_threat_timer_-dt);
    context.threatening=player_threat_timer_>0;
    NpcEnvironment environment;
    environment.move=[&](Vector3 current,Vector3 desired) {
        // Small collision substeps prevent fast creatures tunnelling through thin walls.
        const int steps=std::max(1,static_cast<int>(std::ceil(Vector3Distance(current,desired)/.2F)));
        const auto origin=current;
        for(int i=1;i<=steps;++i) {
            const auto target=Vector3Lerp(origin,desired,static_cast<float>(i)/steps);
            current=world::physics::WorldCollision::resolve_body_movement(current,target,current.y,region,.35F);
        }
        current.y=world::physics::WorldCollision::ground_height(current,current.y,region);
        return current;
    };
    for(auto& actor:actors_) {
        const auto& settings=tuning(actor.pool);
        const float distance_sq=Vector3DistanceSqr(actor.position,context.player_position);
        actor.visible=distance_sq<=settings.despawn_distance*settings.despawn_distance;
        // Residents exist physically only while game::life keeps them near the player.
        if(actor.resident>=0) actor.active=true;
        else if(!actor.visible) actor.active=false;
        else if(!actor.active && distance_sq<=settings.activation_distance*settings.activation_distance) actor.active=true;
        if(context.paused || !actor.active) {actor.decision_clock=0;continue;}
        if(actor.state!=NpcState::Dead) {
            actor.decision_clock+=dt;
            constexpr float interval=.1F; // 10Hz decisions; no catch-up burst for sleeping actors.
            while(actor.decision_clock>=interval) {
                actor.decision_clock-=interval;
                const float damage=NpcBehavior::tick(actor,settings,interval,context,environment);
                if(damage>0) {events_.player_damage+=damage;++events_.attacks;}
            }
        }
        actor.animation.advance(NpcBehavior::animation(actor.state),dt);
    }
}
bool NpcSystem::damage(std::size_t index,float amount,Vector3 attacker) {
    if(index>=actors_.size() || !std::isfinite(amount) || amount<=0 ||
        !std::isfinite(attacker.x) || !std::isfinite(attacker.y) || !std::isfinite(attacker.z)) return false;
    auto& actor=actors_[index];
    if(actor.state==NpcState::Dead || actor.health<=0) return false;
    actor.health=std::max(0.0F,actor.health-amount);
    actor.threat_position=attacker;actor.threat_timer=tuning(actor.pool).memory_seconds;actor.state_time=0;
    actor.state=actor.health==0 ? NpcState::Dead :
        (actor.pool==CharacterPool::Hostile || actor.pool==CharacterPool::Creature ? NpcState::Chase : NpcState::Flee);
    actor.animation.advance(NpcBehavior::animation(actor.state),0);
    return true;
}
NpcHit NpcSystem::trace_segment(Vector3 start,Vector3 end) const {
    NpcHit best;
    if(!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(start.z) ||
        !std::isfinite(end.x) || !std::isfinite(end.y) || !std::isfinite(end.z)) return best;
    const auto delta=Vector3Subtract(end,start);
    for(std::size_t i=0;i<actors_.size();++i) {
        const auto& actor=actors_[i];
        if(!actor.visible || actor.health<=0 || actor.state==NpcState::Dead) continue;
        const auto p=actor.position;
        const float lo[3]{p.x-.35F,p.y,p.z-.35F}, hi[3]{p.x+.35F,p.y+1.85F,p.z+.35F};
        const float origin[3]{start.x,start.y,start.z}, direction[3]{delta.x,delta.y,delta.z};
        float enter=0,exit=1;Vector3 normal{0,1,0};bool hit=true;
        for(int axis=0;axis<3;++axis) {
            if(std::abs(direction[axis])<.000001F) {if(origin[axis]<lo[axis] || origin[axis]>hi[axis]) hit=false;continue;}
            float a=(lo[axis]-origin[axis])/direction[axis], b=(hi[axis]-origin[axis])/direction[axis];
            if(a>b) std::swap(a,b);
            if(a>enter) {enter=a;normal={0,0,0};if(axis==0) normal.x=direction[axis]>0 ? -1.0F:1.0F;
                if(axis==1) normal.y=direction[axis]>0 ? -1.0F:1.0F;
                if(axis==2) normal.z=direction[axis]>0 ? -1.0F:1.0F;}
            exit=std::min(exit,b);if(enter>exit) hit=false;
        }
        if(hit && enter>=0 && enter<=1 && (best.actor<0 || enter<best.fraction)) {
            const auto point=Vector3Add(start,Vector3Scale(delta,enter));
            best={static_cast<int>(i),enter,point,normal,point.y>p.y+1.45F};
        }
    }
    return best;
}
std::size_t NpcSystem::spawn_resident(const ResidentSpawn& spawn) {
    if(const int existing=find_resident(spawn.resident);existing>=0) return static_cast<std::size_t>(existing);
    NpcInstance actor;
    actor.spawn_key="resident:"+std::to_string(spawn.resident);actor.character_id=spawn.character_id;
    actor.pool=spawn.pool;actor.resident=spawn.resident;actor.directed=true;
    actor.position=spawn.position;actor.spawn_position=spawn.anchor;actor.waypoint=spawn.anchor;
    actor.yaw_degrees=actor.spawn_yaw=spawn.yaw_degrees;
    actor.health=std::isfinite(spawn.health) ? std::clamp(spawn.health,0.0F,tuning(spawn.pool).health) : tuning(spawn.pool).health;
    actor.anchor_radius=std::isfinite(spawn.anchor_radius) ? std::clamp(spawn.anchor_radius,.25F,60.0F) : 2;
    actor.random_state=character_seed(actor.spawn_key)|1;
    actor.state=actor.health<=0 ? NpcState::Dead : NpcState::Idle;actor.state_time=1;
    actors_.push_back(std::move(actor));
    return actors_.size()-1;
}
bool NpcSystem::despawn_resident(int resident) {
    const int index=find_resident(resident);
    if(index<0) return false;
    actors_.erase(actors_.begin()+index);
    return true;
}
int NpcSystem::find_resident(int resident) const {
    if(resident<0) return -1;
    for(std::size_t i=0;i<actors_.size();++i) if(actors_[i].resident==resident) return static_cast<int>(i);
    return -1;
}
void NpcSystem::direct_resident(int resident,Vector3 anchor,float radius,bool hurry) {
    const int index=find_resident(resident);
    if(index<0 || !std::isfinite(anchor.x) || !std::isfinite(anchor.z) || !std::isfinite(radius)) return;
    auto& actor=actors_[static_cast<std::size_t>(index)];
    actor.hurry=hurry;actor.anchor_radius=std::clamp(radius,.25F,60.0F);
    if(Vector3DistanceSqr(actor.spawn_position,anchor)<.25F) return;
    actor.spawn_position=anchor;
    // Only everyday states follow the schedule; fear and combat keep control until they settle.
    if(actor.state==NpcState::Idle || actor.state==NpcState::Wander) {actor.state=NpcState::Idle;actor.state_time=actor.idle_hold=1;}
}
}
