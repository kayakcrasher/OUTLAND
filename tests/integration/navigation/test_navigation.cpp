#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ai/CombatBot.hpp"
#include "outland/game/sound/SoundBus.hpp"
#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/navigation/NavGrid.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
using world::navigation::NavGrid;
using world::navigation::NavPath;
using world::navigation::PathFollower;
using world::physics::WorldCollision;
namespace {
void check(bool condition,const std::string& message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
Vector3 world_of(const dev::KitBuilding& b,float x,float z,float y=0) {
    const float a=b.yaw*DEG2RAD;
    return {b.origin.x+x*std::cos(a)+z*std::sin(a),b.origin.y+y,b.origin.z-x*std::sin(a)+z*std::cos(a)};
}
float flat(Vector3 a,Vector3 b) {return std::hypot(a.x-b.x,a.z-b.z);}
float length(const NavPath& p) {float d=0;for(std::size_t i=1;i<p.points.size();++i) d+=Vector3Distance(p.points[i-1],p.points[i]);return d;}
// Walk a real body (NPC collision) along a path with a follower; returns the final feet position.
Vector3 walk(const world::VerdaRegion& region,NavGrid& grid,Vector3 feet,Vector3 goal,float seconds) {
    PathFollower follower;
    const world::navigation::PathFinder finder=[&](Vector3 a,Vector3 b,NavPath& p){return grid.find_path(a,b,p);};
    double now=0;float stuck=0;
    for(float t=0;t<seconds && flat(feet,goal)+std::abs(feet.y-goal.y)>.3F;t+=.05F,now+=.05) {
        const auto target=follower.steer(feet,goal,now,finder);
        auto delta=Vector3Subtract(target,feet);delta.y=0;
        const float d=Vector3Length(delta);
        if(d<.001F) continue;
        const float step=std::min(d,2.0F*.05F);
        const auto origin=feet;
        const auto wanted=Vector3Add(feet,Vector3Scale(delta,step/d));
        for(int i=1;i<=4;++i) {
            feet=WorldCollision::resolve_body_movement(feet,Vector3Lerp(origin,wanted,i/4.0F),feet.y,region,.35F);
            feet.y=WorldCollision::ground_height(feet,feet.y,region);
        }
        // Like every real mover: walking straight into something asks for a route.
        stuck=flat(feet,origin)<step*.3F ? stuck+.05F : 0;
        if(stuck>.6F) {stuck=0;follower.repath_soon();}
    }
    return feet;
}
}
int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    const creator::CreatorAssetRegistry catalog;
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region,std::string(OUTLAND_SOURCE_DIR)+"/maps/verda_loot_defaults.map"),"load defaults");
    const auto kit=dev::build_verda_towns(region,catalog);
    world::physics::MeshCollisionLibrary::preload(region);
    NavGrid grid(region);

    // ---- Open ground: a straight, complete path ----
    {
        const Vector3 a{40,0,500},b{60,0,520};
        Vector3 fa=a,fb=b;fa.y=world::terrain::TerrainHeight::sample(a.x,a.z);fb.y=world::terrain::TerrainHeight::sample(b.x,b.z);
        NavPath p;
        if(grid.walkable(fa) && grid.walkable(fb)) {
            check(grid.find_path(fa,fb,p) && p.complete,"open ground path");
            check(length(p)<Vector3Distance(fa,fb)*1.15F && p.points.size()<=4,"open ground path is nearly straight ("+std::to_string(p.points.size())+" points)");
        }
    }

    // ---- Into a building and up its stairs ----
    int upstairs=0,checked=0;
    double worst_ms=0;
    for(const auto& b:kit.enterable) {
        if(b.storeys<2 || checked>=8) continue;
        ++checked;
        auto street=world_of(b,b.door_x,-b.depth*.5F-4);
        street.y=WorldCollision::ground_height(street,b.origin.y+.5F,region);
        const auto top=Vector3Add(b.stairs_start,Vector3Scale(b.stairs_direction,b.stairs_length));
        const auto upper=world_of(b,top.x+b.upstairs_walk.x,top.z+b.upstairs_walk.z,b.upper_floor);
        check(grid.walkable(upper),"upper floor is walkable");
        NavPath p;
        const auto t0=std::chrono::steady_clock::now();
        const bool ok=grid.find_path(street,upper,p);
        worst_ms=std::max(worst_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count());
        check(ok && p.complete,"path from the street to the upper floor of building at "+std::to_string(b.origin.x)+","+std::to_string(b.origin.z)+
            " ("+std::to_string(p.expansions)+" expansions)");
        // The path goes through the door, not through a wall or window.
        bool climbs=false;for(const auto& q:p.points) climbs|=q.y>b.origin.y+1.5F;
        check(climbs && std::abs(p.points.back().y-upper.y)<.2F,"path climbs to the upper floor");
        // A real body following it gets there.
        const auto arrived=walk(region,grid,street,upper,60);
        if(flat(arrived,upper)<.6F && std::abs(arrived.y-upper.y)<.3F) ++upstairs;
        else {
            for(const auto& q:p.points) std::cerr<<"    ("<<q.x<<","<<q.y<<","<<q.z<<")\n";
            std::cerr<<"    stairs start local "<<b.stairs_start.x<<","<<b.stairs_start.z<<" dir "<<b.stairs_direction.x<<","<<b.stairs_direction.z<<" yaw "<<b.yaw<<"\n";
            std::cerr<<"  body stopped at "<<arrived.x<<","<<arrived.y<<","<<arrived.z<<" for "<<upper.x<<","<<upper.y<<","<<upper.z<<'\n';}
    }
    std::cout<<"street -> upper floor: "<<upstairs<<"/"<<checked<<" walked, worst search "<<worst_ms<<" ms (cold)\n";
    check(checked>=6 && upstairs==checked,"bodies follow paths upstairs");

    // ---- Around a building: the straight line is blocked, the path goes round ----
    {
        const auto& b=kit.enterable.front();
        auto front=world_of(b,0,-b.depth*.5F-5);front.y=WorldCollision::ground_height(front,b.origin.y+.5F,region);
        auto back=world_of(b,0,b.depth*.5F+5);back.y=WorldCollision::ground_height(back,b.origin.y+.5F,region);
        NavPath p;
        check(grid.find_path(front,back,p) && p.complete,"path round (or through) a building");
        const auto arrived=walk(region,grid,front,back,90);
        check(flat(arrived,back)<.6F,"body reaches the far side");
    }

    // ---- Unreachable: the sea. Partial paths head toward the goal. ----
    {
        Vector3 shore{},sea{};bool found=false;
        for(float r=1800;r<2400 && !found;r+=10) {
            const float h=world::terrain::TerrainHeight::sample(r,0);
            if(h<=world::layout::sea_level) {sea={r+60,world::layout::sea_level,0};shore={r-40,0,0};shore.y=world::terrain::TerrainHeight::sample(shore.x,0);found=true;}
        }
        check(found,"found the coast");
        check(!grid.walkable(sea),"the sea is not walkable");
        NavPath p;
        if(grid.find_path(shore,sea,p)) {
            check(!p.complete,"no complete path into the sea");
            check(flat(p.points.back(),sea)<flat(shore,sea),"a partial path heads toward the goal");
        }
    }
    // ---- Followers re-plan when the goal moves, not every frame ----
    {
        PathFollower follower;int plans=0;
        const world::navigation::PathFinder finder=[&](Vector3 a,Vector3 b,NavPath& p){++plans;return grid.find_path(a,b,p);};
        const dev::KitBuilding* tall=nullptr;
        for(const auto& k:kit.enterable) if(k.storeys>=2 && !tall) tall=&k;
        const auto& b=*tall;
        auto from=world_of(b,b.door_x,-b.depth*.5F-4);from.y=WorldCollision::ground_height(from,b.origin.y+.5F,region);
        auto goal=world_of(b,b.door_x+1.5F,-b.depth*.5F-4);goal.y=from.y;
        const auto top=Vector3Add(b.stairs_start,Vector3Scale(b.stairs_direction,b.stairs_length));
        const auto far=world_of(b,top.x+b.upstairs_walk.x,top.z+b.upstairs_walk.z,b.upper_floor); // another floor: routed
        follower.steer(from,far,0,finder);follower.steer(from,far,.1,finder);follower.steer(from,far,.2,finder);
        check(plans==1,"one plan for a still goal");
        follower.steer(from,Vector3Add(far,{0,0,-2}),.3,finder);check(plans==1,"re-planning is rate limited");
        follower.steer(from,Vector3Add(far,{0,0,-2}),1.2,finder);check(plans==2,"moved goal re-planned");
        // Same-level goals walk straight until the mover reports being stuck.
        PathFollower level;int level_plans=0;
        const world::navigation::PathFinder counting=[&](Vector3 a,Vector3 c,NavPath& p){++level_plans;return grid.find_path(a,c,p);};
        const auto ahead=world_of(b,b.door_x,-b.depth*.5F-14,0);
        check(Vector3Distance(level.steer(from,ahead,0,counting),ahead)<1e-4F && level_plans==0,"same-level goal walked straight");
        level.repath_soon();level.steer(from,ahead,.1,counting);
        check(level_plans==1 && level.routing(),"stuck mover gets a route");
        check(Vector3Distance(follower.steer(from,goal,2.5,finder),goal)<1e-4F || flat(from,goal)>=PathFollower::direct_range,"close goals are walked straight");
    }
    // ---- A cell budget spreads a cold search over several calls that reach the same answer ----
    {
        NavGrid cold(region);
        const dev::KitBuilding* tall=nullptr;
        for(const auto& k:kit.enterable) if(k.storeys>=2 && !tall) tall=&k;
        check(tall!=nullptr,"a multi-storey building");
        const auto& b=*tall;
        auto street=world_of(b,b.door_x,-b.depth*.5F-4);street.y=WorldCollision::ground_height(street,b.origin.y+.5F,region);
        const auto top=Vector3Add(b.stairs_start,Vector3Scale(b.stairs_direction,b.stairs_length));
        const auto inside=world_of(b,top.x+b.upstairs_walk.x,top.z+b.upstairs_walk.z,b.upper_floor);
        NavPath full;check(cold.find_path(street,inside,full) && full.complete,"unbudgeted path");
        NavGrid budgeted(region);budgeted.set_max_new_cells(150);
        NavPath p;int calls=0;
        do {budgeted.find_path(street,inside,p);++calls;} while(!p.complete && calls<200);
        check(p.complete && calls>1,"budgeted search completes over "+std::to_string(calls)+" calls");
        check(std::abs(length(p)-length(full))<.01F,"same route either way");
    }
    // ---- Movers use routes: a bot and a hostile NPC hear a noise upstairs and go up there ----
    {
        const dev::KitBuilding* tall=nullptr;
        for(const auto& k:kit.enterable) if(k.storeys>=2 && !tall) tall=&k;
        const auto& b=*tall;
        auto street=world_of(b,b.door_x,-b.depth*.5F-4);street.y=WorldCollision::ground_height(street,b.origin.y+.5F,region);
        const auto top=Vector3Add(b.stairs_start,Vector3Scale(b.stairs_direction,b.stairs_length));
        const auto upper=world_of(b,top.x+b.upstairs_walk.x,top.z+b.upstairs_walk.z,b.upper_floor);
        const auto walk_body=[&](Vector3 current,Vector3 desired) {
            const int steps=std::max(1,static_cast<int>(std::ceil(Vector3Distance(current,desired)/.2F)));
            const auto origin=current;
            for(int i=1;i<=steps;++i) current=WorldCollision::resolve_body_movement(current,Vector3Lerp(origin,desired,static_cast<float>(i)/steps),current.y,region,.35F);
            current.y=WorldCollision::ground_height(current,current.y,region);
            return current;
        };
        const world::navigation::PathFinder finder=[&](Vector3 a,Vector3 c,NavPath& p){return grid.find_path(a,c,p);};
        for(const bool routed:{false,true}) {
            game::ai::CombatBot bot(1,game::ai::BotLevel::Hard,game::combat::WeaponId::Rifle,3);
            bot.set_position(street);bot.set_roam_center(street,1);
            game::ai::BotEnvironment env;env.line_of_sight=[](Vector3,Vector3){return false;};env.move=walk_body;
            if(routed) env.find_path=finder;
            std::vector<game::ai::BotShot> shots;
            const std::vector<game::ai::BotAgent> agents{{1,street,{},true}};
            std::vector<game::ai::BotSound> noise{{upper,40,-1}};
            for(int i=0;i<1500;++i) {
                bot.tick(.04F,i*.04,agents,noise,env,shots);noise.clear();
                if(bot.intent()!=game::ai::BotIntent::Investigate && i>20) break;
            }
            // Hearing is approximate (about 12% of the distance), so "there" means on that floor, near the noise.
            const bool there=std::abs(bot.position().y-upper.y)<.3F && flat(bot.position(),upper)<4.0F;
            std::cout<<"bot "<<(routed ? "with" : "without")<<" routes ends at "<<bot.position().x<<","<<bot.position().y<<","<<bot.position().z<<'\n';
            if(routed) check(there,"a routed bot investigates up the stairs");
            else check(!there,"without routes the bot cannot get upstairs (the test is meaningful)");
        }
        characters::NpcSystem npcs;npcs.set_path_finder(finder);
        characters::ResidentSpawn hunter;hunter.resident=9;hunter.character_id="h";hunter.pool=characters::CharacterPool::Hostile;
        hunter.position=hunter.anchor=street;
        npcs.spawn_resident(hunter);
        game::sound::SoundBus bus;
        characters::NpcContext context;context.player_position={0,0,9000};context.sounds=&bus;
        npcs.update(.1F,context,region);
        bus.emit(game::sound::SoundKind::Gunshot,upper,300,4,0);
        // The hunter keeps searching while it remembers the shot; re-shoot to keep the memory fresh.
        for(int i=0;i<600;++i) {
            if(i%40==0) bus.emit(game::sound::SoundKind::Gunshot,upper,300,4,i*.1);
            bus.advance(i*.1);npcs.update(.1F,context,region);
        }
        const auto& h=npcs.actors()[static_cast<std::size_t>(npcs.find_resident(9))];
        check(std::abs(h.position.y-upper.y)<.3F && flat(h.position,upper)<2.5F,"a hostile NPC follows the noise upstairs (at "+
            std::to_string(h.position.x)+","+std::to_string(h.position.y)+","+std::to_string(h.position.z)+")");
    }
    std::cout<<grid.evaluated_cells()<<" cells evaluated in "<<grid.cached_tiles()<<" tiles\n";
    std::cout<<"navigation tests passed\n";
}
