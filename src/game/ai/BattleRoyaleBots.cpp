#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::ai {
namespace {
constexpr float body_half_width=.35F, body_height=1.85F, head_height=1.45F;
constexpr float tracer_seconds=.09F;
constexpr std::size_t feed_lines=3;

std::uint64_t mix(std::uint64_t x) {
    x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;
    return x^(x>>31);
}
float unit(std::uint64_t& state) { state=mix(state);return static_cast<float>(state>>40)/static_cast<float>(1ULL<<24); }

// Segment fraction where start->end enters a standing body's box, or a negative value.
float enter_body(Vector3 start,Vector3 end,Vector3 feet) {
    const float lo[3]{feet.x-body_half_width,feet.y,feet.z-body_half_width};
    const float hi[3]{feet.x+body_half_width,feet.y+body_height,feet.z+body_half_width};
    const float origin[3]{start.x,start.y,start.z}, delta[3]{end.x-start.x,end.y-start.y,end.z-start.z};
    float enter=0,exit=1;
    for(int axis=0;axis<3;++axis) {
        if(std::abs(delta[axis])<1e-6F) {
            if(origin[axis]<lo[axis] || origin[axis]>hi[axis]) return -1;
            continue;
        }
        float a=(lo[axis]-origin[axis])/delta[axis], b=(hi[axis]-origin[axis])/delta[axis];
        if(a>b) std::swap(a,b);
        enter=std::max(enter,a);exit=std::min(exit,b);
        if(enter>exit) return -1;
    }
    return enter;
}
std::string bot_name(int id) { return "BOT "+std::to_string(id); }
}

CombatBot& BattleRoyaleBots::add_bot(Vector3 feet,combat::WeaponId weapon,BotLevel level,std::uint64_t seed) {
    const int id=static_cast<int>(bots_.size())+1;
    bots_.emplace_back(id,level,weapon,mix(seed+static_cast<std::uint64_t>(id)));
    auto& bot=bots_.back();
    bot.set_position(feet);bot.set_roam_center(feet,60);
    level_=level;
    return bot;
}

void BattleRoyaleBots::clear() {
    bots_.clear();agents_.clear();sounds_.clear();own_bus_.clear();heard_up_to_=0;shots_.clear();
    tracers_.clear();feed_.clear();events_={};elapsed_=0;mounted_.clear();asleep_.clear();hidden_.clear();lag_.clear();
    player_kills_=0;
}

void BattleRoyaleBots::start(const world::VerdaRegion& region,BotLevel level,int count,std::uint64_t seed,Vector3 player_feet,
                             const std::function<bool(Vector3)>& prefer) {
    clear();
    level_=level;
    const auto& settlements=region.settlements();
    std::uint64_t state=seed|1;
    std::vector<Vector3> placed;
    const int attempts=std::max(1,count)*120;
    // First only where `prefer` allows (in reach of the helicopters' line); then anywhere.
    for(int pass=prefer ? 0 : 1;pass<2;++pass)
    for(int attempt=0;attempt<attempts && static_cast<int>(bots_.size())<count;++attempt) {
        // Drop zones: around each town in turn, like players spreading out from the bus.
        Vector3 center{};
        if(!settlements.empty()) center=settlements[static_cast<std::size_t>(attempt)%settlements.size()].center;
        const float angle=unit(state)*2*PI, radius=25+unit(state)*190;
        Vector3 feet{center.x+std::sin(angle)*radius,0,center.z+std::cos(angle)*radius};
        if(pass==0 && !prefer(feet)) continue;
        const float terrain=world::terrain::TerrainHeight::sample(feet.x,feet.z);
        if(!std::isfinite(terrain) || terrain<=world::layout::sea_level+1) continue;
        feet.y=world::physics::WorldCollision::ground_height({feet.x,terrain,feet.z},terrain,region);
        if(world::physics::WorldCollision::body_blocked(feet,feet.y,region,.35F)) continue;
        if(Vector3Distance(feet,player_feet)<drop_clearance) continue;
        if(std::any_of(placed.begin(),placed.end(),[&](Vector3 p){return Vector3Distance(p,feet)<60;})) continue;
        // Loadouts roughly as found in a drop: mostly rifles, then SMGs, pistols, shotguns (pump,
        // double barrel, automatic), a few snipers. No bot carries a rocket launcher.
        const float pick=unit(state);
        const auto weapon=pick<.40F ? combat::WeaponId::Rifle : pick<.60F ? combat::WeaponId::Smg :
            pick<.78F ? combat::WeaponId::Pistol : pick<.85F ? combat::WeaponId::Shotgun :
            pick<.89F ? combat::WeaponId::DoubleBarrel : pick<.92F ? combat::WeaponId::AutoShotgun : combat::WeaponId::Sniper;
        auto& bot=add_bot(feet,weapon,level,state);
        bot.set_roam_center(center,140);
        placed.push_back(feet);
    }
}

