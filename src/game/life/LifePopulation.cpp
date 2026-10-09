#include "outland/game/life/LifePopulation.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace outland::game::life {
namespace {
enum class Identity { Capital, Rural, Port, Residential, Industrial, Generic };
Identity identity(const std::string& id) {
    if(id=="capital_verda") return Identity::Capital;
    if(id=="village_espera") return Identity::Rural;
    if(id=="port_luma") return Identity::Port;
    if(id=="south_haven") return Identity::Residential;
    if(id=="west_roka") return Identity::Industrial;
    return Identity::Generic;
}
struct Random {
    std::uint64_t state;
    std::uint64_t next() {state^=state<<13;state^=state>>7;state^=state<<17;return state;}
    float unit() {return static_cast<float>(next()%100000)/100000.0F;}
    int range(int low,int high) {return low+static_cast<int>(next()%static_cast<std::uint64_t>(high-low+1));}
    bool chance(float p) {return unit()<p;}
};
constexpr const char* male_names[]{"Ivan","Petro","Marko","Janko","Tomaso","Paulo","Luko","Stefano","Andreo","Mikaelo",
    "Jozefo","Karlo","Davido","Viktoro","Adamo","Bruno","Emilo","Frederiko","Gustavo","Henriko","Leono","Oskaro","Rikardo","Teodoro"};
constexpr const char* female_names[]{"Ana","Maria","Klara","Elena","Sofia","Lidia","Roza","Marta","Eva","Helena","Kata",
    "Vera","Irena","Julia","Nadia","Olga","Paula","Silvia","Tereza","Zofia","Agata","Berta","Dora","Lucia"};
constexpr const char* family_names[]{"Novak","Horvat","Kovac","Marin","Petrov","Babic","Rossi","Costa","Ferro","Varga",
    "Dobra","Stelo","Montero","Kardo","Lindo","Verdano","Ruzic","Moreno","Sorensen","Bianki","Tamas","Ostrova","Lazar","Pajic"};
template<class T,std::size_t N> const T& pick(const T (&values)[N],Random& random) {return values[random.next()%N];}

float flat_distance(Vector3 a,Vector3 b) {const float x=a.x-b.x,z=a.z-b.z;return std::sqrt(x*x+z*z);}
bool contains(const std::string& text,const char* word) {return text.find(word)!=std::string::npos;}

int nearest(const Island& island,Vector3 from,PlaceKind kind,int prefer_settlement=-1) {
    int best=-1;float best_score=std::numeric_limits<float>::max();
    for(std::size_t i=0;i<island.places.size();++i) {
        const auto& place=island.places[i];
        if(place.kind!=kind) continue;
        // Slight preference for one's own town keeps people local when a choice is close.
        const float score=flat_distance(from,place.door)*(place.settlement==prefer_settlement ? .6F : 1.0F);
        if(score<best_score) {best_score=score;best=static_cast<int>(i);}
    }
    return best;
}

Occupation occupation_for(PlaceKind kind) {
    switch(kind) {
        case PlaceKind::Garage:return Occupation::Mechanic;case PlaceKind::Industrial:return Occupation::FactoryWorker;
        case PlaceKind::Dock:return Occupation::Dockworker;case PlaceKind::Shop:return Occupation::Shopkeeper;
        case PlaceKind::Pub:return Occupation::Publican;case PlaceKind::Church:return Occupation::Pastor;
        case PlaceKind::Police:return Occupation::PoliceOfficer;case PlaceKind::Clinic:return Occupation::Doctor;
        case PlaceKind::Office:return Occupation::Clerk;case PlaceKind::Farm:return Occupation::Farmer;
        default:return Occupation::Unemployed;
    }
}
int job_capacity(PlaceKind kind) {
    switch(kind) {
        case PlaceKind::Garage:return 3;case PlaceKind::Industrial:return 6;case PlaceKind::Dock:return 8;
        case PlaceKind::Shop:return 2;case PlaceKind::Pub:return 2;case PlaceKind::Church:return 1;
        case PlaceKind::Police:return 6;case PlaceKind::Clinic:return 3;case PlaceKind::Office:return 4;
        case PlaceKind::Farm:return 6;default:return 0;
    }
}
int priority(Occupation occupation) {
    switch(occupation) {
        case Occupation::Pastor:return 0;case Occupation::PoliceOfficer:return 1;case Occupation::Publican:return 2;
        case Occupation::Shopkeeper:return 3;case Occupation::Doctor:return 4;case Occupation::Mechanic:return 5;
        default:return 6;
    }
}
bool keyword_kind(const std::string& id,PlaceKind& kind) {
    struct Rule {const char* word;PlaceKind kind;};
    static constexpr Rule rules[]{{"church",PlaceKind::Church},{"chapel",PlaceKind::Church},{"pub",PlaceKind::Pub},
        {"police",PlaceKind::Police},{"clinic",PlaceKind::Clinic},{"hall",PlaceKind::Office},{"dock",PlaceKind::Dock},
        {"garage",PlaceKind::Garage},{"shop",PlaceKind::Shop},{"factory",PlaceKind::Industrial},{"home",PlaceKind::Home}};
    for(const auto& rule:rules) if(contains(id,rule.word)) {kind=rule.kind;return true;}
    return false;
}

void build_places(Island& island,const world::VerdaRegion& region) {
    const auto& settlements=region.settlements();
    for(std::size_t s=0;s<settlements.size();++s) {
        const auto& town=settlements[s];
        island.settlement_names.push_back(town.name.empty() ? town.id : town.name);
        island.settlement_centers.push_back(town.center);
        const auto who=identity(town.id);
        const std::size_t first=island.places.size();
        std::vector<bool> named;
        for(const auto& building:town.buildings) {
            if(!std::isfinite(building.position.x) || !std::isfinite(building.position.z)) continue;
            Place place;place.id=building.id;place.building_id=building.id;place.settlement=static_cast<int>(s);
            place.interior=building.position;place.building_yaw=building.rotation_y;place.building_size=building.size;
            place.door=door_position(building.position,building.size,building.rotation_y,1.6F);
            place.indoor=building.enterable;
            bool explicit_kind=keyword_kind(building.id,place.kind);
            if(!explicit_kind) switch(building.style) {
                case world::BuildingStyle::Garage:place.kind=PlaceKind::Garage;break;
                case world::BuildingStyle::Warehouse:place.kind=who==Identity::Port ? PlaceKind::Dock : PlaceKind::Industrial;break;
                case world::BuildingStyle::Shop:place.kind=PlaceKind::Shop;break;
                default:place.kind=PlaceKind::Home;break;
            }
            if(!place.indoor && place.kind==PlaceKind::Home) continue;
            named.push_back(explicit_kind);
            island.places.push_back(std::move(place));
        }
        // Order this town's buildings from the centre outward for civic designation.
        std::vector<std::size_t> order;
        for(std::size_t i=first;i<island.places.size();++i) order.push_back(i);
        std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){
            return flat_distance(island.places[a].interior,town.center)<flat_distance(island.places[b].interior,town.center);});
        const auto has=[&](PlaceKind kind){
            for(std::size_t i=first;i<island.places.size();++i) if(island.places[i].kind==kind) return true;
            return false;
        };
        const auto homes=[&]{int n=0;for(std::size_t i=first;i<island.places.size();++i) n+=island.places[i].kind==PlaceKind::Home;return n;};
        // A second shop becomes the local pub before a house is converted.
        if(!has(PlaceKind::Pub)) {
            int shops=0;
            for(const auto i:order) if(island.places[i].kind==PlaceKind::Shop && !named[i-first] && ++shops==2) {island.places[i].kind=PlaceKind::Pub;break;}
        }
        // Working towns house their dockers and factory hands above the surplus shopfronts.
        std::vector<bool> tenement(island.places.size()-first,false);
        if(who==Identity::Port || who==Identity::Industrial) {
            int shops=0;
            for(const auto i:order) if(island.places[i].kind==PlaceKind::Shop && !named[i-first] && ++shops>1)
                {island.places[i].kind=PlaceKind::Home;tenement[i-first]=true;}
        }
        std::vector<PlaceKind> civic{PlaceKind::Church,PlaceKind::Pub};
        if(who==Identity::Capital) civic.insert(civic.end(),{PlaceKind::Police,PlaceKind::Clinic,PlaceKind::Office,PlaceKind::Shop});
        for(const auto kind:civic) {
            if(has(kind) || homes()<=2) continue;
            for(const auto i:order) if(island.places[i].kind==PlaceKind::Home && !named[i-first]) {island.places[i].kind=kind;break;}
        }
        for(std::size_t i=first;i<island.places.size();++i) {
            auto& place=island.places[i];
            const bool two_story=place.building_size.y>5.5F;
            if(place.kind==PlaceKind::Home) place.home_capacity=tenement[i-first] ? 6 : two_story ? 8 : 4;
            if(place.kind==PlaceKind::Shop || place.kind==PlaceKind::Pub) {place.mixed_home=true;place.home_capacity=3;}
            place.job_capacity=job_capacity(place.kind);
            place.id=town.id+":"+place_name(place.kind)+":"+place.building_id;
        }
        const auto outdoor=[&](PlaceKind kind,Vector3 offset,const char* name){
            Place place;place.kind=kind;place.settlement=static_cast<int>(s);place.indoor=false;
            place.interior=place.door=Vector3Add(town.center,offset);place.job_capacity=job_capacity(kind);
            place.id=town.id+":"+name;island.places.push_back(std::move(place));
        };
        outdoor(PlaceKind::Square,{4,0,4},"square");
        if(who==Identity::Rural) {
            // Fields sit between the village and the island interior, away from the north shore.
            const float length=std::max(1.0F,std::sqrt(town.center.x*town.center.x+town.center.z*town.center.z));
            outdoor(PlaceKind::Farm,{-town.center.x/length*70+30,0,-town.center.z/length*70},"fields");
        }
    }
}

