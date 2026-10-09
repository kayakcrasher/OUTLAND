#include "outland/game/life/LifeSimulation.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace outland::game::life {
namespace {
float flat_distance(Vector3 a,Vector3 b) {const float x=a.x-b.x,z=a.z-b.z;return std::sqrt(x*x+z*z);}
std::uint64_t mix(std::uint64_t value) {
    value+=0x9e3779b97f4a7c15ULL;value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27))*0x94d049bb133111ebULL;return value^(value>>31);
}
// Island minutes for a trip: walk locally, drive (or catch a lift) between towns.
double travel_minutes(float distance) {
    if(distance<2) return 0;
    return distance<=500 ? distance/1.4/60 : 4+distance/11.0/60;
}
float outdoor_share(Activity activity,PlaceKind kind) {
    switch(activity) {
        case Activity::Sleep:case Activity::Shelter:return 0;
        case Activity::Home:return .12F;
        case Activity::Lunch:return .3F;
        case Activity::Socialize:return .35F;
        case Activity::Worship:return .05F;
        case Activity::Shopping:return .25F;
        case Activity::Visit:return .2F;
        case Activity::FactionMeet:return .5F;
        case Activity::Work:
            switch(kind) {
                case PlaceKind::Garage:return .55F;case PlaceKind::Industrial:return .25F;
                case PlaceKind::Police:return .2F;case PlaceKind::Pub:return .2F;case PlaceKind::Shop:return .15F;
                case PlaceKind::Church:return .1F;case PlaceKind::Office:return .1F;default:return .05F;
            }
        default:return 1;
    }
}
}

void LifeConfig::sanitize() {
    const auto bound=[](float value,float low,float high,float fallback){return std::isfinite(value) ? std::clamp(value,low,high) : fallback;};
    time_scale=bound(time_scale,0,3600,30);
    materialize_radius=bound(materialize_radius,5,1000,110);
    dematerialize_radius=std::max(bound(dematerialize_radius,5,1500,150),materialize_radius+5);
    indoor_radius=bound(indoor_radius,1,materialize_radius,18);
    max_physical=std::clamp(max_physical,0,256);
    hearing_radius=bound(hearing_radius,0,3000,260);
    shelter_minutes=bound(shelter_minutes,0,1440,150);
    replan_period=bound(replan_period,.02F,60,.5F);
}

void LifeSimulation::build(const world::VerdaRegion& region,const characters::CharacterRegistry* registry,PopulationOptions options) {
    island_=build_island(region,registry,options);
    cursor_=0;replan_budget_=bridge_clock_=0;
}

void LifeSimulation::clear(characters::NpcSystem& npcs) {
    for(const auto& r:island_.residents) if(r.physical) npcs.despawn_resident(r.id);
    island_={};cursor_=0;
}

void LifeSimulation::reset(characters::NpcSystem& npcs,WorldClock start) {
    clock_=start;cursor_=0;replan_budget_=0;bridge_clock_=std::numeric_limits<float>::max();
    for(auto& r:island_.residents) {
        if(r.physical) npcs.despawn_resident(r.id);
        r.alive=true;r.physical=false;r.health=100;r.fear=0;r.shelter_until=-1;r.patrol_step=0;
        r.activity=planned_activity(r,clock_.weekday(),clock_.minute_of_day());
        r.place=resolve(r,r.activity);
        r.depart=r.arrive=clock_.minutes;
        r.from=r.body=abstract_position(r);
    }
}

int LifeSimulation::resolve(const Resident& r,Activity activity) const {
    const auto square=[&](int settlement) {
        for(std::size_t i=0;i<island_.places.size();++i)
            if(island_.places[i].kind==PlaceKind::Square && island_.places[i].settlement==settlement) return static_cast<int>(i);
        return r.home;
    };
    const auto either=[&](int place){return place>=0 ? place : r.home;};
    switch(activity) {
        case Activity::Work:return either(r.work);
        case Activity::Lunch:return either(r.lunch);
        case Activity::Socialize:return either(r.pub);
        case Activity::Worship:return either(r.church);
        case Activity::Shopping:return either(r.shop);
        case Activity::Visit:return r.visit_home>=0 ? r.visit_home : square(r.settlement);
        case Activity::Stroll:return square(r.settlement);
        case Activity::FactionMeet:return either(r.hangout);
        case Activity::Patrol: {
            // Officers walk a beat through every public place in their assigned town.
            std::vector<int> beat;
            for(std::size_t i=0;i<island_.places.size();++i) {
                const auto& place=island_.places[i];
                if(place.settlement==r.patrol_settlement && place.kind!=PlaceKind::Home) beat.push_back(static_cast<int>(i));
            }
            if(beat.empty()) return either(r.work);
            const auto period=static_cast<std::uint64_t>(std::max(0.0,clock_.minutes/40));
            return beat[(period+static_cast<std::uint64_t>(r.id))%beat.size()];
        }
        default:return r.home;
    }
}

