#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ModeRules.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
using namespace outland;
using namespace outland::game::life;
using characters::NpcState;
namespace {
void check(bool condition,const std::string& message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
const Resident* find_if(const LifeSimulation& life,auto predicate) {
    for(const auto& r:life.residents()) if(predicate(r)) return &r;
    return nullptr;
}
float flat(Vector3 a,Vector3 b) {return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z));}
const Vector3 far_away{0,0,9000};
void run(LifeSimulation& life,characters::NpcSystem& npcs,const world::VerdaRegion& region,Vector3 player,float seconds) {
    characters::NpcContext context;context.player_position=player;
    for(float t=0;t<seconds;t+=.05F) {npcs.update(.05F,context,region);life.update(.05F,player,npcs);}
}
}
int main() {
    // Mode rules: one island, three interpretations.
    check(game::rules_for(game::GameMode::Explore).civilian_life,"explore has civilian life");
    check(!game::rules_for(game::GameMode::BattleRoyale).civilian_life && game::rules_for(game::GameMode::BattleRoyale).combat_bots,"battle royale suspends civilian life");
    check(game::rules_for(game::GameMode::ZombieSurvival).zombie_ecology && game::rules_for(game::GameMode::ZombieSurvival).ammo_scarcity<1,"zombie rules");

    // Clock.
    auto clock=WorldClock::at(6,9,14);
    check(clock.weekday()==Weekday::Sunday && clock.label()=="SUN 09:14","sunday label "+clock.label());
    clock.advance(120,30);check(clock.label()=="SUN 10:14","clock advances at time scale");
    clock=WorldClock::at(6,23,59);clock.advance(4,30);check(clock.weekday()==Weekday::Monday && clock.minute_of_day()==1,"week wraps");
    check(WorldClock::at(0,22).night() && !WorldClock::at(0,12).night(),"night");

    // Doors are outside the front wall, matching WorldCollision.
    for(float yaw:{0.0F,90.0F,180.0F,-37.0F}) {
        Place p;p.building_id="b";p.interior={10,0,20};p.building_size={12,4,10};p.building_yaw=yaw;
        p.door=door_position(p.interior,p.building_size,yaw,1.6F);
        check(!inside_footprint(p,p.door) && inside_footprint(p,p.interior),"door outside, interior inside");
        check(std::abs(flat(p.door,p.interior)-6.6F)<.01F,"door distance");
    }

    characters::CharacterRegistry registry;std::string error;
    check(registry.load(std::string(OUTLAND_SOURCE_DIR)+"/assets/verda/characters/character_manifest.tsv",error),error);
    world::VerdaRegion region(true);
    LifeSimulation life;life.build(region,&registry);
    LifeSimulation again;again.build(region,&registry);
    check(life.residents().size()==again.residents().size(),"deterministic size");
    for(std::size_t i=0;i<life.residents().size();++i) {
        const auto& a=life.residents()[i];const auto& b=again.residents()[i];
        check(a.full_name()==b.full_name() && a.occupation==b.occupation && a.work==b.work && a.character_id==b.character_id,"deterministic resident");
    }
    const auto& residents=life.residents();const auto& places=life.places();
    std::cout<<"population "<<residents.size()<<", places "<<places.size()<<'\n';
    check(residents.size()>=60,"populated island");
    const auto place_in=[&](PlaceKind kind,const std::string& town){
        return std::any_of(places.begin(),places.end(),[&](const Place& p){
            return p.kind==kind && life.island().settlement_names[static_cast<std::size_t>(p.settlement)]==town;});};
    check(place_in(PlaceKind::Dock,"Porto Luma") && place_in(PlaceKind::Industrial,"Roka"),"port docks and industrial works");
    check(place_in(PlaceKind::Police,"Verda") && place_in(PlaceKind::Clinic,"Verda") && place_in(PlaceKind::Church,"Verda"),"capital civic buildings");
    check(place_in(PlaceKind::Farm,"Espera") && place_in(PlaceKind::Garage,"Espera"),"rural fields and garage");
    for(const auto& town:{"Roka","Porto Luma","Suda Haveno"}) check(place_in(PlaceKind::Pub,town),std::string("pub in ")+town);
    std::set<Occupation> jobs;int krimulo=0,with_relatives=0;
    for(const auto& r:residents) {
        check(r.home>=0 && places[static_cast<std::size_t>(r.home)].home_capacity>0,"everyone has a home");
        check(!r.character_id.empty(),"everyone has a body model");
        check(std::find(r.household.begin(),r.household.end(),r.id)!=r.household.end(),"household includes self");
        check(r.days[0].front().minute==0,"day plans start at midnight");
        if(r.work>=0) check(r.occupation!=Occupation::Retired && r.occupation!=Occupation::Unemployed,"workers have jobs");
        jobs.insert(r.occupation);krimulo+=r.faction==Faction::Krimulo;with_relatives+=!r.relatives.empty();
        if(r.occupation==Occupation::PoliceOfficer) check(registry.find(r.character_id)->category=="emergency_police","police wear uniforms");
        if(r.occupation==Occupation::Dockworker) check(places[static_cast<std::size_t>(r.work)].kind==PlaceKind::Dock,"dockworkers work the docks");
    }
    for(auto job:{Occupation::Mechanic,Occupation::Dockworker,Occupation::FactoryWorker,Occupation::Pastor,Occupation::PoliceOfficer,
        Occupation::Shopkeeper,Occupation::Publican,Occupation::Farmer,Occupation::Doctor,Occupation::Retired})
        check(jobs.count(job)==1,std::string("occupation present: ")+occupation_name(job));
    check(krimulo>0 && with_relatives>0,"Krimulo members and relatives exist");

    // Schedules.
    const auto* mechanic=find_if(life,[](const Resident& r){return r.occupation==Occupation::Mechanic;});
    check(mechanic,"mechanic");
    check(planned_activity(*mechanic,Weekday::Monday,10*60)==Activity::Work,"mechanic works monday morning");
    check(planned_activity(*mechanic,Weekday::Monday,3*60)==Activity::Sleep,"mechanic sleeps at night");
    check(planned_activity(*mechanic,Weekday::Saturday,10*60+40)==Activity::Shopping,"saturday errands");
    const auto* pastor=find_if(life,[](const Resident& r){return r.occupation==Occupation::Pastor;});
    check(planned_activity(*pastor,Weekday::Sunday,10*60+30)==Activity::Work && places[static_cast<std::size_t>(pastor->work)].kind==PlaceKind::Church,"pastor leads sunday service");
    const auto* faithful=find_if(life,[](const Resident& r){return r.churchgoer && r.occupation==Occupation::FactoryWorker;});
    check(faithful && planned_activity(*faithful,Weekday::Sunday,10*60+30)==Activity::Worship,"churchgoer at church on sunday");
    const auto* docker=find_if(life,[](const Resident& r){return r.occupation==Occupation::Dockworker;});
    check(planned_activity(*docker,Weekday::Tuesday,6*60+30)==Activity::Work && planned_activity(*mechanic,Weekday::Tuesday,6*60)!=Activity::Work,"dock shift starts early");
    const auto* gangster=find_if(life,[](const Resident& r){return r.faction==Faction::Krimulo;});
    check(planned_activity(*gangster,Weekday::Saturday,23*60+30)==Activity::FactionMeet && planned_activity(*gangster,Weekday::Sunday,60)==Activity::FactionMeet,"Krimulo night");
    check(planned_activity(*gangster,Weekday::Wednesday,10*60+40)!=Activity::FactionMeet,"Krimulo daytime looks legitimate");

    // Abstract tier: nobody near, so no bodies, but everyone keeps living.
    characters::NpcSystem npcs;
    LifeConfig config;config.time_scale=60;life.configure(config);
    life.reset(npcs,WorldClock::at(0,6,0));
    check(life.residents()[static_cast<std::size_t>(mechanic->id)].activity==Activity::Sleep,"reset follows schedule");
    for(int i=0;i<60*4*4;++i) life.update(.25F,far_away,npcs); // four island hours
    check(life.clock().label()=="MON 10:00","clock "+life.clock().label());
    const auto& ivan=life.residents()[static_cast<std::size_t>(mechanic->id)];
    check(ivan.activity==Activity::Work && ivan.place==ivan.work && !life.travelling(ivan),"mechanic at work");
    check(flat(life.position(ivan),places[static_cast<std::size_t>(ivan.work)].door)<10,"abstract position at workplace");
    check(life.describe(ivan).find("working at the garage")!=std::string::npos,life.describe(ivan));
    std::cout<<life.describe(ivan)<<'\n';
    check(life.stats().physical==0 && npcs.actors().empty(),"no bodies far from the player");
    int at_work=0,workers=0;
    for(const auto& r:life.residents()) if(r.work>=0 && planned_activity(r,Weekday::Monday,600)==Activity::Work) {++workers;at_work+=r.place==r.work && !life.travelling(r);}
    check(at_work==workers,"every scheduled worker reached work");

    // Commuting between towns takes island time.
    const auto* commuter=find_if(life,[&](const Resident& r){return r.work>=0 && places[static_cast<std::size_t>(r.work)].settlement!=r.settlement &&
        planned_activity(r,Weekday::Monday,600)==Activity::Work && planned_activity(r,Weekday::Monday,1200)==Activity::Home;});
    check(commuter,"someone commutes to another town");
    life.set_clock(WorldClock::at(0,19,59));
    for(int i=0;i<12;++i) life.update(.25F,far_away,npcs);
    check(life.travelling(*commuter),"commuter on the road home: "+life.describe(*commuter));
    check(life.describe(*commuter).find("on the way to home")!=std::string::npos,life.describe(*commuter));
    for(int i=0;i<4*60;++i) life.update(.25F,far_away,npcs);
    check(!life.travelling(*commuter) && commuter->place==commuter->home,"commuter got home");

    // Physical tier: stand at the garage while the mechanic works outside.
    life.set_clock(WorldClock::at(0,10,0));
    life.update(.3F,far_away,npcs);
    const auto& garage=places[static_cast<std::size_t>(ivan.work)];
    config.time_scale=0;config.max_physical=4;life.configure(config);
    life.update(.3F,garage.door,npcs);
    check(life.stats().physical==std::min(4,static_cast<int>(std::count_if(life.residents().begin(),life.residents().end(),[&](const Resident& r){
        return flat(life.position(r),garage.door)<=(life.indoors(r) ? config.indoor_radius : config.materialize_radius);}))),"bodies capped and nearest first");
    check(static_cast<int>(npcs.actors().size())==life.stats().physical,"one actor per body");
    for(const auto& actor:npcs.actors()) check(actor.resident>=0 && actor.directed && actor.state!=NpcState::Dead,"resident actors directed");
    npcs.reconcile(region,registry);
    check(static_cast<int>(npcs.actors().size())==life.stats().physical,"marker reconciliation keeps residents");
    life.update(.3F,far_away,npcs);
    check(life.stats().physical==0 && npcs.actors().empty(),"bodies return to the abstract tier");

    // A body walks out of one building and in through another's doorway.
    config.max_physical=24;life.configure(config);
    const Resident* walker=nullptr;
    for(const auto& r:life.residents()) {
        if(r.work<0 || r.home<0 || !places[static_cast<std::size_t>(r.work)].indoor) continue;
        const float d=flat(places[static_cast<std::size_t>(r.home)].door,places[static_cast<std::size_t>(r.work)].door);
        if(d>40 && d<90 && planned_activity(r,Weekday::Monday,7*60)!=Activity::Work && planned_activity(r,Weekday::Monday,10*60)==Activity::Work)
            {walker=&r;break;}
    }
    check(walker,"walker found");
    const auto& home=places[static_cast<std::size_t>(walker->home)];
    const auto& work=places[static_cast<std::size_t>(walker->work)];
    config.time_scale=60;life.configure(config);life.set_clock(WorldClock::at(0,2,0));
    for(int i=0;i<4*30;++i) life.update(.25F,far_away,npcs); // let everyone finish the trip home
    config.time_scale=0;life.configure(config);life.set_clock(WorldClock::at(0,3,0));
    const Vector3 middle=Vector3Lerp(home.door,work.door,.5F);
    life.update(.3F,middle,npcs);
    check(!walker->physical,"sleepers indoors stay abstract unless the player is at the building");
    life.update(.3F,home.door,npcs);
    check(walker->physical && inside_footprint(home,walker->body),"asleep inside home");
    life.set_clock(WorldClock::at(0,10,0));
    bool left_home=false;
    for(int i=0;i<200 && !inside_footprint(work,walker->body,.2F);++i) {
        run(life,npcs,region,left_home ? middle : home.door,1);
        check(walker->physical,"walker stays physical near the player");
        left_home=left_home || !inside_footprint(home,walker->body);
    }
    check(left_home && inside_footprint(work,walker->body,.2F),"walked through both doorways: "+life.describe(*walker));
    std::cout<<life.describe(*walker)<<'\n';

    // Fear interrupts the schedule; when danger passes, life resumes.
    const int index=npcs.find_resident(walker->id);
    check(index>=0 && npcs.damage(static_cast<std::size_t>(index),10,work.door),"wound a resident");
    life.update(.3F,middle,npcs);
    check(walker->activity==Activity::Shelter && walker->fear>0 && walker->place==walker->home,"wounded resident shelters at home");
    check(npcs.actors()[static_cast<std::size_t>(npcs.find_resident(walker->id))].state==NpcState::Flee,"combat layer still owns the body");
    run(life,npcs,region,middle,3);
    check(walker->activity==Activity::Shelter,"fleeing keeps the resident sheltering");
    run(life,npcs,region,middle,6); // threat memory expires; the body calms down
    check(npcs.actors()[static_cast<std::size_t>(npcs.find_resident(walker->id))].state!=NpcState::Flee,"flee settles");
    life.set_clock(WorldClock::at(0,15,0));life.update(.3F,middle,npcs);
    check(walker->activity==Activity::Work && walker->fear==0,"schedule resumes after shelter");
    life.report_gunfire(middle);
    int sheltering=life.stats().sheltering;
    check(sheltering>0 && walker->activity==Activity::Shelter,"gunfire sends nearby residents home");
    const auto* distant=find_if(life,[&](const Resident& r){return flat(life.position(r),middle)>config.hearing_radius+50;});
    check(distant && distant->activity!=Activity::Shelter,"distant towns do not hear it");
    life.update(.3F,middle,npcs);
    for(const auto& actor:npcs.actors()) check(actor.hurry,"frightened bodies hurry");

    // Death persists for the session.
    const int victim=npcs.find_resident(walker->id);
    npcs.damage(static_cast<std::size_t>(victim),1000,middle);
    life.update(.3F,middle,npcs);
    check(!walker->alive && life.describe(*walker).find("dead")!=std::string::npos,"resident died");
    life.update(.3F,far_away,npcs);life.update(.3F,middle,npcs);
    check(npcs.find_resident(walker->id)<0,"the dead do not respawn");
    npcs.reset_session();life.reset(npcs);
    check(walker->alive && npcs.actors().empty(),"new session restores the island");

    // Directed wandering: travel to a far anchor without the six-second wander timeout.
    characters::NpcInstance traveller;traveller.directed=true;traveller.anchor_radius=1;traveller.spawn_position={30,0,0};
    characters::NpcTuning tuning;characters::NpcContext context;context.player_position={0,0,-500};
    for(int i=0;i<300;++i) characters::NpcBehavior::tick(traveller,tuning,.1F,context,{});
    check(flat(traveller.position,{30,0,0})<2,"directed actor reached a far anchor");

    // Hundreds of people stay cheap when far away.
    LifeSimulation crowd;crowd.build(region,&registry,{0xC0FFEE,8});
    characters::NpcSystem empty_npcs;crowd.configure({});crowd.reset(empty_npcs);
    std::cout<<"crowd "<<crowd.residents().size()<<'\n';
    check(crowd.residents().size()>=500,"hundreds of residents");
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<3600;++i) crowd.update(1.0F/60,far_away,empty_npcs);
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/3600;
    std::cout<<"abstract update "<<ms<<" ms/frame for "<<crowd.residents().size()<<" residents\n";
    check(ms<1.0,"abstract tier under a millisecond per frame");
    std::cout<<"life simulation tests passed\n";
}