std::vector<Block> day_plan(const Resident& r,Weekday day,int jitter) {
    using A=Activity;
    const bool weekday=day_type(day)==DayType::Weekday, saturday=day==Weekday::Saturday, sunday=day==Weekday::Sunday;
    const auto evening=r.pub_regular ? A::Socialize : A::Home;
    std::vector<Block> plan;
    const auto rest_saturday=[&]{plan={{0,A::Sleep},{510,A::Home},{600,A::Shopping},{690,A::Home},{840,A::Visit},{1050,A::Home},{1170,evening},{1410,A::Sleep}};};
    const auto rest_sunday=[&]{plan={{0,A::Sleep},{480,A::Home},{570,r.churchgoer ? A::Worship : A::Home},{690,A::Visit},{900,A::Stroll},{1020,A::Home},{1320,A::Sleep}};};
    switch(r.occupation) {
        case Occupation::Dockworker:
            if(weekday) plan={{0,A::Sleep},{270,A::Home},{330,A::Work},{660,A::Lunch},{705,A::Work},{870,evening},{1020,A::Home},{1260,A::Sleep}};
            else if(saturday) plan={{0,A::Sleep},{300,A::Home},{360,A::Work},{720,evening},{900,A::Home},{1080,A::Visit},{1260,A::Home},{1380,A::Sleep}};
            else rest_sunday();
            break;
        case Occupation::Shopkeeper:
            if(!sunday) plan={{0,A::Sleep},{420,A::Home},{510,A::Work},{780,A::Lunch},{840,A::Work},{1140,A::Home},{1380,A::Sleep}};
            else rest_sunday();
            break;
        case Occupation::Publican:
            if(day==Weekday::Monday) plan={{0,A::Sleep},{570,A::Home},{660,A::Shopping},{780,A::Home},{900,A::Visit},{1140,A::Home},{1320,A::Sleep}};
            else plan={{0,A::Work},{60,A::Sleep},{570,A::Home},{660,A::Work},{900,A::Home},{1020,A::Work}};
            break;
        case Occupation::Pastor:
            if(sunday) plan={{0,A::Sleep},{390,A::Home},{450,A::Work},{780,A::Visit},{960,A::Stroll},{1080,A::Home},{1320,A::Sleep}};
            else if(saturday) plan={{0,A::Sleep},{420,A::Home},{600,A::Work},{780,A::Home},{900,A::Visit},{1080,A::Home},{1320,A::Sleep}};
            else plan={{0,A::Sleep},{390,A::Home},{450,A::Work},{720,A::Lunch},{840,A::Visit},{1020,A::Work},{1140,A::Home},{1320,A::Sleep}};
            break;
        case Occupation::PoliceOfficer: {
            const int d=static_cast<int>(day);
            if(d==r.id%7 || d==(r.id+3)%7) {if(sunday) rest_sunday(); else rest_saturday();break;}
            if(r.shift==0) plan={{0,A::Sleep},{300,A::Home},{360,A::Patrol},{600,A::Work},{660,A::Patrol},{840,A::Home},{1080,evening},{1260,A::Home},{1320,A::Sleep}};
            else if(r.shift==1) plan={{0,A::Sleep},{480,A::Home},{660,A::Shopping},{750,A::Home},{840,A::Patrol},{1080,A::Work},{1140,A::Patrol},{1320,A::Home},{1410,A::Sleep}};
            else plan={{0,A::Patrol},{360,A::Home},{420,A::Sleep},{900,A::Home},{1080,A::Visit},{1260,A::Home},{1320,A::Patrol}};
            break;
        }
        case Occupation::Doctor:
            if(weekday || (saturday && r.id%2==0)) plan={{0,A::Sleep},{405,A::Home},{480,A::Work},{750,A::Lunch},{795,A::Work},{saturday ? 840 : 1080,A::Home},{1350,A::Sleep}};
            else if(saturday) rest_saturday(); else rest_sunday();
            break;
        case Occupation::Farmer:
            if(weekday) plan={{0,A::Sleep},{285,A::Home},{330,A::Work},{720,A::Home},{810,A::Work},{1140,A::Home},{1290,A::Sleep}};
            else if(saturday) plan={{0,A::Sleep},{285,A::Home},{330,A::Work},{660,A::Home},{900,A::Visit},{1080,evening},{1290,A::Sleep}};
            else rest_sunday();
            break;
        case Occupation::Retired:
            if(sunday) {rest_sunday();break;}
            plan={{0,A::Sleep},{390,A::Home},{570,A::Stroll},{660,A::Shopping},{720,A::Home},{900,r.pub_regular ? A::Socialize : A::Visit},{1080,A::Home},{1290,A::Sleep}};
            break;
        case Occupation::Unemployed:
            if(sunday) {rest_sunday();break;}
            plan={{0,A::Home},{60,A::Sleep},{570,A::Home},{690,A::Stroll},{780,A::Home},{960,evening},{1200,A::Home}};
            break;
        default: // mechanic, factory worker, clerk
            if(weekday) plan={{0,A::Sleep},{390,A::Home},{450,A::Work},{720,A::Lunch},{780,A::Work},{1020,evening},{1140,A::Home},{1350,A::Sleep}};
            else if(saturday) rest_saturday(); else rest_sunday();
            break;
    }
    // Friday night out for pub regulars who otherwise go home after work.
    if(day==Weekday::Friday && r.pub_regular && r.occupation!=Occupation::Publican && r.occupation!=Occupation::PoliceOfficer)
        for(auto& block:plan) if(block.minute>=1080 && block.activity==A::Home) {block.activity=A::Socialize;break;}
    // Krimulo members keep an ordinary day job; Friday and Saturday nights belong to the group.
    if(r.faction==Faction::Krimulo && (day==Weekday::Friday || day==Weekday::Saturday)) {
        std::erase_if(plan,[](const Block& b){return b.minute>=1320;});plan.push_back({1320,A::FactionMeet});
    }
    if(r.faction==Faction::Krimulo && (day==Weekday::Saturday || day==Weekday::Sunday)) {
        for(auto& block:plan) if(block.minute==0) block.activity=A::FactionMeet;
        if(std::none_of(plan.begin(),plan.end(),[](const Block& b){return b.minute==150;})) plan.push_back({150,A::Sleep});
        std::erase_if(plan,[](const Block& b){return b.minute>0 && b.minute<150;});
    }
    std::sort(plan.begin(),plan.end(),[](const Block& a,const Block& b){return a.minute<b.minute;});
    // Personal timing so a town does not move in lockstep; midnight blocks stay anchored.
    int last=0;
    for(auto& block:plan) {
        if(block.minute>0) block.minute=std::clamp(block.minute+jitter,last+5,1439);
        last=block.minute;
    }
    return plan;
}

