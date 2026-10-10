#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/game/sound/SoundBus.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
using game::sound::SoundBus;
using game::sound::SoundKind;
namespace radius = game::sound::radius;
namespace {
void check(bool condition,const std::string& message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
}
int main() {
    // ---- The bus itself ----
    {
        SoundBus bus;
        const auto a=bus.emit(SoundKind::Gunshot,{0,0,0},radius::rifle,0,1.0);
        const auto b=bus.emit(SoundKind::Footstep,{10,0,0},radius::sprint,3,1.1);
        check(a==1 && b==2 && bus.latest()==2 && bus.events().size()==2,"serials increase");
        check(bus.emit(SoundKind::Impact,{NAN,0,0},5,-1,1)==0 && bus.emit(SoundKind::Impact,{0,0,0},0,-1,1)==0,"bad events rejected");
        check(SoundBus::audible(bus.events()[0],{299,0,0}) && !SoundBus::audible(bus.events()[0],{301,0,0}),"gunshot range");
        check(SoundBus::audible(bus.events()[0],{400,0,0},1.5F),"keen hearing hears further");
        check(SoundBus::loudness(bus.events()[0],{0,0,0})==1 && SoundBus::loudness(bus.events()[0],{150,0,0})==.5F,"loudness falls off");
        int heard=0;bus.heard({20,0,0},1,0,[&](const auto&){++heard;});
        check(heard==2,"a listener between both hears both");
        heard=0;bus.heard({20,0,0},1,a,[&](const auto&){++heard;});
        check(heard==1,"only events newer than the last one consumed");
        heard=0;bus.heard({200,0,0},1,0,[&](const auto&){++heard;});
        check(heard==1,"footsteps don't carry 190 m");
        bus.advance(2.05);
        check(bus.events().size()==1 && bus.events()[0].kind==SoundKind::Footstep,"old events expire");
        bus.advance(3);check(bus.events().empty() && bus.latest()==2,"serials survive expiry");
    }

    // ---- NPCs: hunters investigate, civilians flee close gunfire ----
    {
        world::VerdaRegion region(false);
        characters::NpcSystem npcs;
        characters::ResidentSpawn hunter;hunter.resident=1;hunter.character_id="h";hunter.pool=characters::CharacterPool::Hostile;
        hunter.position=hunter.anchor={0,0,0};
        characters::ResidentSpawn near;near.resident=2;near.character_id="c";near.pool=characters::CharacterPool::Civilian;
        near.position=near.anchor={40,0,0};
        characters::ResidentSpawn far;far.resident=3;far.character_id="c";far.pool=characters::CharacterPool::Civilian;
        far.position=far.anchor={250,0,0};
        npcs.spawn_resident(hunter);npcs.spawn_resident(near);npcs.spawn_resident(far);
        SoundBus bus;
        characters::NpcContext context;context.player_position={0,0,5000};context.sounds=&bus;
        npcs.update(.1F,context,region);
        bus.emit(SoundKind::Gunshot,{60,0,0},radius::rifle,7,0);
        npcs.update(.1F,context,region);
        const auto& hunter_actor=npcs.actors()[static_cast<std::size_t>(npcs.find_resident(1))];
        const auto& near_actor=npcs.actors()[static_cast<std::size_t>(npcs.find_resident(2))];
        const auto& far_actor=npcs.actors()[static_cast<std::size_t>(npcs.find_resident(3))];
        check(hunter_actor.state==characters::NpcState::Chase && Vector3Distance(hunter_actor.threat_position,{60,0,0})<.01F,"hostile goes to investigate a gunshot it heard");
        check(near_actor.state==characters::NpcState::Flee,"civilian near gunfire flees");
        check(far_actor.state!=characters::NpcState::Flee,"civilian far from gunfire carries on");
        for(int i=0;i<30;++i) npcs.update(.1F,context,region);
        check(Vector3Distance(npcs.actors()[static_cast<std::size_t>(npcs.find_resident(1))].position,{60,0,0})<60,"hunter moves toward the sound");
        check(Vector3Distance(npcs.actors()[static_cast<std::size_t>(npcs.find_resident(2))].position,{60,0,0})>20,"civilian moves away");
        // Each event is heard once: an old shot doesn't keep re-alarming.
        const auto chased_at=npcs.actors()[static_cast<std::size_t>(npcs.find_resident(1))].threat_position;
        npcs.update(.1F,context,region);
        check(Vector3Distance(chased_at,npcs.actors()[static_cast<std::size_t>(npcs.find_resident(1))].threat_position)<.01F,"no repeated hearing");
        // Running feet are heard only up close.
        characters::NpcSystem quiet;
        quiet.spawn_resident(hunter);
        SoundBus steps;context.sounds=&steps;
        quiet.update(.1F,context,region);
        steps.emit(SoundKind::Footstep,{30,0,0},radius::sprint,0,0);
        quiet.update(.1F,context,region);
        check(quiet.actors()[0].state!=characters::NpcState::Chase,"distant footsteps unheard");
        steps.emit(SoundKind::Footstep,{8,0,0},radius::sprint,0,0);
        quiet.update(.1F,context,region);
        check(quiet.actors()[0].state==characters::NpcState::Chase,"close footsteps heard");
    }

    // ---- Combat bots hear the shared bus, including noises they didn't make ----
    {
        game::ai::BattleRoyaleBots match;
        auto& bot=match.add_bot({0,0,0},game::combat::WeaponId::Rifle,game::ai::BotLevel::Hard,5);bot.set_yaw(0);
        SoundBus bus;
        game::ai::BotWorld world;
        world.environment.line_of_sight=[](Vector3,Vector3){return false;};
        world.environment.move=[](Vector3 from,Vector3){return from;};
        world.sounds=&bus;
        game::ai::BattleRoyaleFrame frame;frame.dt=.02F;frame.player_feet={0,0,900};frame.player_alive=false;
        match.update(frame,world);
        bus.emit(SoundKind::Engine,{0,0,-30},radius::engine,-1,.02);
        bool investigated=false;
        for(int i=2;i<60;++i) {frame.now=i*.02;match.update(frame,world);investigated|=match.bots()[0].intent()==game::ai::BotIntent::Investigate;}
        check(investigated,"bot investigates an engine it heard (got "+std::string(game::ai::bot_intent_name(match.bots()[0].intent()))+")");
        // Bot gunfire goes onto the shared bus for everyone else.
        game::ai::BattleRoyaleBots duel;
        auto& a=duel.add_bot({0,0,0},game::combat::WeaponId::Rifle,game::ai::BotLevel::Hard,1);a.set_yaw(0);
        auto& b=duel.add_bot({0,0,30},game::combat::WeaponId::Rifle,game::ai::BotLevel::Hard,2);b.set_yaw(180);
        SoundBus shared;game::ai::BotWorld open;
        open.environment.line_of_sight=[](Vector3,Vector3){return true;};
        open.environment.move=[](Vector3,Vector3 to){return to;};
        open.sounds=&shared;
        bool gunshot=false;
        for(int i=0;i<200 && !gunshot;++i) {
            frame.now=i*.02;duel.update(frame,open);
            for(const auto& e:shared.events()) gunshot|=e.kind==SoundKind::Gunshot && e.source>0;
        }
        check(gunshot,"bot gunfire is on the shared bus");
    }

    // ---- Residents: any gunshot on the bus frightens them ----
    {
        game::life::LifeSimulation life;
        world::VerdaRegion region(true);
        characters::NpcSystem npcs;
        life.build(region,nullptr);
        life.reset(npcs);
        check(!life.empty(),"island has residents");
        const auto& r=life.residents().front();
        const Vector3 at=life.position(r);
        SoundBus bus;
        life.hear(bus);
        bus.emit(SoundKind::Footstep,at,radius::sprint,0,0);
        life.hear(bus);
        check(life.residents().front().fear==0,"footsteps don't frighten residents");
        bus.emit(SoundKind::Gunshot,at,radius::rifle,4,0);
        life.hear(bus);
        check(life.residents().front().fear>.9F,"a gunshot next to a resident frightens them");
    }
    std::cout<<"sound bus tests passed\n";
}