bool LifeSimulation::outdoors_now(const Resident& r) const {
    if(r.place<0) return true;
    const auto& place=island_.places[static_cast<std::size_t>(r.place)];
    if(!place.indoor || r.activity==Activity::Patrol) return true;
    // People step outside for a smoke, a chat or yard work in twenty-minute spells.
    const auto period=static_cast<std::uint64_t>(std::max(0.0,clock_.minutes/20));
    const float roll=static_cast<float>(mix(period*2654435761ULL+static_cast<std::uint64_t>(r.id))%1000)/1000.0F;
    return roll<outdoor_share(r.activity,place.kind);
}

Vector3 LifeSimulation::abstract_position(const Resident& r) const {
    if(r.place<0) return r.from;
    const auto& place=island_.places[static_cast<std::size_t>(r.place)];
    if(clock_.minutes<r.arrive && r.arrive>r.depart) {
        const float t=static_cast<float>((clock_.minutes-r.depart)/(r.arrive-r.depart));
        return Vector3Lerp(r.from,place.door,std::clamp(t,0.0F,1.0F));
    }
    if(place.indoor && !outdoors_now(r)) return place.interior;
    const float angle=static_cast<float>(mix(static_cast<std::uint64_t>(r.id))%6283)/1000.0F;
    const float spread=place.indoor ? 1.5F : 5.0F;
    return {place.door.x+std::sin(angle)*spread,place.door.y,place.door.z+std::cos(angle)*spread};
}

Vector3 LifeSimulation::position(const Resident& r) const {return r.physical ? r.body : abstract_position(r);}

bool LifeSimulation::indoors(const Resident& r) const {
    if(r.place<0) return false;
    const auto& place=island_.places[static_cast<std::size_t>(r.place)];
    if(r.physical) return inside_footprint(place,r.body,0);
    return !travelling(r) && place.indoor && !outdoors_now(r);
}

void LifeSimulation::replan(Resident& r) {
    if(!r.alive) return;
    if(r.shelter_until>=0 && r.shelter_until<=clock_.minutes) {r.shelter_until=-1;r.fear=0;}
    const Activity activity=r.shelter_until>clock_.minutes ? Activity::Shelter :
        planned_activity(r,clock_.weekday(),clock_.minute_of_day());
    const int place=resolve(r,activity);
    if(activity==r.activity && place==r.place) return;
    const Vector3 current=position(r);
    r.activity=activity;r.place=place;r.from=current;r.depart=clock_.minutes;
    r.arrive=clock_.minutes+travel_minutes(flat_distance(current,island_.places[static_cast<std::size_t>(place)].door));
}

LifeSimulation::Anchor LifeSimulation::physical_anchor(const Resident& r,Vector3 body) const {
    const auto& target=island_.places[static_cast<std::size_t>(r.place)];
    const bool want_inside=target.indoor && !outdoors_now(r);
    const float inside_radius=std::max(.5F,std::min(target.building_size.x,target.building_size.z)*.28F);
    // Leave any other building through its doorway before heading off; walls have no shortcuts.
    for(const auto& place:island_.places) {
        if(!inside_footprint(place,body,.1F)) continue;
        if(&place==&target && want_inside) return {target.interior,inside_radius};
        return {place.door,.6F};
    }
    if(want_inside) {
        if(flat_distance(body,target.door)<2.2F) return {target.interior,inside_radius};
        return {target.door,.8F};
    }
    if(r.activity==Activity::Patrol) return {target.door,4};
    return {target.door,target.indoor ? 2.5F : 7.0F};
}