int BattleRoyaleBots::alive_bots() const {
    return static_cast<int>(std::count_if(bots_.begin(),bots_.end(),[](const CombatBot& b){return b.alive();}));
}

CombatBot* BattleRoyaleBots::find(int bot) {
    for(auto& b:bots_) if(b.id()==bot) return &b;
    return nullptr;
}

bool BattleRoyaleBots::damage_bot(int bot,float amount,Vector3 from,double now) {
    auto* target=find(bot);
    if(!target || !target->alive() || !std::isfinite(amount) || amount<=0) return false;
    target->set_health(std::max(0.0F,target->health()-amount));
    target->on_damage(amount,from,now);
    if(target->alive()) return false;
    ++player_kills_;
    feed_.push_back("YOU ELIMINATED "+bot_name(bot));
    if(feed_.size()>feed_lines) feed_.erase(feed_.begin());
    return true;
}

bool BattleRoyaleBots::damage_bot_by(int bot,float amount,Vector3 from,double now,const std::string& who) {
    auto* target=find(bot);
    if(!target || !target->alive() || !std::isfinite(amount) || amount<=0) return false;
    target->set_health(std::max(0.0F,target->health()-amount));
    target->on_damage(amount,from,now);
    if(target->alive()) return false;
    add_feed(who+" ELIMINATED "+bot_name(bot));
    return true;
}
void BattleRoyaleBots::credit_kill(int bot) {
    ++player_kills_;
    add_feed("YOU ELIMINATED "+bot_name(bot));
}
void BattleRoyaleBots::credit_player_kill(const std::string& victim) {
    ++player_kills_;
    add_feed("YOU ELIMINATED "+victim);
}
void BattleRoyaleBots::add_feed(const std::string& line) {
    feed_.push_back(line);
    if(feed_.size()>feed_lines) feed_.erase(feed_.begin());
}

void BattleRoyaleBots::apply_circle(Vector2 center, float radius, Vector2 next_center, float next_radius, float damage) {
    for(auto& bot:bots_) if(bot.alive()) {
        const Vector3 at=bot.position();
        const float here=std::hypot(at.x-center.x,at.z-center.y), there=std::hypot(at.x-next_center.x,at.z-next_center.y);
        // Run for the next circle before the wall arrives (or for this one if already caught).
        if(here>radius*.85F || there>next_radius*.9F) {
            const bool caught=here>radius;
            const Vector2 goal=caught ? center : next_center;
            const float room=caught ? radius : next_radius;
            bot.set_roam_center({goal.x,0,goal.y}, std::max(10.0F,room*.5F));
        }
        if(here>radius) {
            bot.set_health(std::max(0.0F,bot.health()-damage));
            if(!bot.alive()) {
                feed_.push_back(bot_name(bot.id())+" LOST TO THE ZONE");
                if(feed_.size()>feed_lines) feed_.erase(feed_.begin());
            }
        }
    }
}

void BattleRoyaleBots::set_mounted(int bot,bool mounted) {
    std::erase(mounted_,bot);
    if(mounted) mounted_.push_back(bot);
}
bool BattleRoyaleBots::mounted(int bot) const {
    return std::find(mounted_.begin(),mounted_.end(),bot)!=mounted_.end();
}

void BattleRoyaleBots::set_asleep(int bot,bool asleep) {
    std::erase(asleep_,bot);
    if(asleep) asleep_.push_back(bot);
}
bool BattleRoyaleBots::asleep(int bot) const {
    return std::find(asleep_.begin(),asleep_.end(),bot)!=asleep_.end();
}

void BattleRoyaleBots::set_hidden(int bot,bool hidden) {
    std::erase(hidden_,bot);
    if(hidden) hidden_.push_back(bot);
}

bool BattleRoyaleBots::hidden(int bot) const {
    return std::find(hidden_.begin(),hidden_.end(),bot)!=hidden_.end();
}

void BattleRoyaleBots::respawn(int bot,Vector3 feet,float yaw,std::uint64_t seed) {
    std::erase(hidden_,bot);
    for(auto& brain:bots_) {
        if(brain.id()!=bot) continue;
        brain=CombatBot(bot,brain.level(),brain.weapon(),mix(seed+static_cast<std::uint64_t>(bot)));
        brain.set_position(feet);brain.set_yaw(yaw);brain.set_roam_center(feet,60);
        set_mounted(bot,false);
        return;
    }
}

void BattleRoyaleBots::retire(int bot) {
    if(auto* brain=find(bot)) brain->set_health(0);
    set_asleep(bot,true);
}

