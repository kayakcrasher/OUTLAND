#include "outland/characters/NpcSystem.hpp"
#include "outland/game/law/Justice.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/sound/SoundBus.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
using game::law::CrimeKind;
namespace {
void check(bool condition,const std::string& message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
float flat(Vector3 a,Vector3 b) {return std::hypot(a.x-b.x,a.z-b.z);}
struct World {
    world::VerdaRegion region{true};
    game::life::LifeSimulation life;
    characters::NpcSystem npcs;
    game::law::Justice justice;
    double now=0;
    World() {life.build(region,nullptr);life.reset(npcs);life.configure({});}
    game::law::JusticeFrame frame(Vector3 player,bool see=true,float daylight=1) {
        game::law::JusticeFrame f;f.dt=.1F;f.now=now;f.player=player;f.daylight=daylight;
        f.line_of_sight=[see](Vector3,Vector3){return see;};
        return f;
    }
    // Abstract tier only: the player stays far from everyone, so nobody materialises.
    void run(float seconds,Vector3 player,bool see=true,bool bodies=false) {
        characters::NpcContext context;context.player_position=bodies ? player : Vector3{0,0,9000};
        for(float t=0;t<seconds;t+=.1F) {
            now+=.1;
            if(bodies) npcs.update(.1F,context,region);
            life.update(.1F,bodies ? player : Vector3{0,0,9000},npcs);
            justice.update(frame(player,see),life,npcs);
        }
    }
    const game::life::Resident* outdoor_civilian(int skip=0) {
        for(const auto& r:life.residents())
            if(r.alive && r.occupation!=game::life::Occupation::PoliceOfficer && !life.indoors(r) && skip--<=0) return &r;
        return nullptr;
    }
    int officers() const {int n=0;for(const auto& r:life.residents()) n+=r.occupation==game::life::Occupation::PoliceOfficer;return n;}
};
}
int main() {
    check(game::law::crime_severity(CrimeKind::ShotsFired)==1 && game::law::crime_severity(CrimeKind::Murder)==3 &&
          game::law::crime_severity(CrimeKind::AttackOnPolice)==4,"severities");

    // ---- A witness sees an assault, calls a few seconds later, police are dispatched ----
    {
        World w;
        check(w.officers()>=3,"the island has police officers ("+std::to_string(w.officers())+")");
        const auto* witness=w.outdoor_civilian();
        check(witness!=nullptr,"someone is outdoors");
        const auto player=Vector3Add(w.life.position(*witness),{4,0,0});
        w.justice.crime(CrimeKind::Assault,player,-1,w.frame(player),w.life);
        check(!w.justice.witnesses().empty() && w.justice.wanted()==0,"the witness has not called yet");
        w.run(2,player);
        check(w.justice.wanted()==0,"no instant reports from civilians");
        w.run(14,player);
        check(w.justice.wanted()==2 && w.justice.reports()>=1,"assault reported: two stars");
        check(w.justice.responders().size()==3,"three officers dispatched");
        for(const int id:w.justice.responders()) check(w.life.residents()[static_cast<std::size_t>(id)].responding,"responders drop their schedule");
        // They drive toward the scene.
        const int first=w.justice.responders().front();
        const float before=flat(w.life.position(w.life.residents()[static_cast<std::size_t>(first)]),player);
        w.run(3,player);
        const float after=flat(w.life.position(w.life.residents()[static_cast<std::size_t>(first)]),player);
        check(after<before || after<3,"responders close in ("+std::to_string(before)+" -> "+std::to_string(after)+")");
        // ---- Out of sight long enough: the police give up and go back to their day ----
        w.run(55,player,false);
        check(w.justice.wanted()==0 && w.justice.responders().empty(),"lost the police");
        check(w.justice.message()=="You lost the police","told so");
        for(const auto& r:w.life.residents()) check(!r.responding,"everyone released");
    }

    // ---- Witnesses who die before calling report nothing ----
    {
        World w;
        const auto* witness=w.outdoor_civilian();
        const auto spot=w.life.position(*witness);
        const auto player=Vector3Add(spot,{3,0,0});
        // Bodies near the player: the witness becomes a physical NPC.
        w.run(1,player,true,true);
        const auto& r=w.life.residents()[static_cast<std::size_t>(witness->id)];
        check(r.physical,"witness has a body");
        w.justice.crime(CrimeKind::Assault,player,-1,w.frame(player),w.life);
        bool pending=false;for(const auto& p:w.justice.witnesses()) pending|=p.resident==r.id;
        check(pending,"the witness will call");
        // Silence every witness before the calls go through.
        for(std::size_t i=0;i<w.npcs.actors().size();++i) if(w.npcs.actors()[i].resident>=0) w.npcs.damage(i,1000,player);
        w.run(20,player,true,true);
        check(w.justice.wanted()==0,"dead witnesses do not call ("+std::to_string(w.justice.reports())+" reports)");
    }

    // ---- Shots heard but unseen: someone comes to look, nobody is wanted ----
    {
        World w;
        const auto* hearer=w.outdoor_civilian();
        // A spot 60 m from the hearer with no resident within sight of it.
        Vector3 spot{};bool found=false;
        for(int k=0;k<36 && !found;++k) {
            const float a=k*10*DEG2RAD;
            spot=Vector3Add(w.life.position(*hearer),{std::cos(a)*60,0,std::sin(a)*60});
            found=true;
            for(const auto& r:w.life.residents()) if(r.alive && flat(w.life.position(r),spot)<30) {found=false;break;}
        }
        check(found,"a quiet spot");
        w.justice.crime(CrimeKind::ShotsFired,spot,-1,w.frame(spot,false),w.life);
        w.run(16,spot,false);
        check(w.justice.wanted()==0 && w.justice.reports()==1,"anonymous shots fired");
        check(w.justice.responders().size()==1,"one officer investigates");
        w.run(70,spot,false);
        check(w.justice.responders().empty(),"investigation ends");
    }

    // ---- Darkness: a witness 20 m away sees by day, not by night ----
    {
        World day,night;
        const auto* a=day.outdoor_civilian();const auto* b=night.outdoor_civilian();
        const auto pa=Vector3Add(day.life.position(*a),{20,0,0}),pb=Vector3Add(night.life.position(*b),{20,0,0});
        day.justice.crime(CrimeKind::Murder,pa,-1,day.frame(pa,true,1),day.life);
        night.justice.crime(CrimeKind::Murder,pb,-1,night.frame(pb,true,0),night.life);
        bool day_saw=false,night_saw=false;
        for(const auto& p:day.justice.witnesses()) day_saw|=p.resident==a->id && p.saw;
        for(const auto& p:night.justice.witnesses()) night_saw|=p.resident==b->id && p.saw;
        check(day_saw && !night_saw,"night shortens sight");
    }

    // ---- One star: a calm player next to an officer is arrested ----
    {
        World w;
        const game::life::Resident* officer=nullptr;
        for(const auto& r:w.life.residents()) if(r.occupation==game::life::Occupation::PoliceOfficer && !w.life.indoors(r)) {officer=&r;break;}
        check(officer!=nullptr,"an officer outdoors");
        const auto player=Vector3Add(w.life.position(*officer),{6,0,0});
        w.run(1,player,true,true);
        check(w.life.residents()[static_cast<std::size_t>(officer->id)].physical,"officer has a body");
        auto f=w.frame(player);f.player_fired=true;
        w.justice.crime(CrimeKind::ShotsFired,player,-1,f,w.life);
        w.justice.update(f,w.life,w.npcs);
        check(w.justice.wanted()==1,"an officer saw the shot: one star");
        bool arrested=false;
        for(int i=0;i<300 && !arrested;++i) {w.run(.1F,player,true,true);arrested|=w.justice.message().starts_with("ARRESTED");}
        check(arrested && w.justice.wanted()==0,"arrested once the officer reaches a calm player");
    }

    // ---- Armed officers shoot on sight; officers never flee gunfire ----
    {
        characters::NpcTuning tuning;
        characters::NpcInstance cop;cop.pool=characters::CharacterPool::Emergency;cop.police=true;cop.armed=true;cop.random_state=7;
        characters::NpcContext context;context.player_position={0,0,15};context.visible=[](Vector3,Vector3){return true;};
        float damage=0;int shots=0;
        for(int i=0;i<60;++i) {damage+=characters::NpcBehavior::tick(cop,tuning,.1F,context,{});if(cop.fired) {++shots;cop.fired=false;}}
        check(shots>=3 && damage>0,"armed officer fires and hits ("+std::to_string(shots)+" shots)");
        check(cop.state==characters::NpcState::Attack,"standing to shoot");
        context.visible=[](Vector3,Vector3){return false;};context.player_position={0,0,40};
        characters::NpcInstance blind=cop;blind.threat_timer=0;blind.state=characters::NpcState::Idle;
        int blind_shots=0;
        for(int i=0;i<30;++i) {characters::NpcBehavior::tick(blind,tuning,.1F,context,{});blind_shots+=blind.fired;blind.fired=false;}
        check(blind_shots==0,"no shots without line of sight");

        world::VerdaRegion region(false);
        characters::NpcSystem npcs;
        characters::ResidentSpawn officer;officer.resident=1;officer.character_id="o";officer.pool=characters::CharacterPool::Emergency;officer.police=true;
        characters::ResidentSpawn civilian;civilian.resident=2;civilian.character_id="c";civilian.position=civilian.anchor={2,0,0};
        npcs.spawn_resident(officer);npcs.spawn_resident(civilian);
        game::sound::SoundBus bus;characters::NpcContext quiet;quiet.player_position={0,0,9000};quiet.sounds=&bus;
        npcs.update(.1F,quiet,region);
        bus.emit(game::sound::SoundKind::Gunshot,{5,0,0},300,0,0);
        npcs.update(.1F,quiet,region);
        check(npcs.actors()[static_cast<std::size_t>(npcs.find_resident(1))].state!=characters::NpcState::Flee,"officer holds ground");
        check(npcs.actors()[static_cast<std::size_t>(npcs.find_resident(2))].state==characters::NpcState::Flee,"civilian flees");
        npcs.damage(static_cast<std::size_t>(npcs.find_resident(1)),10,{5,0,0});
        check(npcs.actors()[static_cast<std::size_t>(npcs.find_resident(1))].state!=characters::NpcState::Flee,"a shot officer does not run");
    }
    std::cout<<"justice tests passed\n";
}