void LifeSimulation::sync_bodies(Vector3 player,characters::NpcSystem& npcs) {
    using characters::NpcState;
    int bodies=0;
    for(auto& r:island_.residents) {
        if(!r.physical) continue;
        const int index=npcs.find_resident(r.id);
        if(index<0) {r.physical=false;r.from=r.body;r.depart=r.arrive=clock_.minutes;continue;}
        const auto& actor=npcs.actors()[static_cast<std::size_t>(index)];
        r.body=actor.position;r.health=actor.health;
        if(actor.state==NpcState::Dead || actor.health<=0) r.alive=false;
        else if(actor.state==NpcState::Flee) {
            r.fear=1;r.shelter_until=std::max(r.shelter_until,clock_.minutes+config_.shelter_minutes);
        }
        replan(r);
        const float distance=flat_distance(r.body,player);
        const bool keep=distance<=config_.dematerialize_radius &&
            (!r.alive || !indoors(r) || distance<=config_.indoor_radius+12);
        if(keep) {++bodies;continue;}
        // Back to the abstract tier from exactly where the body stood.
        npcs.despawn_resident(r.id);r.physical=false;
        if(!r.alive) continue;
        r.from=r.body;r.depart=clock_.minutes;
        r.arrive=clock_.minutes+travel_minutes(flat_distance(r.body,island_.places[static_cast<std::size_t>(r.place)].door));
    }
    std::vector<std::pair<float,int>> candidates;
    for(const auto& r:island_.residents) {
        if(r.physical || !r.alive) continue;
        const float distance=flat_distance(abstract_position(r),player);
        if(distance<=(indoors(r) ? config_.indoor_radius : config_.materialize_radius)) candidates.emplace_back(distance,r.id);
    }
    std::sort(candidates.begin(),candidates.end());
    for(const auto& [distance,id]:candidates) {
        if(bodies>=config_.max_physical) break;
        auto& r=island_.residents[static_cast<std::size_t>(id)];
        auto start=abstract_position(r);
        start.y=world::terrain::TerrainHeight::sample(start.x,start.z);
        const auto anchor=physical_anchor(r,start);
        characters::ResidentSpawn spawn;
        spawn.resident=r.id;spawn.character_id=r.character_id;spawn.position=start;spawn.anchor=anchor.point;
        spawn.anchor_radius=anchor.radius;spawn.health=r.health;
        spawn.pool=r.occupation==Occupation::PoliceOfficer || r.occupation==Occupation::Doctor ?
            characters::CharacterPool::Emergency : characters::CharacterPool::Civilian;
        spawn.yaw_degrees=std::atan2(anchor.point.x-start.x,anchor.point.z-start.z)*RAD2DEG;
        npcs.spawn_resident(spawn);
        r.physical=true;r.body=start;++bodies;
    }
    for(const auto& r:island_.residents) {
        if(!r.physical || !r.alive) continue;
        const auto anchor=physical_anchor(r,r.body);
        npcs.direct_resident(r.id,anchor.point,anchor.radius,r.fear>0);
    }
}

void LifeSimulation::update(float real_dt,Vector3 player,characters::NpcSystem& npcs,bool paused) {
    if(island_.residents.empty() || paused || !std::isfinite(real_dt) || real_dt<=0 ||
        !std::isfinite(player.x) || !std::isfinite(player.z)) return;
    real_dt=std::min(real_dt,.25F);
    clock_.advance(real_dt,config_.time_scale);
    // Round-robin re-planning: the whole island is revisited every replan_period seconds.
    const auto count=island_.residents.size();
    replan_budget_+=static_cast<float>(count)*real_dt/config_.replan_period;
    auto steps=std::min<std::size_t>(count,static_cast<std::size_t>(replan_budget_));
    replan_budget_-=static_cast<float>(steps);
    while(steps-->0) {replan(island_.residents[cursor_]);cursor_=(cursor_+1)%count;}
    bridge_clock_+=real_dt;
    if(bridge_clock_>=.25F) {bridge_clock_=0;sync_bodies(player,npcs);}
}

void LifeSimulation::report_gunfire(Vector3 position) {
    if(config_.hearing_radius<=0 || !std::isfinite(position.x) || !std::isfinite(position.z)) return;
    for(auto& r:island_.residents) {
        if(!r.alive) continue;
        const float distance=flat_distance(this->position(r),position);
        if(distance>config_.hearing_radius) continue;
        const float closeness=1-distance/config_.hearing_radius;
        r.fear=std::max(r.fear,.3F+.7F*closeness);
        r.shelter_until=std::max(r.shelter_until,clock_.minutes+config_.shelter_minutes*(.5+.5*closeness));
        replan(r);
    }
}

std::string LifeSimulation::describe(const Resident& r) const {
    std::string text=r.full_name()+", "+occupation_name(r.occupation);
    if(!r.alive) return text+" - dead";
    if(r.place<0) return text;
    const auto& place=island_.places[static_cast<std::size_t>(r.place)];
    const std::string town=island_.settlement_names[static_cast<std::size_t>(place.settlement)];
    const std::string where=(r.place==r.home ? std::string("home") : place.kind==PlaceKind::Home ? std::string("a home") :
        std::string("the ")+place_name(place.kind))+" in "+town;
    const bool moving=r.physical ? flat_distance(r.body,place.door)>8 && !inside_footprint(place,r.body) : travelling(r);
    if(moving) return text+" - on the way to "+where;
    if(r.activity==Activity::Work) return text+" - working at "+where;
    return text+" - "+activity_name(r.activity)+" ("+where+")";
}

const Resident* LifeSimulation::nearest(Vector3 point,float max_distance,bool physical_only) const {
    const Resident* best=nullptr;float best_distance=max_distance;
    for(const auto& r:island_.residents) {
        if(physical_only && !r.physical) continue;
        const float distance=flat_distance(position(r),point);
        if(distance<=best_distance) {best_distance=distance;best=&r;}
    }
    return best;
}

LifeStats LifeSimulation::stats() const {
    LifeStats s;s.population=static_cast<int>(island_.residents.size());
    for(const auto& r:island_.residents) {
        if(!r.alive) continue;
        ++s.alive;s.physical+=r.physical;s.sheltering+=r.activity==Activity::Shelter;
        if(indoors(r)) ++s.indoors; else if(!r.physical && travelling(r)) ++s.travelling;
    }
    return s;
}
}