void BattleRoyaleBots::report_player_death(int killer) {
    feed_.push_back(killer>0 ? bot_name(killer)+" ELIMINATED YOU" : std::string("YOU WERE ELIMINATED"));
    if(feed_.size()>feed_lines) feed_.erase(feed_.begin());
}

void BattleRoyaleBots::update(const BattleRoyaleFrame& frame,const BotWorld& world) {
    events_={};
    if(!std::isfinite(frame.dt) || frame.dt<=0 || bots_.empty()) return;
    const float dt=std::min(frame.dt,.25F);
    elapsed_+=dt;
    for(auto& tracer:tracers_) tracer.life-=dt;
    std::erase_if(tracers_,[](const BotTracer& t){return t.life<=0;});

    agents_.clear();
    const auto player_velocity=frame.player_velocity;
    agents_.push_back({player_id,frame.player_feet,player_velocity,frame.player_alive});
    for(const auto& other:frame.others) agents_.push_back({other.id,other.feet,other.velocity,other.alive && !other.in_vehicle});
    for(const auto& bot:bots_) agents_.push_back({bot.id(),bot.position(),bot.velocity(),bot.alive() && !mounted(bot.id())});
    last_player_=frame.player_feet;

    auto& bus=world.sounds ? *world.sounds : own_bus_;
    bus.advance(frame.now);
    if(!world.sounds && frame.player_alive) {
        if(frame.player_fired)
            bus.emit(sound::SoundKind::Gunshot,frame.player_feet,combat::definition(frame.player_weapon).hearing,player_id,frame.now);
        if(Vector3Length({player_velocity.x,0,player_velocity.z})>CombatBot::walk_speed+.6F)
            bus.emit(sound::SoundKind::Footstep,frame.player_feet,footstep_radius,player_id,frame.now);
    }
    for(const auto& bot:bots_) {
        // Running feet are audible close by; walking is quiet.
        if(bot.alive() && !mounted(bot.id()) && !asleep(bot.id()) && Vector3Length({bot.velocity().x,0,bot.velocity().z})>CombatBot::walk_speed+.6F)
            bus.emit(sound::SoundKind::Footstep,bot.position(),footstep_radius,bot.id(),frame.now);
    }
    // Everything new on the bus this frame; each bot judges its own earshot.
    sounds_.clear();
    for(const auto& event:bus.events())
        if(event.serial>heard_up_to_) sounds_.push_back({event.position,event.radius,event.source});
    heard_up_to_=bus.latest();

    shots_.clear();
    // Bots far from the player think and move in steps: one frame in four each (taking turns),
    // with the time they skipped. Near ones every frame. A hundred bots stay cheap.
    ++frame_;
    lag_.resize(bots_.size(),0);
    for(std::size_t i=0;i<bots_.size();++i) {
        auto& bot=bots_[i];
        if(!bot.alive() || mounted(bot.id()) || asleep(bot.id())) {lag_[i]=0;continue;}
        lag_[i]+=dt;
        bool far=std::hypot(bot.position().x-frame.player_feet.x,bot.position().z-frame.player_feet.z)>far_radius;
        for(const auto& other:frame.others) far=far && std::hypot(bot.position().x-other.feet.x,bot.position().z-other.feet.z)>far_radius;
        // ...but never miss a sound in earshot (it is only on the bus this frame).
        const auto earshot=[&] {
            return std::any_of(sounds_.begin(),sounds_.end(),[&](const BotSound& heard) {
                return heard.source!=bot.id() && Vector3Distance(heard.position,bot.position())<heard.radius*1.5F;});
        };
        if(far && (frame_+i)%far_step!=0 && lag_[i]<.2F && !earshot()) continue;
        bot.tick(std::min(lag_[i],.25F),frame.now,agents_,sounds_,world.environment,shots_);
        lag_[i]=0;
    }
    for(const auto& shot:shots_) {
        resolve(shot,frame,world);
        // Heard by everyone from the next frame on.
        const auto* shooter=find(shot.shooter);
        bus.emit(sound::SoundKind::Gunshot,shooter ? shooter->position() : shot.origin,
            combat::definition(shot.weapon).hearing,shot.shooter,frame.now);
    }
}

