#include "outland/game/ModeSession.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaLayout.hpp"
#include <algorithm>
#include <cmath>
namespace outland::game {
void ModeSession::start(GameMode mode, int bots, ai::BotLevel level) {
    mode_=mode; phase_=SessionPhase::Playing; elapsed_=0;
    result_={0,0,std::max(0,bots)+1,level};
}
float ModeSession::radius() const {
    const float progress=std::clamp((elapsed_-circle.opening_seconds)/std::max(1.0F,circle.shrink_seconds),0.0F,1.0F);
    return circle.start_radius+(circle.end_radius-circle.start_radius)*progress;
}
float ModeSession::distance(Vector3 feet) const { return std::hypot(feet.x,feet.z); }
const char* ModeSession::objective() const {
    if(mode_==GameMode::BattleRoyale) return "Be the last survivor.";
    if(mode_==GameMode::Explore) return "Free roam: explore, drive, interact, visit towns, obey or break the law.";
    return "Survive, scavenge and explore Verda.";
}
void ModeSession::update(float dt, Vector3 feet, Health& health, ai::BattleRoyaleBots& bots, bool paused) {
    if(terminal() || paused || !std::isfinite(dt) || dt<0) return;
    if(mode_!=GameMode::BattleRoyale) {
        if(mode_!=GameMode::DevLab && !health.alive()) phase_=SessionPhase::Dead;
        return; // Explore has no victory condition.
    }
    const float step=std::min(dt,.25F);
    elapsed_+=step;
    // Check every damage source before victory (including mutual final eliminations).
    if(health.alive()) {
        if(distance(feet)>radius()) health.damage(circle.damage_per_second*step);
        bots.apply_circle(radius(),circle.damage_per_second*step);
    }
    if(!health.alive()) {
        phase_=SessionPhase::Dead; result_.placement=std::max(result_.participants>1 ? 2 : 1,bots.alive_bots()+1);
        bots.report_player_death(bots.events().player_damage_by);
    } else if(bots.alive_bots()==0) { phase_=SessionPhase::Won; result_.placement=1; }
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