const characters::CharacterDefinition* choose_model(const characters::CharacterRegistry& registry,Occupation occupation,Random& random) {
    const char* category=occupation==Occupation::PoliceOfficer ? "emergency_police" : occupation==Occupation::Doctor ? "emergency_doctor" : "civilian";
    std::vector<const characters::CharacterDefinition*> matches;
    for(const auto& asset:registry.assets()) if(asset.role=="npc" && asset.category==category) matches.push_back(&asset);
    if(matches.empty()) for(const auto& asset:registry.assets()) if(asset.role=="npc" && asset.pool==characters::CharacterPool::Civilian) matches.push_back(&asset);
    return matches.empty() ? nullptr : matches[random.next()%matches.size()];
}
}

Vector3 door_position(Vector3 center,Vector3 size,float yaw_degrees,float outside) {
    // Inverse of WorldCollision's local frame; the doorway is centred in the -Z (front) wall.
    const float angle=yaw_degrees*DEG2RAD, local_z=-(size.z*.5F+outside);
    return {center.x+local_z*std::sin(angle),center.y,center.z+local_z*std::cos(angle)};
}
bool inside_footprint(const Place& place,Vector3 position,float margin) {
    if(!place.indoor || place.building_id.empty()) return false;
    const float angle=place.building_yaw*DEG2RAD, dx=position.x-place.interior.x, dz=position.z-place.interior.z;
    const float local_x=dx*std::cos(angle)-dz*std::sin(angle), local_z=dx*std::sin(angle)+dz*std::cos(angle);
    return std::abs(local_x)<place.building_size.x*.5F-margin && std::abs(local_z)<place.building_size.z*.5F-margin;
}
Activity planned_activity(const Resident& resident,Weekday day,int minute) {
    const auto& plan=resident.days[static_cast<std::size_t>(day)];
    Activity activity=Activity::Home;
    for(const auto& block:plan) {if(block.minute>minute) break;activity=block.activity;}
    return activity;
}