void BattleRoyaleBots::resolve(const BotShot& shot,const BattleRoyaleFrame& frame,const BotWorld& world) {
    ++events_.shots;
    const float range=std::max(90.0F,combat::definition(shot.weapon).bot_range*1.6F);
    const auto end=Vector3Add(shot.origin,Vector3Scale(shot.direction,range));
    combat::BulletHit world_hit;
    if(world.trace) world_hit=world.trace(shot.origin,end);
    float best=world_hit.hit() ? world_hit.fraction : 1.0F;
    int victim=-1;Vector3 victim_feet{};
    if(frame.player_alive && !frame.player_in_vehicle && shot.shooter!=player_id) {
        const float f=enter_body(shot.origin,end,frame.player_feet);
        if(f>=0 && f<best) {best=f;victim=player_id;victim_feet=frame.player_feet;}
    }
    for(const auto& other:frame.others) {
        if(!other.alive || other.in_vehicle || other.id==shot.shooter) continue;
        const float f=enter_body(shot.origin,end,other.feet);
        if(f>=0 && f<best) {best=f;victim=other.id;victim_feet=other.feet;}
    }
    Vector3 shooter_position=shot.origin;
    for(const auto& bot:bots_) {
        if(bot.id()==shot.shooter) {shooter_position=bot.position();continue;}
        if(!bot.alive() || mounted(bot.id())) continue;
        // With sides (the Mafio crews), a gang's rounds pass its own people.
        if(world.environment.hostile && !world.environment.hostile(shot.shooter,bot.id())) continue;
        const float f=enter_body(shot.origin,end,bot.position());
        if(f>=0 && f<best) {best=f;victim=bot.id();victim_feet=bot.position();}
    }
    const auto impact=Vector3Lerp(shot.origin,end,best);
    tracers_.push_back({Vector3Add(shot.origin,Vector3Scale(shot.direction,.8F)),impact,tracer_seconds});
    if(frame.player_alive && victim!=player_id && shot.shooter!=player_id) {
        // Shots at (or past) the player that miss are heard going by.
        const Vector3 head{frame.player_feet.x,frame.player_feet.y+1.6F,frame.player_feet.z};
        const Vector3 path=Vector3Subtract(impact,shot.origin);
        const float length=Vector3Length(path);
        if(length>1e-3F) {
            const float along=std::clamp(Vector3DotProduct(Vector3Subtract(head,shot.origin),path)/(length*length),0.0F,1.0F);
            const Vector3 closest=Vector3Add(shot.origin,Vector3Scale(path,along));
            if(along<1 && along>0 && Vector3Distance(closest,head)<3.0F) events_.near_misses.push_back(closest);
        }
        if(victim<0 && world_hit.hit() && Vector3Distance(impact,head)<8.0F) events_.ricochets.push_back(impact);
    }

    if(victim<0) {
        if(world_hit.hit() && world.world_damage) world.world_damage(world_hit,shot.damage,shooter_position);
        return;
    }
    // Same body zones as the player's bullets: heads take the weapon's multiplier, legs less.
    const auto& gun=combat::definition(shot.weapon);
    const float damage=shot.damage*(impact.y>victim_feet.y+head_height ? gun.head_multiplier : impact.y<victim_feet.y+.85F ? gun.limb_multiplier : 1.0F);
    ++events_.hits;
    if(auto* shooter=find(shot.shooter)) shooter->on_hit_given(frame.now);
    if(victim==player_id) {
        events_.player_damage+=damage;events_.player_damage_from=shooter_position;events_.player_damage_by=shot.shooter;
        return;
    }
    auto* target=find(victim);
    if(!target) {events_.other_hits.push_back({victim,damage,shooter_position,shot.shooter});return;}
    target->set_health(std::max(0.0F,target->health()-damage));
    target->on_damage(damage,shooter_position,frame.now);
    if(!target->alive()) {
        ++events_.kills;
        feed_.push_back(bot_name(shot.shooter)+" ELIMINATED "+bot_name(victim));
        if(feed_.size()>feed_lines) feed_.erase(feed_.begin());
    }
}

void BattleRoyaleBots::sync_bodies(characters::NpcSystem& npcs,const characters::CharacterRegistry* registry) const {
    for(const auto& bot:bots_) {
        if(npcs.find_bot(bot.id())<0) {
            std::string character="bot";
            if(registry) {
                const auto seed=mix(static_cast<std::uint64_t>(bot.id())*7919);
                const auto* definition=registry->choose(characters::CharacterPool::Hostile,seed);
                if(!definition) definition=registry->choose(characters::CharacterPool::Civilian,seed);
                if(definition) character=definition->id;
            }
            npcs.spawn_bot(bot.id(),character,bot.position(),bot.yaw());
        }
        const float speed=Vector3Length({bot.velocity().x,0,bot.velocity().z});
        const auto state=!bot.alive() ? characters::NpcState::Dead :
            (bot.moving() && speed>CombatBot::walk_speed+.6F) ? characters::NpcState::Chase :
            bot.moving() ? characters::NpcState::Wander : characters::NpcState::Idle;
        npcs.set_bot(bot.id(),bot.position(),bot.yaw(),bot.health(),state);
        npcs.set_bot_weapon(bot.id(),static_cast<int>(bot.weapon()));
        npcs.set_bot_hidden(bot.id(),mounted(bot.id()) || hidden(bot.id()));
    }
}
}
