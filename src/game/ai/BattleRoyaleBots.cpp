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
    tracers_.clear();feed_.clear();events_={};elapsed_=0;
    player_kills_=0;
}

void BattleRoyaleBots::start(const world::VerdaRegion& region,BotLevel level,int count,std::uint64_t seed,Vector3 player_feet) {
    clear();
    level_=level;
    const auto& settlements=region.settlements();
    std::uint64_t state=seed|1;
    std::vector<Vector3> placed;
    const int attempts=std::max(1,count)*120;
    for(int attempt=0;attempt<attempts && static_cast<int>(bots_.size())<count;++attempt) {
        // Drop zones: around each town in turn, like players spreading out from the bus.
        Vector3 center{};
        if(!settlements.empty()) center=settlements[static_cast<std::size_t>(attempt)%settlements.size()].center;
        const float angle=unit(state)*2*PI, radius=25+unit(state)*190;
        Vector3 feet{center.x+std::sin(angle)*radius,0,center.z+std::cos(angle)*radius};
        const float terrain=world::terrain::TerrainHeight::sample(feet.x,feet.z);
        if(!std::isfinite(terrain) || terrain<=world::layout::sea_level+1) continue;
        feet.y=world::physics::WorldCollision::ground_height({feet.x,terrain,feet.z},terrain,region);
        if(world::physics::WorldCollision::body_blocked(feet,feet.y,region,.35F)) continue;
        if(Vector3Distance(feet,player_feet)<drop_clearance) continue;
        if(std::any_of(placed.begin(),placed.end(),[&](Vector3 p){return Vector3Distance(p,feet)<60;})) continue;
        const auto weapon=unit(state)<.6F ? combat::WeaponId::Rifle : combat::WeaponId::Pistol;
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

    // The circle: after the opening minute everyone drifts toward the capital, tightening over time.
    if(elapsed_>90) {
        const float radius=std::max(60.0F,700.0F-static_cast<float>(elapsed_-90)*1.5F);
        for(auto& bot:bots_) if(Vector3Distance(bot.position(),{0,0,0})>radius*.6F) bot.set_roam_center({0,0,0},radius*.6F);
    }

    agents_.clear();
    const auto player_velocity=frame.player_velocity;
    agents_.push_back({player_id,frame.player_feet,player_velocity,frame.player_alive});
    for(const auto& bot:bots_) agents_.push_back({bot.id(),bot.position(),bot.velocity(),bot.alive()});
    last_player_=frame.player_feet;

    auto& bus=world.sounds ? *world.sounds : own_bus_;
    bus.advance(frame.now);
    if(!world.sounds && frame.player_alive) {
        if(frame.player_fired)
            bus.emit(sound::SoundKind::Gunshot,frame.player_feet,frame.player_weapon==combat::WeaponId::Rifle ? gunshot_radius_rifle : gunshot_radius_pistol,player_id,frame.now);
        if(Vector3Length({player_velocity.x,0,player_velocity.z})>CombatBot::walk_speed+.6F)
            bus.emit(sound::SoundKind::Footstep,frame.player_feet,footstep_radius,player_id,frame.now);
    }
    for(const auto& bot:bots_) {
        // Running feet are audible close by; walking is quiet.
        if(bot.alive() && Vector3Length({bot.velocity().x,0,bot.velocity().z})>CombatBot::walk_speed+.6F)
            bus.emit(sound::SoundKind::Footstep,bot.position(),footstep_radius,bot.id(),frame.now);
    }
    // Everything new on the bus this frame; each bot judges its own earshot.
    sounds_.clear();
    for(const auto& event:bus.events())
        if(event.serial>heard_up_to_) sounds_.push_back({event.position,event.radius,event.source});
    heard_up_to_=bus.latest();

    shots_.clear();
    for(auto& bot:bots_) {
        if(!bot.alive()) continue;
        bot.tick(dt,frame.now,agents_,sounds_,world.environment,shots_);
    }
    for(const auto& shot:shots_) {
        resolve(shot,frame,world);
        // Heard by everyone from the next frame on.
        const auto* shooter=find(shot.shooter);
        bus.emit(sound::SoundKind::Gunshot,shooter ? shooter->position() : shot.origin,
            shot.weapon==combat::WeaponId::Rifle ? gunshot_radius_rifle : gunshot_radius_pistol,shot.shooter,frame.now);
    }
}

void BattleRoyaleBots::resolve(const BotShot& shot,const BattleRoyaleFrame& frame,const BotWorld& world) {
    ++events_.shots;
    const float range=shot.weapon==combat::WeaponId::Rifle ? 220.0F : 90.0F;
    const auto end=Vector3Add(shot.origin,Vector3Scale(shot.direction,range));
    combat::BulletHit world_hit;
    if(world.trace) world_hit=world.trace(shot.origin,end);
    float best=world_hit.hit() ? world_hit.fraction : 1.0F;
    int victim=-1;Vector3 victim_feet{};
    if(frame.player_alive && !frame.player_in_vehicle && shot.shooter!=player_id) {
        const float f=enter_body(shot.origin,end,frame.player_feet);
        if(f>=0 && f<best) {best=f;victim=player_id;victim_feet=frame.player_feet;}
    }
    Vector3 shooter_position=shot.origin;
    for(const auto& bot:bots_) {
        if(bot.id()==shot.shooter) {shooter_position=bot.position();continue;}
        if(!bot.alive()) continue;
        const float f=enter_body(shot.origin,end,bot.position());
        if(f>=0 && f<best) {best=f;victim=bot.id();victim_feet=bot.position();}
    }
    const auto impact=Vector3Lerp(shot.origin,end,best);
    tracers_.push_back({Vector3Add(shot.origin,Vector3Scale(shot.direction,.8F)),impact,tracer_seconds});

    if(victim<0) {
        if(world_hit.hit() && world.world_damage) world.world_damage(world_hit,shot.damage,shooter_position);
        return;
    }
    const float damage=shot.damage*(impact.y>victim_feet.y+head_height ? 2.0F : 1.0F);
    ++events_.hits;
    if(auto* shooter=find(shot.shooter)) shooter->on_hit_given(frame.now);
    if(victim==player_id) {
        events_.player_damage+=damage;events_.player_damage_from=shooter_position;events_.player_damage_by=shot.shooter;
        return;
    }
    auto* target=find(victim);
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
    }
}
}