Island build_island(const world::VerdaRegion& region,const characters::CharacterRegistry* registry,PopulationOptions options) {
    Island island;
    build_places(island,region);
    Random random{options.seed|1};
    const float density=std::isfinite(options.density) ? std::clamp(options.density,.05F,20.0F) : 1;
    const auto& settlements=region.settlements();
    // Households: each town fills its homes up to its population (or capacity when unset).
    for(std::size_t s=0;s<settlements.size();++s) {
        std::vector<int> homes;int capacity=0;
        for(std::size_t i=0;i<island.places.size();++i)
            if(island.places[i].settlement==static_cast<int>(s) && island.places[i].home_capacity>0) {homes.push_back(static_cast<int>(i));capacity+=island.places[i].home_capacity;}
        if(homes.empty()) continue;
        const int stated=settlements[s].survivors;
        const int target=std::max(1,static_cast<int>(std::lround((stated>0 ? std::min(stated,capacity) : capacity)*density)));
        int remaining=target;
        for(std::size_t h=0;h<homes.size() && remaining>0;++h) {
            const auto& home=island.places[static_cast<std::size_t>(homes[h])];
            const int share=h+1==homes.size() ? remaining :
                std::clamp(static_cast<int>(std::lround(static_cast<float>(target)*home.home_capacity/capacity)),1,remaining);
            const std::string family=pick(family_names,random);
            std::vector<int> members;
            for(int m=0;m<share;++m) {
                Resident resident;resident.id=static_cast<int>(island.residents.size());
                resident.family_name=family;resident.settlement=static_cast<int>(s);resident.home=homes[h];
                resident.female=m%2==1 ? !island.residents.back().female : random.chance(.5F);
                members.push_back(resident.id);island.residents.push_back(std::move(resident));
            }
            for(const int id:members) island.residents[static_cast<std::size_t>(id)].household=members;
            remaining-=share;
        }
    }
    // Retirees first, then essential jobs go to the nearest available working-age resident.
    for(auto& resident:island.residents) if(random.chance(.16F)) resident.occupation=Occupation::Retired;
    struct Slot {int place;Occupation occupation;};
    std::vector<Slot> slots;
    for(std::size_t i=0;i<island.places.size();++i)
        for(int n=0;n<static_cast<int>(std::lround(island.places[i].job_capacity*std::max(1.0F,density)));++n)
            slots.push_back({static_cast<int>(i),occupation_for(island.places[i].kind)});
    std::stable_sort(slots.begin(),slots.end(),[](const Slot& a,const Slot& b){return priority(a.occupation)<priority(b.occupation);});
    std::vector<bool> employed(island.residents.size(),false), taken(slots.size(),false);
    const auto home_door=[&](const Resident& r){return island.places[static_cast<std::size_t>(r.home)].door;};
    const auto hire=[&](std::size_t slot,int id){
        auto& worker=island.residents[static_cast<std::size_t>(id)];
        employed[static_cast<std::size_t>(id)]=taken[slot]=true;
        worker.occupation=slots[slot].occupation;worker.work=slots[slot].place;
    };
    // Essential posts (pastor, police, publican, shopkeeper, doctor) go to the nearest resident.
    for(std::size_t i=0;i<slots.size() && priority(slots[i].occupation)<=4;++i) {
        int best=-1;float best_distance=std::numeric_limits<float>::max();
        for(const auto& resident:island.residents) {
            if(employed[static_cast<std::size_t>(resident.id)] || resident.occupation==Occupation::Retired) continue;
            const float d=flat_distance(home_door(resident),island.places[static_cast<std::size_t>(slots[i].place)].door);
            if(d<best_distance) {best_distance=d;best=resident.id;}
        }
        if(best>=0) hire(i,best);
    }
    // Everyone else of working age takes the nearest open job; a few are out of work.
    for(const auto& resident:island.residents) {
        if(employed[static_cast<std::size_t>(resident.id)] || resident.occupation==Occupation::Retired || random.chance(.07F)) continue;
        int best=-1;float best_distance=std::numeric_limits<float>::max();
        for(std::size_t i=0;i<slots.size();++i) {
            if(taken[i]) continue;
            const float d=flat_distance(home_door(resident),island.places[static_cast<std::size_t>(slots[i].place)].door);
            if(d<best_distance) {best_distance=d;best=static_cast<int>(i);}
        }
        if(best>=0) hire(static_cast<std::size_t>(best),resident.id);
    }
    int police=0;
    for(auto& r:island.residents) {
        const auto& home=island.places[static_cast<std::size_t>(r.home)];
        r.first_name=r.female ? pick(female_names,random) : pick(male_names,random);
        const Vector3 base=r.work>=0 && r.occupation==Occupation::Dockworker ? island.places[static_cast<std::size_t>(r.work)].door : home.door;
        r.pub=nearest(island,base,PlaceKind::Pub,r.settlement);
        r.church=nearest(island,home.door,PlaceKind::Church,r.settlement);
        r.shop=nearest(island,home.door,PlaceKind::Shop,r.settlement);
        r.lunch=r.home;
        if(r.work>=0) {
            const auto& work=island.places[static_cast<std::size_t>(r.work)];
            if(work.kind==PlaceKind::Pub || work.kind==PlaceKind::Shop) r.lunch=r.work;
            else for(const auto kind:{PlaceKind::Pub,PlaceKind::Shop}) {
                const int option=nearest(island,work.door,kind,work.settlement);
                if(option>=0 && flat_distance(island.places[static_cast<std::size_t>(option)].door,work.door)<400) {r.lunch=option;break;}
            }
        }
        r.churchgoer=r.occupation==Occupation::Pastor || random.chance(r.occupation==Occupation::Retired ? .8F : .45F);
        r.pub_regular=r.occupation!=Occupation::Pastor && random.chance(.35F);
        r.works_weekends=r.occupation==Occupation::Shopkeeper || r.occupation==Occupation::Publican ||
            r.occupation==Occupation::PoliceOfficer || r.occupation==Occupation::Farmer;
        if(r.occupation==Occupation::PoliceOfficer) {
            r.faction=Faction::Police;r.shift=police%3;
            r.patrol_settlement=static_cast<int>(police%std::max<std::size_t>(1,settlements.size()));++police;
        }
        const auto town=identity(settlements[static_cast<std::size_t>(r.settlement)].id);
        const bool candidate=r.occupation==Occupation::Dockworker || r.occupation==Occupation::FactoryWorker ||
            r.occupation==Occupation::Mechanic || r.occupation==Occupation::Unemployed;
        if(candidate && (town==Identity::Industrial || town==Identity::Port) && random.chance(.22F)) r.faction=Faction::Krimulo;
    }
    // Krimulo gather at the outermost works in Roka (or any works/garage if Roka is missing).
    int hangout=-1;float far=-1;
    for(std::size_t i=0;i<island.places.size();++i) {
        const auto& place=island.places[i];
        if(place.kind!=PlaceKind::Industrial && place.kind!=PlaceKind::Garage) continue;
        const bool roka=identity(settlements[static_cast<std::size_t>(place.settlement)].id)==Identity::Industrial;
        const float score=(roka ? 10000.0F : 0.0F)+flat_distance(place.interior,island.settlement_centers[static_cast<std::size_t>(place.settlement)]);
        if(score>far) {far=score;hangout=static_cast<int>(i);}
    }
    // Relatives: half of households have family in another home, often in another town.
    std::vector<int> heads;
    for(const auto& r:island.residents) if(r.household.front()==r.id) heads.push_back(r.id);
    for(const int head:heads) {
        auto& self=island.residents[static_cast<std::size_t>(head)];
        if(heads.size()<2) break;
        int other=self.id;
        if(random.chance(.55F)) while(other==self.id) other=heads[random.next()%heads.size()];
        else {
            std::vector<int> local;
            for(const int h:heads) if(h!=self.id && island.residents[static_cast<std::size_t>(h)].settlement==self.settlement) local.push_back(h);
            if(!local.empty()) other=local[random.next()%local.size()];
        }
        if(other==self.id) continue;
        const auto their_home=island.residents[static_cast<std::size_t>(other)].home;
        const auto their_household=island.residents[static_cast<std::size_t>(other)].household;
        const auto our_household=self.household;
        const bool family=random.chance(.7F);
        for(const int id:our_household) {
            auto& member=island.residents[static_cast<std::size_t>(id)];
            member.visit_home=their_home;
            if(family) member.relatives.insert(member.relatives.end(),their_household.begin(),their_household.end());
        }
        if(family) for(const int id:their_household) {
            auto& member=island.residents[static_cast<std::size_t>(id)];
            member.relatives.insert(member.relatives.end(),our_household.begin(),our_household.end());
            if(member.visit_home<0) member.visit_home=self.home;
        }
    }
    for(auto& r:island.residents) {
        if(r.faction==Faction::Krimulo) r.hangout=hangout;
        const int jitter=random.range(-25,25);
        for(int d=0;d<7;++d) r.days[static_cast<std::size_t>(d)]=day_plan(r,static_cast<Weekday>(d),jitter);
        if(registry) if(const auto* model=choose_model(*registry,r.occupation,random)) {
            r.character_id=model->id;r.female=model->name.find("Female")!=std::string::npos;
            r.first_name=r.female ? pick(female_names,random) : pick(male_names,random);
        }
        r.place=r.home;r.from=r.body=island.places[static_cast<std::size_t>(r.home)].interior;
    }
    return island;
}
}
