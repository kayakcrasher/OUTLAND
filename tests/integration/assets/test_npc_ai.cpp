#include "outland/characters/NpcSystem.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/combat/CombatWorld.hpp"
#include "outland/game/combat/WeaponSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace outland;
using namespace characters;
namespace {
NpcInstance actor(CharacterPool pool) { NpcInstance a; a.pool=pool;a.random_state=42; return a; }
void step(NpcInstance& a,NpcTuning t,NpcContext c,int count) {
    for(int i=0;i<count;++i) NpcBehavior::tick(a,t,.1F,c,{});
}
}
int main() {
    NpcTuning tuning; tuning.reaction_delay=.3F;tuning.attack_interval=.3F;
    NpcContext context; context.player_position={5,0,0};
    for(auto pool:{CharacterPool::Civilian,CharacterPool::Emergency}) {
        auto a=actor(pool);step(a,tuning,context,3);assert(a.state==NpcState::Idle);
        step(a,tuning,context,8);assert(a.state==NpcState::Wander);
        const auto previous=a.position;step(a,tuning,context,2);assert(a.position.x!=previous.x || a.position.z!=previous.z);
        a=actor(pool);context.threatening=true;step(a,tuning,context,2);assert(a.state!=NpcState::Flee);
        step(a,tuning,context,2);assert(a.state==NpcState::Flee && a.position.x<0);
        for(int i=0;i<40;++i) assert(NpcBehavior::tick(a,tuning,.1F,context,{})==0);
        context.threatening=false;context.player_position={1000,0,0};step(a,tuning,context,45);assert(a.state!=NpcState::Flee);
        context.player_position={5,0,0};
    }
    for(auto pool:{CharacterPool::Hostile,CharacterPool::Creature}) {
        auto a=actor(pool);step(a,tuning,context,1);assert(a.state==NpcState::Alert);
        step(a,tuning,context,3);assert(a.state==NpcState::Chase && a.position.x>0);
        context.player_position={a.position.x+1,0,0};
        assert(NpcBehavior::tick(a,tuning,.1F,context,{})==0 && a.state==NpcState::Attack);
        float damage=0;for(int i=0;i<8;++i) damage+=NpcBehavior::tick(a,tuning,.1F,context,{});
        assert(damage>=tuning.attack_damage*2);
        context.visible=[](Vector3,Vector3){return false;};step(a,tuning,context,45);assert(a.state==NpcState::Idle || a.state==NpcState::Wander);
        context.visible={};context.player_alive=false;step(a,tuning,context,3);assert(a.state!=NpcState::Attack && a.state!=NpcState::Chase);
        context.player_alive=true;a.health=0;step(a,tuning,context,1);assert(a.state==NpcState::Dead);
        a.health=100;auto position=a.position;step(a,tuning,context,20);assert(a.state==NpcState::Dead && a.position.x==position.x);
        context.player_position={5,0,0};
    }
    auto blocked=actor(CharacterPool::Hostile);context.visible=[](Vector3,Vector3){return false;};step(blocked,tuning,context,5);assert(blocked.state==NpcState::Idle);
    context.visible={};auto paused=actor(CharacterPool::Hostile);context.paused=true;step(paused,tuning,context,30);assert(paused.state==NpcState::Idle && paused.state_time==0);context.paused=false;
    auto stopped=actor(CharacterPool::Hostile);stopped.state=NpcState::Chase;stopped.threat_timer=4;
    NpcEnvironment wall;wall.move=[](Vector3 current,Vector3){return current;};NpcBehavior::tick(stopped,tuning,.1F,context,wall);assert(stopped.position.x==0);
    tuning.activation_distance=200;tuning.despawn_distance=10;tuning.health=-2;tuning.movement_speed=std::numeric_limits<float>::quiet_NaN();tuning.sanitize();
    assert(tuning.despawn_distance==200 && tuning.health==1 && std::isfinite(tuning.movement_speed));

    CharacterRegistry registry;std::string error;
    assert(registry.load(std::string(OUTLAND_SOURCE_DIR)+"/assets/verda/characters/character_manifest.tsv",error));
    world::VerdaRegion region;NpcSystem npcs;
    assert(npcs.tuning(CharacterPool::Creature).movement_speed!=npcs.tuning(CharacterPool::Hostile).movement_speed);
    const auto map=std::filesystem::temp_directory_path()/"outland_npc_ai_test.map";
    auto load=[&](float x,bool enabled) {
        {std::ofstream out(map);out<<"OUTLAND_CREATOR_MAP 3\nMARKER \"creator_marker_npc_spawn_hostile_test\" 2 "<<x<<" 20 0 .8 1.8 .8 0 "<<enabled<<"\n";}
        assert(creator::CreatorMapIO::load(region,map.string()));npcs.reconcile(region,registry);
    };
    load(0,true);assert(npcs.actors().size()==1);
    tuning=npcs.tuning(CharacterPool::Hostile);tuning.activation_distance=10;tuning.despawn_distance=20;tuning.detection_radius=0;npcs.configure(CharacterPool::Hostile,tuning);
    context={};context.player_position={30,20,0};int sight_queries=0;context.visible=[&](Vector3,Vector3){++sight_queries;return true;};
    for(int i=0;i<100;++i)npcs.update(.1F,context,region);
    assert(!npcs.actors()[0].active && !npcs.actors()[0].visible && npcs.actors()[0].state_time==0 && sight_queries==0);
    context.player_position={9,20,0};npcs.update(.1F,context,region);assert(npcs.actors()[0].active && npcs.actors()[0].visible && npcs.actors()[0].state_time<.2F);
    context.player_position={15,20,0};npcs.update(.1F,context,region);assert(npcs.actors()[0].active); // Hysteresis.
    context.paused=true;const float time=npcs.actors()[0].state_time;npcs.update(.2F,context,region);assert(npcs.actors()[0].state_time==time);context.paused=false;
    context.player_position={21,20,0};npcs.update(.1F,context,region);assert(!npcs.actors()[0].active && !npcs.actors()[0].visible);
    context.player_position={0,20,0};npcs.update(.1F,context,region);
    assert(npcs.damage(0,10,{4,20,0}));const auto wounded=npcs.actors()[0];
    npcs.reconcile(region,registry);assert(npcs.actors()[0].health==wounded.health && npcs.actors()[0].state==wounded.state);
    auto melee=tuning;melee.detection_radius=5;melee.movement_speed=0;melee.reaction_delay=0;melee.attack_interval=.1F;
    npcs.configure(CharacterPool::Hostile,melee);context.player_position={1,20,0};
    npcs.update(.1F,context,region);assert(npcs.events().attacks==1 && npcs.events().player_damage==melee.attack_damage);
    npcs.configure(CharacterPool::Hostile,tuning);
    const auto hit=npcs.trace_segment({-5,21,0},{5,21,0});assert(hit.actor==0 && !hit.headshot && hit.fraction<.5F);
    assert(npcs.trace_segment({-5,21.7F,0},{5,21.7F,0}).headshot);
    assert(npcs.trace_segment({-5,23,0},{5,23,0}).actor==-1);
    game::combat::CombatWorld combat(region);
    combat.bind_actors([&](Vector3 a,Vector3 b){const auto h=npcs.trace_segment(a,b);return game::combat::BulletHit{h.actor<0 ? game::combat::HitKind::None : game::combat::HitKind::Npc,h.fraction,h.position,h.normal,h.actor,h.headshot};},
        [&](int id,float amount,Vector3 source){(void)npcs.damage(static_cast<std::size_t>(id),amount,source);});
    auto bullet=combat.trace_segment({-5,21,0},{5,21,0},true,false);assert(bullet.kind==game::combat::HitKind::Npc);
    assert(!combat.trace_segment({-5,21,0},{5,21,0},false,false).hit()); // Sight/camera exclude actors.
    game::combat::WeaponSystem gun;game::combat::WeaponInput trigger;trigger.fire=true;trigger.aim=true;
    gun.update(.02F,trigger,{{-5,21,0},{1,0,0}},combat);
    assert(npcs.actors()[0].health<wounded.health && gun.hit_marker()>0); // Existing swept-projectile damage path.
    auto& settlements=const_cast<std::vector<world::Settlement>&>(region.settlements());
    world::Building cover;cover.position={-2,0,0};cover.size={1,30,2};settlements.front().buildings.push_back(cover);
    assert(combat.trace_segment({-5,21,0},{5,21,0},true,false).kind==game::combat::HitKind::Building);
    settlements.front().buildings.pop_back();
    combat.damage_hit(bullet,10000,{-5,21,0});assert(npcs.actors()[0].state==NpcState::Dead && npcs.actors()[0].health==0);
    assert(!npcs.damage(0,1,{}));assert(npcs.trace_segment({-5,21,0},{5,21,0}).actor==-1);
    npcs.update(.1F,context,region);assert(npcs.actors()[0].animation.action()==AnimationAction::Death && npcs.actors()[0].animation.elapsed()>0);
    const auto death_clock=npcs.actors()[0].animation.elapsed();npcs.reconcile(region,registry);assert(npcs.actors()[0].animation.elapsed()==death_clock);
    load(2,true);assert(npcs.actors()[0].state==NpcState::Dead && npcs.actors()[0].health==0 && npcs.actors()[0].position.x==2);
    npcs.reset_session();assert(npcs.actors()[0].state==NpcState::Idle && npcs.actors()[0].health==tuning.health);
    assert(!npcs.damage(0,10,{std::numeric_limits<float>::quiet_NaN(),0,0}));
    load(2,false);assert(npcs.actors().empty());load(2,true);assert(npcs.actors().size()==1 && npcs.actors()[0].health==tuning.health);
    std::filesystem::remove(map);
    std::cout<<"[PASS] NPC categories, seven states, sight/reaction, attacks, flee, pause, distances, damage, death, marker edits and combat bridge\n";
}
