#include "outland/game/ModeSession.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaLayout.hpp"
#include <algorithm>
#include <cmath>
namespace outland::game {
void ModeSession::start(GameMode mode, int bots, ai::BotLevel level) {
    mode_=mode; phase_=SessionPhase::Playing; elapsed_=0;
    others_alive_=0; run_bots_=true;
    result_={0,0,std::max(0,bots)+1,level};
    centers_.assign(circle.phases.size(),Vector2{0,0});
}
void ModeSession::plan_circles(std::uint64_t seed, const std::function<bool(float,float)>& on_land) {
    centers_.assign(circle.phases.size(),Vector2{0,0});
    std::uint64_t state=seed*0x9E3779B97F4A7C15ULL+0x632BE59BD9B4E019ULL;
    const auto unit=[&]{state^=state<<13;state^=state>>7;state^=state<<17;return static_cast<float>(state>>40)/static_cast<float>(1ULL<<24);};
    Vector2 center{0,0};float radius=circle.start_radius;
    for(std::size_t k=0;k<circle.phases.size();++k) {
        const float next=std::clamp(circle.phases[k].radius,0.0F,radius); // never grows
        // The first circle shrinks toward the island's heart (the first one covers sea too).
        const float room=k==0 ? std::min(radius-next,600.0F) : radius-next;
        Vector2 chosen=center;
        for(int attempt=0;attempt<24;++attempt) {
            const float angle=unit()*2*PI, along=std::sqrt(unit())*std::max(0.0F,room)*(attempt<16 ? 1.0F : .5F);
            const Vector2 candidate{center.x+std::cos(angle)*along,center.y+std::sin(angle)*along};
            if(!on_land || on_land(candidate.x,candidate.y)) {chosen=candidate;break;}
        }
        centers_[k]=chosen;center=chosen;radius=next;
    }
}
ZoneState ModeSession::zone() const {
    ZoneState z;
    Vector2 center{0,0};float radius=circle.start_radius;
    float t=elapsed_;
    const auto& phases=circle.phases;
    for(std::size_t k=0;k<phases.size();++k) {
        const auto& p=phases[k];
        const float closes_to=std::clamp(p.radius,0.0F,radius); // a circle never grows
        const Vector2 target=k<centers_.size() ? centers_[k] : center;
        z.phase=static_cast<int>(k);z.next_center=target;z.next_radius=closes_to;
        z.damage=p.damage*circle.damage_per_second;
        if(t<p.wait) {z.center=center;z.radius=radius;z.shrinking=false;z.remaining=p.wait-t;return z;}
        t-=p.wait;
        if(t<p.shrink) {
            const float f=t/std::max(.001F,p.shrink);
            z.center={center.x+(target.x-center.x)*f,center.y+(target.y-center.y)*f};
            z.radius=radius+(closes_to-radius)*f;z.shrinking=true;z.remaining=p.shrink-t;
            // Damage steps up as the wall starts to move.
            return z;
        }
        t-=p.shrink;center=target;radius=closes_to;
    }
    z.phase=static_cast<int>(phases.size());z.center=z.next_center=center;z.radius=z.next_radius=radius;
    z.shrinking=false;z.remaining=0;
    z.damage=(phases.empty() ? 1.0F : phases.back().damage)*circle.damage_per_second;
    return z;
}
float ModeSession::distance(Vector3 feet) const {
    const auto c=center();
    return std::hypot(feet.x-c.x,feet.z-c.y);
}
const char* ModeSession::objective() const {
    if(mode_==GameMode::BattleRoyale) return "Be the last survivor.";
    if(mode_==GameMode::Explore) return "Free roam: explore, drive, interact, visit towns, obey or break the law.";
    return "Survive, scavenge and explore Verda.";
}
void ModeSession::update(float dt, Vector3 feet, Health& health, ai::BattleRoyaleBots& bots, bool paused) {
    if(paused || !std::isfinite(dt) || dt<0) return;
    // A LAN host out of the match still closes the zone on the bots for the others still in it.
    const bool host_watching=terminal() && mode_==GameMode::BattleRoyale && run_bots_ && others_alive_>0;
    if(terminal() && !host_watching) return;
    if(mode_!=GameMode::BattleRoyale) {
        if(mode_!=GameMode::DevLab && !health.alive()) phase_=SessionPhase::Dead;
        return; // Explore has no victory condition.
    }
    const float step=std::min(dt,.25F);
    elapsed_+=step;
    if(host_watching) {
        const auto z=zone();
        bots.apply_circle(z.center,z.radius,z.next_center,z.next_radius,z.damage*step);
        return;
    }
    // Check every damage source before victory (including mutual final eliminations).
    if(health.alive()) {
        const auto z=zone();
        if(std::hypot(feet.x-z.center.x,feet.z-z.center.y)>z.radius) health.damage(z.damage*step);
        if(run_bots_) bots.apply_circle(z.center,z.radius,z.next_center,z.next_radius,z.damage*step);
    }
    if(!health.alive()) {
        phase_=SessionPhase::Dead; result_.placement=std::max(result_.participants>1 ? 2 : 1,bots.alive_bots()+others_alive_+1);
        bots.report_player_death(bots.events().player_damage_by);
    } else if(bots.alive_bots()==0 && others_alive_==0) { phase_=SessionPhase::Won; result_.placement=1; }
    result_.kills=bots.player_kills();
}
Vector3 session_spawn(const world::VerdaRegion& region, GameMode mode, std::uint64_t seed) {
    if(mode==GameMode::DevLab) return {0,world::terrain::TerrainHeight::sample(0,8),8};
    const auto& towns=region.settlements();
    Vector3 center{};
    if(!towns.empty()) {
        if(mode==GameMode::BattleRoyale) center=towns[seed%towns.size()].center;
        else {
            const auto capital=std::find_if(towns.begin(),towns.end(),[](const auto& town){return town.id=="capital_verda";});
            center=(capital==towns.end() ? towns.front() : *capital).center;
        }
    }
    // Public streets around the town centre, rather than roofs or inside authored buildings.
    for(int i=0;i<192;++i) {
        const float angle=(static_cast<float>(seed%6283)/1000.0F)+i*2.399963F;
        const float radius=12.0F+static_cast<float>(i%24)*6;
        Vector3 feet{center.x+std::sin(angle)*radius,0,center.z+std::cos(angle)*radius};
        const float ground=world::terrain::TerrainHeight::sample(feet.x,feet.z);
        if(!std::isfinite(ground) || ground<=world::layout::sea_level+1) continue;
        // Reject upper floors: spawning on a roof is not an accessible public location.
        feet.y=world::physics::WorldCollision::ground_height({feet.x,ground+.1F,feet.z},ground,region);
        if(std::abs(feet.y-ground)>2 || world::physics::WorldCollision::body_blocked(feet,feet.y,region,.4F)) continue;
        return feet;
    }
    // An open land fallback outside the capital, tested against collision as well.
    for(int i=0;i<256;++i) {
        Vector3 feet{250.0F+i*4,0,250}; feet.y=world::terrain::TerrainHeight::sample(feet.x,feet.z);
        if(feet.y>world::layout::sea_level+1 && !world::physics::WorldCollision::body_blocked(feet,feet.y,region,.4F)) return feet;
    }
    return {0,world::terrain::TerrainHeight::sample(0,0),0};
}
}
