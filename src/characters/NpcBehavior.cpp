#include "outland/characters/NpcSystem.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
namespace outland::characters {
void NpcTuning::sanitize() {
    auto bound=[](float value,float low,float high,float fallback) {return std::isfinite(value) ? std::clamp(value,low,high) : fallback;};
    detection_radius=bound(detection_radius,0,500,18);attack_range=bound(attack_range,.1F,10,1.6F);
    movement_speed=bound(movement_speed,0,10,1.5F);run_multiplier=bound(run_multiplier,1,3,1.7F);
    reaction_delay=bound(reaction_delay,0,20,.6F);health=bound(health,1,10000,100);
    activation_distance=bound(activation_distance,1,500,65);
    despawn_distance=bound(despawn_distance,activation_distance,1000,100);
    despawn_distance=std::max(despawn_distance,activation_distance);
    attack_damage=bound(attack_damage,0,1000,12);attack_interval=bound(attack_interval,.1F,30,1);
    wander_radius=bound(wander_radius,0,100,6);memory_seconds=bound(memory_seconds,.1F,30,4);
}
AnimationAction NpcBehavior::animation(NpcState state) {
    switch(state) {
        case NpcState::Wander:return AnimationAction::Walk;
        case NpcState::Chase:case NpcState::Flee:return AnimationAction::Run;
        case NpcState::Attack:return AnimationAction::Attack;
        case NpcState::Dead:return AnimationAction::Death;
        default:return AnimationAction::Idle;
    }
}
namespace {
float random(NpcInstance& actor) {
    actor.random_state^=actor.random_state<<13;actor.random_state^=actor.random_state>>7;actor.random_state^=actor.random_state<<17;
    return static_cast<float>(actor.random_state%10000)/10000.0F;
}
void enter(NpcInstance& actor,NpcState state,const NpcTuning& tuning) {
    if(actor.state==state) return;
    actor.state=state;actor.state_time=0;
    if(state==NpcState::Attack) actor.attack_clock=tuning.attack_interval;
    if(state==NpcState::Idle && actor.directed) actor.idle_hold=1+random(actor)*5;
    if(state==NpcState::Wander) {
        const float limit=actor.directed ? actor.anchor_radius : tuning.wander_radius;
        const float dx=actor.spawn_position.x-actor.position.x, dz=actor.spawn_position.z-actor.position.z;
        // A directed actor far from its schedule anchor travels there instead of milling about.
        actor.travelling=actor.directed && dx*dx+dz*dz>(limit+.75F)*(limit+.75F);
        if(actor.travelling) {actor.waypoint=actor.spawn_position;return;}
        const float angle=random(actor)*2*PI, radius=limit*std::sqrt(random(actor));
        actor.waypoint=Vector3Add(actor.spawn_position,{std::sin(angle)*radius,0,std::cos(angle)*radius});
    }
}
void move(NpcInstance& actor,Vector3 toward,float speed,float dt,const NpcEnvironment& environment) {
    auto direction=Vector3Subtract(toward,actor.position);direction.y=0;
    const float length=Vector3Length(direction);
    if(length<.02F || speed<=0) return;
    direction=Vector3Scale(direction,1/length);
    const float distance=std::min(length,speed*dt);
    const auto before=actor.position;
    const auto desired=Vector3Add(before,Vector3Scale(direction,distance));
    actor.position=environment.move ? environment.move(before,desired) : desired;
    const auto moved=Vector3Subtract(actor.position,before);
    if(Vector3LengthSqr(moved)>.000001F) actor.yaw_degrees=std::atan2(moved.x,moved.z)*RAD2DEG;
}
// Longer trips follow a route through doors and round buildings instead of a straight line.
void navigate(NpcInstance& actor,Vector3 goal,float speed,float dt,const NpcEnvironment& environment) {
    const auto before=actor.position;
    const auto target=actor.follower.steer(actor.position,goal,environment.now,environment.find_path);
    move(actor,target,speed,dt,environment);
    // Walking straight into a wall: ask for a route (open ground never needs one).
    const float wanted=std::min(speed*dt,std::hypot(target.x-before.x,target.z-before.z));
    const float moved=std::hypot(actor.position.x-before.x,actor.position.z-before.z);
    actor.stuck_time=wanted>.01F && moved<wanted*.3F ? actor.stuck_time+dt : 0.0F;
    if(actor.stuck_time>.6F) {actor.stuck_time=0;actor.follower.repath_soon();}
}
}
float NpcBehavior::tick(NpcInstance& actor,const NpcTuning& tuning,float dt,
    const NpcContext& context,const NpcEnvironment& environment) {
    if(!std::isfinite(dt) || dt<=0) return 0;
    if(actor.health<=0) {enter(actor,NpcState::Dead,tuning);return 0;}
    if(actor.state==NpcState::Dead || context.paused) return 0;
    actor.state_time+=dt;
    actor.threat_timer=std::max(0.0F,actor.threat_timer-dt);
    const auto difference=Vector3Subtract(context.player_position,actor.position);
    const float distance=Vector3Length(difference);
    const bool in_range=context.player_alive && distance<=tuning.detection_radius;
    const Vector3 eye=Vector3Add(actor.position,{0,1.45F,0}), player_eye=Vector3Add(context.player_position,{0,1.45F,0});
    const bool sees=in_range && (!context.visible || context.visible(eye,player_eye));
    const bool aggressive=actor.pool==CharacterPool::Hostile || actor.pool==CharacterPool::Creature;
    if(sees && (aggressive || context.threatening)) {
        actor.threat_position=context.player_position;
        actor.threat_timer=tuning.memory_seconds;
        actor.reaction_clock+=dt;
    } else actor.reaction_clock=0;
    if(!aggressive) {
        if(actor.threat_timer>0 && (actor.state==NpcState::Flee || actor.reaction_clock>=tuning.reaction_delay)) enter(actor,NpcState::Flee,tuning);
        if(actor.state==NpcState::Flee) {
            if(actor.threat_timer<=0) enter(actor,NpcState::Idle,tuning);
            else {
                auto away=Vector3Subtract(actor.position,actor.threat_position);away.y=0;
                if(Vector3LengthSqr(away)<.001F) away={1,0,0};
                move(actor,Vector3Add(actor.position,Vector3Scale(Vector3Normalize(away),5)),tuning.movement_speed*tuning.run_multiplier,dt,environment);
                return 0;
            }
        }
    } else {
        if(!context.player_alive) {actor.threat_timer=0;enter(actor,NpcState::Idle,tuning);}
        else if(actor.state==NpcState::Idle || actor.state==NpcState::Wander) {
            if(sees) enter(actor,NpcState::Alert,tuning);
        }
        if(actor.state==NpcState::Alert) {
            if(!sees) enter(actor,NpcState::Idle,tuning);
            else if(actor.reaction_clock>=tuning.reaction_delay) enter(actor,NpcState::Chase,tuning);
            return 0;
        }
        if(actor.state==NpcState::Chase || actor.state==NpcState::Attack) {
            const bool melee=context.player_alive && distance<=tuning.attack_range &&
                (!context.visible || context.visible(eye,player_eye));
            if(melee) {
                enter(actor,NpcState::Attack,tuning);
                actor.yaw_degrees=std::atan2(difference.x,difference.z)*RAD2DEG;
                actor.attack_clock-=dt;
                if(actor.attack_clock<=0) {
                    actor.attack_clock=tuning.attack_interval;actor.animation.restart();
                    return tuning.attack_damage;
                }
                return 0;
            }
            if(actor.threat_timer<=0) enter(actor,NpcState::Idle,tuning);
            else {enter(actor,NpcState::Chase,tuning);navigate(actor,actor.threat_position,tuning.movement_speed*tuning.run_multiplier,dt,environment);return 0;}
        }
    }
    if(actor.state==NpcState::Idle && actor.state_time>=(actor.directed ? actor.idle_hold : 1.0F)) enter(actor,NpcState::Wander,tuning);
    if(actor.state==NpcState::Wander) {
        const auto delta=Vector3Subtract(actor.waypoint,actor.position);
        if(delta.x*delta.x+delta.z*delta.z<.09F || (!actor.travelling && actor.state_time>6)) enter(actor,NpcState::Idle,tuning);
        else if(actor.travelling) navigate(actor,actor.waypoint,tuning.movement_speed*(actor.hurry ? tuning.run_multiplier : 1.0F),dt,environment);
        else move(actor,actor.waypoint,tuning.movement_speed*(actor.hurry ? tuning.run_multiplier : 1.0F),dt,environment);
    }
    return 0;
}
}
