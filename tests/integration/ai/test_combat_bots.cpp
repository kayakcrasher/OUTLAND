#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace outland;
using namespace outland::game::ai;
using outland::game::combat::WeaponId;
namespace {
void check(bool condition,const std::string& message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
// A flat test range with box walls; feet at y=0.
struct Box { Vector3 lo, hi; };
struct Range {
    std::vector<Box> walls;
    bool frozen{false};
    bool segment_blocked(Vector3 a,Vector3 b) const {
        for(const auto& box:walls) {
            const float lo[3]{box.lo.x,box.lo.y,box.lo.z}, hi[3]{box.hi.x,box.hi.y,box.hi.z};
            const float o[3]{a.x,a.y,a.z}, d[3]{b.x-a.x,b.y-a.y,b.z-a.z};
            float enter=0,exit=1;bool hit=true;
            for(int i=0;i<3 && hit;++i) {
                if(std::abs(d[i])<1e-6F) {if(o[i]<lo[i] || o[i]>hi[i]) hit=false;continue;}
                float t0=(lo[i]-o[i])/d[i], t1=(hi[i]-o[i])/d[i];
                if(t0>t1) std::swap(t0,t1);
                enter=std::max(enter,t0);exit=std::min(exit,t1);
                if(enter>exit) hit=false;
            }
            if(hit) return true;
        }
        return false;
    }
    float trace(Vector3 a,Vector3 b) const {
        // Coarse but adequate: bisect the first blocked fraction.
        if(!segment_blocked(a,b)) return 1;
        float lo=0,hi=1;
        for(int i=0;i<24;++i) {const float mid=(lo+hi)*.5F;if(segment_blocked(a,Vector3Lerp(a,b,mid))) hi=mid;else lo=mid;}
        return hi;
    }
    BotEnvironment environment() const {
        BotEnvironment env;
        env.line_of_sight=[this](Vector3 a,Vector3 b){return !segment_blocked(a,b);};
        env.move=[this](Vector3 from,Vector3 to) {
            if(frozen) return from;
            for(const auto& box:walls)
                if(to.x>box.lo.x-.35F && to.x<box.hi.x+.35F && to.z>box.lo.z-.35F && to.z<box.hi.z+.35F && box.hi.y>.5F) return from;
            return to;
        };
        return env;
    }
    BotWorld world() const {
        BotWorld w;w.environment=environment();
        w.trace=[this](Vector3 a,Vector3 b) {
            game::combat::BulletHit hit;const float f=trace(a,b);
            if(f<1) {hit.kind=game::combat::HitKind::Building;hit.fraction=f;hit.position=Vector3Lerp(a,b,f);}
            return hit;
        };
        return w;
    }
};
std::vector<BotAgent> agents_with(const CombatBot& bot,Vector3 enemy,bool enemy_alive=true,Vector3 velocity={}) {
    return {{0,enemy,velocity,enemy_alive},{bot.id(),bot.position(),bot.velocity(),true}};
}
// Seconds until the bot fires at a standing enemy straight ahead, or -1.
float first_shot(BotLevel level,std::uint64_t seed) {
    Range range;range.frozen=true;
    CombatBot bot(1,level,WeaponId::Rifle,seed);bot.set_yaw(0);
    const auto env=range.environment();
    std::vector<BotShot> shots;
    for(int i=0;i<200;++i) {
        bot.tick(.02F,i*.02,agents_with(bot,{0,0,30}),{},env,shots);
        if(!shots.empty()) return static_cast<float>(i+1)*.02F;
    }
    return -1;
}
struct Duel { int shots{0}, hits{0}; };
Duel duel(BotLevel level,std::uint64_t seed,const Range& range,Vector3 player,float seconds) {
    BattleRoyaleBots match;
    auto& bot=match.add_bot({0,0,0},WeaponId::Rifle,level,seed);bot.set_yaw(0);
    const auto world=range.world();
    Duel result;
    for(float t=0;t<seconds;t+=.02F) {
        BattleRoyaleFrame frame;frame.dt=.02F;frame.now=t;frame.player_feet=player;
        match.update(frame,world);
        result.shots+=match.events().shots;result.hits+=match.events().hits;
        // Keep the bot healthy and the duel going; we are measuring the gun, not the fight.
        match.bots()[0].set_health(100);
    }
    return result;
}
bool exposed(Vector3 feet,Vector3 threat,const Range& range) {
    return !range.segment_blocked({threat.x,threat.y+1.55F,threat.z},{feet.x,feet.y+1.2F,feet.z});
}
}

int main() {
    // ---- Difficulty: thinking and shooting only; never body, gun or senses ----
    const auto easy=bot_difficulty(BotLevel::Easy),medium=bot_difficulty(BotLevel::Medium),hard=bot_difficulty(BotLevel::Hard);
    check(easy.reaction>medium.reaction && medium.reaction>hard.reaction,"reaction ordering");
    check(easy.aim_error_deg>medium.aim_error_deg && medium.aim_error_deg>hard.aim_error_deg,"aim ordering");
    check(easy.aim_floor_deg>medium.aim_floor_deg && medium.aim_floor_deg>hard.aim_floor_deg,"aim floor ordering");
    check(easy.decision_quality<medium.decision_quality && medium.decision_quality<hard.decision_quality,"decision quality ordering");
    check(easy.aggression<medium.aggression && medium.aggression<hard.aggression,"aggression ordering");
    check(easy.view_distance==hard.view_distance && easy.fov_deg==hard.fov_deg && easy.hearing_scale==hard.hearing_scale,"same senses at every level");
    for(const auto level:{BotLevel::Easy,BotLevel::Medium,BotLevel::Hard}) {
        CombatBot bot(1,level,WeaponId::Rifle,7);
        check(bot.health()==100 && bot.loaded()==30 && bot.reserve()==120 && bot.medkits()==1,"same body and kit at every level");
    }

    // ---- No knowledge through walls ----
    {
        Range range;range.frozen=true;range.walls.push_back({{-6,0,9},{6,3,10}});
        CombatBot bot(1,BotLevel::Hard,WeaponId::Rifle,3);bot.set_yaw(0);
        const auto env=range.environment();std::vector<BotShot> shots;
        for(int i=0;i<300;++i) bot.tick(.02F,i*.02,agents_with(bot,{0,0,20}),{},env,shots);
        check(bot.contacts().empty() && shots.empty(),"a hidden enemy is unknown");
        range.walls.clear();
        for(int i=300;i<500;++i) bot.tick(.02F,i*.02,agents_with(bot,{0,0,20}),{},env,shots);
        check(!bot.contacts().empty() && !shots.empty(),"the same enemy in the open is seen and shot");
    }
    // ---- Field of view: an enemy behind is not seen ----
    {
        Range range;range.frozen=true;
        CombatBot bot(1,BotLevel::Hard,WeaponId::Rifle,4);bot.set_yaw(0);
        const auto env=range.environment();std::vector<BotShot> shots;
        for(int i=0;i<100;++i) {bot.set_yaw(0);bot.tick(.02F,i*.02,agents_with(bot,{0,0,-25}),{},env,shots);}
        check(bot.contacts().empty(),"enemy behind is outside the field of view");
    }
    // ---- Hearing: a gunshot behind turns the bot around, then sight takes over ----
    {
        Range range;range.frozen=true;
        CombatBot bot(1,BotLevel::Medium,WeaponId::Rifle,5);bot.set_yaw(0);
        const auto env=range.environment();std::vector<BotShot> shots;
        const std::vector<BotSound> gunshot{{{0,0,-60},BattleRoyaleBots::gunshot_radius_rifle,0}};
        bot.tick(.02F,0,agents_with(bot,{0,0,-60}),gunshot,env,shots);
        check(bot.contacts().size()==1 && !bot.contacts()[0].visible,"a gunshot creates an unseen contact");
        const auto heard=bot.contacts()[0].last_seen;
        check(Vector3Distance(heard,{0,0,-60})>0 && Vector3Distance(heard,{0,0,-60})<60*.2F,"hearing is approximate");
        for(int i=1;i<250;++i) bot.tick(.02F,i*.02,agents_with(bot,{0,0,-60}),{},env,shots);
        check(std::abs(std::abs(bot.yaw())-180)<15,"bot turned toward the gunshot");
        check(!shots.empty(),"after turning, the bot sees and engages");
        // Far beyond earshot, nothing is heard.
        CombatBot deaf(2,BotLevel::Hard,WeaponId::Rifle,6);deaf.set_yaw(0);
        const std::vector<BotSound> distant{{{0,0,-500},BattleRoyaleBots::gunshot_radius_rifle,0}};
        deaf.tick(.02F,0,agents_with(deaf,{0,0,-500}),distant,env,shots);
        check(deaf.contacts().empty(),"a gunshot out of earshot is not heard");
    }
    // ---- Reaction time: easy notices later than hard, nobody fires before reacting ----
    {
        float sums[3]{};
        const BotLevel levels[3]{BotLevel::Easy,BotLevel::Medium,BotLevel::Hard};
        for(int l=0;l<3;++l) for(std::uint64_t seed=1;seed<=6;++seed) {
            const float t=first_shot(levels[l],seed);
            check(t>0,"bot fires at an enemy in the open");
            check(t>=bot_difficulty(levels[l]).reaction-.03F,"no shot before the reaction time");
            sums[l]+=t;
        }
        std::cout<<"first shot (s): easy "<<sums[0]/6<<" medium "<<sums[1]/6<<" hard "<<sums[2]/6<<'\n';
        check(sums[0]>sums[1] && sums[1]>sums[2],"reaction ordering in play");
    }
    // ---- Accuracy: easy < medium < hard; nothing hits through a wall ----
    {
        Range open;
        double rate[3]{};
        const BotLevel levels[3]{BotLevel::Easy,BotLevel::Medium,BotLevel::Hard};
        for(int l=0;l<3;++l) {
            int shots=0,hits=0;
            for(std::uint64_t seed=1;seed<=4;++seed) {const auto d=duel(levels[l],seed,open,{0,0,40},12);shots+=d.shots;hits+=d.hits;}
            check(shots>20,"bots shoot in a duel");
            rate[l]=static_cast<double>(hits)/shots;
        }
        std::cout<<"hit rate at 40 m: easy "<<rate[0]<<" medium "<<rate[1]<<" hard "<<rate[2]<<'\n';
        check(rate[0]<rate[1] && rate[1]<rate[2],"accuracy ordering");
        check(rate[2]<.95,"even hard bots miss sometimes");
        Range walled;walled.walls.push_back({{-30,0,19},{30,4,21}});
        const auto blocked=duel(BotLevel::Hard,1,walled,{0,0,40},10);
        check(blocked.hits==0 && blocked.shots==0,"no shots or hits through a wall");
    }
    // ---- Cover: badly hurt and under fire, the bot breaks line of sight ----
    {
        int hidden=0;
        for(std::uint64_t seed=1;seed<=6;++seed) {
            Range range;range.walls.push_back({{3,0,4},{5,2.5F,6}}); // a low wall to the side
            const Vector3 threat{0,0,30};
            CombatBot bot(1,BotLevel::Medium,WeaponId::Rifle,seed);bot.set_yaw(0);bot.set_health(30);
            const auto env=range.environment();std::vector<BotShot> shots;
            bool took_cover=false;
            for(int i=0;i<200;++i) {
                if(i%25==0) bot.on_damage(5,threat,i*.02);
                bot.tick(.02F,i*.02,agents_with(bot,threat),{},env,shots);
                took_cover|=bot.intent()==BotIntent::TakeCover;
            }
            if(took_cover && !exposed(bot.position(),threat,range)) ++hidden;
        }
        check(hidden>=4,"wounded bots under fire take cover ("+std::to_string(hidden)+"/6)");
    }
    // ---- Reload: an empty magazine is refilled from reserve ----
    {
        Range range;range.frozen=true;
        CombatBot bot(1,BotLevel::Hard,WeaponId::Pistol,9);bot.set_yaw(0);
        const auto env=range.environment();std::vector<BotShot> shots;
        bool reloaded=false;
        for(int i=0;i<1500 && !reloaded;++i) {
            bot.tick(.02F,i*.02,agents_with(bot,{0,0,15}),{},env,shots);
            reloaded=bot.reserve()<60;
        }
        check(reloaded && shots.size()>=12 && bot.loaded()==12,"pistol empties and reloads");
    }
    // ---- Heal: hurt, alone and unhurt for a while, the bot uses its medkit ----
    {
        Range range;range.frozen=true;
        CombatBot bot(1,BotLevel::Medium,WeaponId::Rifle,10);bot.set_health(40);
        const auto env=range.environment();std::vector<BotShot> shots;
        bool healing=false;
        for(int i=0;i<400;++i) {
            bot.tick(.02F,10+i*.02,{{0,{0,0,500},{},true},{1,bot.position(),{},true}},{},env,shots);
            healing|=bot.intent()==BotIntent::Heal;
        }
        check(healing && bot.medkits()==0 && std::abs(bot.health()-85)<.01F,"bot heals with its medkit");
        // Damage interrupts healing.
        CombatBot hurt(2,BotLevel::Medium,WeaponId::Rifle,11);hurt.set_health(40);
        for(int i=0;i<60;++i) hurt.tick(.02F,10+i*.02,{{0,{0,0,500},{},true},{2,hurt.position(),{},true}},{},env,shots);
        check(hurt.healing(),"bandaging started");
        hurt.on_damage(5,{0,0,500},11.2);
        check(!hurt.healing(),"damage interrupts healing");
    }
    // ---- Push / flank: a healthy aggressive bot chases an enemy who ducked out of sight ----
    {
        int closed=0;
        for(std::uint64_t seed=1;seed<=5;++seed) {
            Range range;
            CombatBot bot(1,BotLevel::Hard,WeaponId::Rifle,seed);bot.set_yaw(0);
            const auto env=range.environment();std::vector<BotShot> shots;
            const Vector3 enemy{0,0,50};
            for(int i=0;i<60;++i) bot.tick(.02F,i*.02,agents_with(bot,enemy),{},env,shots);
            range.walls.push_back({{-4,0,46},{4,3,47}}); // the enemy steps behind a wall
            bool pressed=false;
            const float before=Vector3Distance(bot.position(),enemy);
            for(int i=60;i<360;++i) {
                bot.tick(.02F,i*.02,agents_with(bot,enemy),{},env,shots);
                pressed|=bot.intent()==BotIntent::Push || bot.intent()==BotIntent::Flank;
            }
            if(pressed && Vector3Distance(bot.position(),enemy)<before-5) ++closed;
        }
        check(closed>=4,"aggressive bots push or flank a hidden enemy ("+std::to_string(closed)+"/5)");
    }
    // ---- Retreat: dying, no medkit, enemy close: get away ----
    {
        int fled=0;
        for(std::uint64_t seed=1;seed<=5;++seed) {
            Range range;
            CombatBot bot(1,BotLevel::Medium,WeaponId::Pistol,seed);bot.set_yaw(0);bot.set_health(15);bot.give_medkits(-1);
            const auto env=range.environment();std::vector<BotShot> shots;
            const Vector3 enemy{0,0,12};
            bool retreated=false;
            for(int i=0;i<150;++i) {
                if(i%30==0) bot.on_damage(3,enemy,i*.02);
                bot.tick(.02F,i*.02,agents_with(bot,enemy),{},env,shots);
                retreated|=bot.intent()==BotIntent::Retreat;
            }
            if(retreated && Vector3Distance(bot.position(),enemy)>16) ++fled;
        }
        check(fled>=4,"dying bots retreat ("+std::to_string(fled)+"/5)");
    }
    // ---- Bots fight each other; the player is not special ----
    {
        Range range;
        BattleRoyaleBots match;
        auto& a=match.add_bot({0,0,0},WeaponId::Rifle,BotLevel::Medium,1);a.set_yaw(0);
        auto& b=match.add_bot({3,0,35},WeaponId::Rifle,BotLevel::Medium,2);b.set_yaw(180);
        const auto world=range.world();
        int kills=0;
        for(float t=0;t<60 && match.alive_bots()>1;t+=.02F) {
            BattleRoyaleFrame frame;frame.dt=.02F;frame.now=t;frame.player_feet={0,0,900};frame.player_alive=false;
            match.update(frame,world);kills+=match.events().kills;
        }
        check(match.alive_bots()==1 && kills==1 && !match.feed().empty(),"one bot wins a bot duel");
        std::cout<<"duel: "<<match.feed().back()<<" after "<<match.elapsed()<<" s\n";
    }
    // ---- The player's damage and deaths ----
    {
        Range range;
        BattleRoyaleBots match;
        auto& bot=match.add_bot({0,0,0},WeaponId::Rifle,BotLevel::Hard,3);bot.set_yaw(0);
        check(!match.damage_bot(1,40,{0,0,20},0) && match.bots()[0].health()==60,"player damages a bot");
        check(match.damage_bot(1,80,{0,0,20},0) && match.alive_bots()==0 && match.player_kills()==1,"player eliminates a bot");
        check(!match.damage_bot(1,10,{0,0,20},0),"dead bots take no damage");
        BattleRoyaleBots hunt;
        auto& hunter=hunt.add_bot({0,0,0},WeaponId::Rifle,BotLevel::Hard,4);hunter.set_yaw(0);
        float damage=0;
        for(float t=0;t<8;t+=.02F) {
            BattleRoyaleFrame frame;frame.dt=.02F;frame.now=t;frame.player_feet={0,0,25};
            hunt.update(frame,range.world());damage+=hunt.events().player_damage;
        }
        check(damage>0 && hunt.events().player_damage_by<=1,"bots shoot the player");
        hunt.report_player_death(1);
        check(hunt.feed().back()=="BOT 1 ELIMINATED YOU","death in the feed");
        check(!hunt.tracers().empty() || damage>0,"tracers recorded");
    }

    // ---- Bot bodies in NpcSystem ----
    {
        Range range;
        BattleRoyaleBots match;
        match.add_bot({0,0,0},WeaponId::Rifle,BotLevel::Medium,1);
        match.add_bot({0,0,200},WeaponId::Pistol,BotLevel::Medium,2);
        characters::NpcSystem npcs;
        match.sync_bodies(npcs,nullptr);
        check(npcs.find_bot(1)>=0 && npcs.find_bot(2)>=0,"bot bodies spawned");
        const auto far=npcs.trace_segment({0,1.2F,190},{0,1.2F,210});
        check(far.actor==npcs.find_bot(2),"bullets find bot bodies at long range");
        check(!npcs.damage(static_cast<std::size_t>(far.actor),50,{0,0,0}),"NpcSystem leaves bot health to the bot");
        match.damage_bot(2,200,{0,0,0},0);match.sync_bodies(npcs,nullptr);
        check(npcs.actors()[static_cast<std::size_t>(npcs.find_bot(2))].state==characters::NpcState::Dead,"dead bot body");
        check(npcs.trace_segment({0,1.2F,190},{0,1.2F,210}).actor<0,"dead bodies don't stop bullets");
        world::VerdaRegion empty(false);
        characters::CharacterRegistry registry;
        npcs.reconcile(empty,registry);
        check(npcs.find_bot(1)>=0,"reconcile keeps bot bodies");
        npcs.reset_session();
        check(npcs.find_bot(1)<0,"session reset removes bot bodies");
    }

    // ---- Real Verda: drop the field on land ----
    {
        world::VerdaRegion region(true);
        BattleRoyaleBots match,again;
        const Vector3 player{0,world::terrain::TerrainHeight::sample(0,0),0};
        match.start(region,BotLevel::Medium,23,99,player);
        again.start(region,BotLevel::Medium,23,99,player);
        check(match.bots().size()==23,"23 bots dropped ("+std::to_string(match.bots().size())+")");
        int rifles=0;
        for(std::size_t i=0;i<match.bots().size();++i) {
            const auto& bot=match.bots()[i];
            const auto p=bot.position();
            check(Vector3Distance(p,again.bots()[i].position())<.001F,"deterministic drop");
            check(world::terrain::TerrainHeight::sample(p.x,p.z)>world::layout::sea_level,"bot on land");
            check(!world::physics::WorldCollision::body_blocked(p,p.y,region,.35F),"bot not inside geometry");
            check(Vector3Distance(p,player)>=BattleRoyaleBots::drop_clearance,"bots drop away from the player");
            rifles+=bot.weapon()==WeaponId::Rifle;
        }
        check(rifles>0 && rifles<23,"mixed loadouts");
        // A few seconds of the real island with real collision and sight.
        game::combat::CombatWorld combat(region);combat.set_training_range(false);
        BotWorld world;
        world.environment.line_of_sight=[&](Vector3 a,Vector3 b){return !combat.trace_segment(a,b,false,false,false).hit();};
        world.trace=[&](Vector3 a,Vector3 b){return combat.trace_segment(a,b,false,false,false);};
        world.environment.move=[&](Vector3 from,Vector3 to) {
            auto p=world::physics::WorldCollision::resolve_body_movement(from,to,from.y,region,.35F);
            p.y=world::physics::WorldCollision::ground_height(p,from.y,region);
            return p;
        };
        for(float t=0;t<5;t+=.05F) {
            BattleRoyaleFrame frame;frame.dt=.05F;frame.now=t;frame.player_feet=player;
            match.update(frame,world);
        }
        for(const auto& bot:match.bots()) check(std::isfinite(bot.position().x) && std::isfinite(bot.position().y),"bots stay finite");
    }
    std::cout<<"combat bot tests passed\n";
    return 0;
}
