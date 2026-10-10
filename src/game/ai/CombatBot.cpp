#include "outland/game/ai/CombatBot.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace outland::game::ai {

const char* bot_level_name(BotLevel level) {
    switch(level) {
        case BotLevel::Easy:return "EASY";
        case BotLevel::Hard:return "HARD";
        default:return "MEDIUM";
    }
}

// Perception (view distance, field of view, hearing) is identical at every level: a harder bot
// notices and reacts sooner and shoots straighter, but it never knows more than an easy one.
BotDifficulty bot_difficulty(BotLevel level) {
    BotDifficulty d;
    switch(level) {
        case BotLevel::Easy:
            d.reaction=.95F;d.aim_error_deg=6.0F;d.aim_floor_deg=1.5F;d.settle_seconds=1.7F;
            d.decision_interval=.65F;d.decision_quality=.62F;d.aggression=.3F;d.lead_targets=false;
            break;
        case BotLevel::Hard:
            d.reaction=.28F;d.aim_error_deg=2.2F;d.aim_floor_deg=.45F;d.settle_seconds=.7F;
            d.decision_interval=.22F;d.decision_quality=.95F;d.aggression=.7F;d.lead_targets=true;
            break;
        default:break; // Medium uses the struct defaults.
    }
    return d;
}

const char* bot_intent_name(BotIntent intent) {
    switch(intent) {
        case BotIntent::Investigate:return "INVESTIGATE";
        case BotIntent::Engage:return "ENGAGE";
        case BotIntent::TakeCover:return "COVER";
        case BotIntent::Reload:return "RELOAD";
        case BotIntent::Heal:return "HEAL";
        case BotIntent::Push:return "PUSH";
        case BotIntent::Flank:return "FLANK";
        case BotIntent::Retreat:return "RETREAT";
        case BotIntent::Dead:return "DEAD";
        default:return "ROAM";
    }
}

namespace {
constexpr float memory_seconds=20.0F;
constexpr float sight_refresh=.15F;   // seconds between line-of-sight checks per contact
constexpr float turn_rate=420.0F;     // degrees per second, same for every level
constexpr float chest_height=1.2F;

Vector3 flat(Vector3 v) { return {v.x,0,v.z}; }
Vector3 eye(Vector3 feet) { return {feet.x,feet.y+CombatBot::eye_height,feet.z}; }
float heading_of(Vector3 direction) { return std::atan2(direction.x,direction.z)*RAD2DEG; }
Vector3 forward_of(float yaw_degrees) {
    const float r=yaw_degrees*DEG2RAD;
    return {std::sin(r),0,std::cos(r)};
}
float wrap_degrees(float a) {
    while(a>180) a-=360;
    while(a<-180) a+=360;
    return a;
}
}

CombatBot::CombatBot(int id,BotLevel level,combat::WeaponId weapon,std::uint64_t seed)
    : id_(id),level_(level),difficulty_(bot_difficulty(level)),weapon_(weapon),rng_(seed|1) {
    const auto& w=combat::definition(weapon);
    loaded_=w.magazine;reserve_=w.reserve;
    yaw_=random()*360-180;
}

float CombatBot::random() {
    rng_^=rng_<<13;rng_^=rng_>>7;rng_^=rng_<<17;
    return static_cast<float>(rng_>>40)/static_cast<float>(1ULL<<24);
}

const BotContact* CombatBot::target() const {
    for(const auto& contact:contacts_) if(contact.agent==target_) return &contact;
    return nullptr;
}

float CombatBot::effective_range() const {
    return weapon_==combat::WeaponId::Rifle ? 140.0F : 45.0F;
}
float CombatBot::preferred_range() const {
    const float caution=1-difficulty_.aggression;
    return weapon_==combat::WeaponId::Rifle ? 30+caution*28 : 12+caution*10;
}

void CombatBot::on_damage(float amount,Vector3 from,double now) {
    if(!alive() || !std::isfinite(amount) || amount<=0) return;
    last_damage_at_=now;damage_from_=from;
    heal_left_=0; // being shot interrupts bandaging
    decision_clock_=difficulty_.decision_interval; // re-think on the next tick
}

void CombatBot::tick(float dt,double now,const std::vector<BotAgent>& agents,const std::vector<BotSound>& sounds,
                     const BotEnvironment& environment,std::vector<BotShot>& shots) {
    if(!std::isfinite(dt) || dt<=0) return;
    dt=std::min(dt,.25F);
    if(!alive()) {
        intent_=BotIntent::Dead;moving_=false;velocity_={};contacts_.clear();target_=-1;
        return;
    }
    perceive(dt,now,agents,sounds,environment);
    decision_clock_+=dt;
    if(decision_clock_>=difficulty_.decision_interval) {
        decision_clock_=0;
        decide(now,environment);
    }
    act(dt,now,environment,shots);
}

void CombatBot::perceive(float dt,double now,const std::vector<BotAgent>& agents,const std::vector<BotSound>& sounds,
                         const BotEnvironment& environment) {
    const auto self_eye=eye(position_);
    const auto forward=forward_of(yaw_);
    const float half_fov=difficulty_.fov_deg*.5F*DEG2RAD;
    for(const auto& agent:agents) {
        if(agent.id==id_) continue;
        auto found=std::find_if(contacts_.begin(),contacts_.end(),[&](const BotContact& c){return c.agent==agent.id;});
        if(!agent.alive) {
            if(found!=contacts_.end()) contacts_.erase(found);
            continue;
        }
        const auto to=Vector3Subtract(eye(agent.position),self_eye);
        const float distance=Vector3Length(to);
        bool candidate=distance<=difficulty_.view_distance;
        if(candidate && distance>3.0F) { // within 3 m a body is felt/heard regardless of facing
            const auto horizontal=Vector3Normalize(flat(to));
            candidate=std::acos(std::clamp(Vector3DotProduct(horizontal,forward),-1.0F,1.0F))<=half_fov;
        }
        if(!candidate) {
            if(found!=contacts_.end()) found->visible=false;
            continue;
        }
        if(found==contacts_.end()) {
            BotContact contact;contact.agent=agent.id;
            contacts_.push_back(contact);
            found=contacts_.end()-1;
        }
        // Line of sight is the only way to learn a position by eye; checks are throttled.
        bool visible=found->visible;
        if(found->checked_at+sight_refresh<=now) {
            found->checked_at=now;
            visible=!environment.line_of_sight || environment.line_of_sight(self_eye,eye(agent.position));
        }
        if(visible) {
            found->visible=true;
            found->last_seen=agent.position;found->velocity=agent.velocity;found->seen_at=now;
            found->awareness=std::min(1.0F,found->awareness+dt/std::max(.05F,difficulty_.reaction));
        } else {
            found->visible=false;found->tracking=0;
        }
    }
    for(auto& contact:contacts_) {
        if(contact.visible) continue;
        contact.tracking=0;
        // Losing sight lets the reaction lapse; reacquiring a target is quicker than first sight.
        if(contact.awareness>.5F) contact.awareness=std::max(.5F,contact.awareness-dt*.25F);
    }
    for(const auto& sound:sounds) {
        if(sound.source==id_ || sound.radius<=0) continue;
        const float distance=Vector3Distance(sound.position,position_);
        if(distance>sound.radius*difficulty_.hearing_scale) continue;
        // Hearing gives a rough bearing and range, never an exact position.
        const float error=distance*.12F;
        const Vector3 guess{sound.position.x+(random()*2-1)*error,sound.position.y,sound.position.z+(random()*2-1)*error};
        heard_=guess;sound_pending_=true;
        if(sound.source>=0) {
            auto found=std::find_if(contacts_.begin(),contacts_.end(),[&](const BotContact& c){return c.agent==sound.source;});
            if(found==contacts_.end()) {BotContact contact;contact.agent=sound.source;contacts_.push_back(contact);found=contacts_.end()-1;}
            if(!found->visible) {found->last_seen=guess;found->velocity={};found->seen_at=now;}
        }
    }
    std::erase_if(contacts_,[&](const BotContact& c){return !c.visible && now-c.seen_at>memory_seconds;});
    // Target: the closest enemy we are reacting to, otherwise the freshest memory.
    int best=-1;float best_score=-1e9F;
    for(const auto& contact:contacts_) {
        const float distance=Vector3Distance(contact.last_seen,position_);
        float score=-static_cast<float>(now-contact.seen_at)*4-distance*.05F;
        if(contact.visible) score+=40+contact.awareness*20;
        if(contact.agent==target_) score+=6; // stick with a target rather than flicking between two
        if(score>best_score) {best_score=score;best=contact.agent;}
    }
    if(best!=target_) {
        for(auto& contact:contacts_) if(contact.agent==best) contact.tracking=0;
    }
    target_=best;
}

bool CombatBot::find_cover(Vector3 threat,const BotEnvironment& environment,Vector3& out) {
    if(!environment.line_of_sight || !environment.move) return false;
    const auto threat_eye=eye(threat);
    const float threat_distance=Vector3Distance(flat(threat),flat(position_));
    const float offset=random()*360;
    float best=1e9F;bool found=false;
    for(const float radius:{3.5F,7.0F,12.0F}) {
        for(int i=0;i<10;++i) {
            const float angle=(offset+i*36.0F)*DEG2RAD;
            const Vector3 wanted{position_.x+std::sin(angle)*radius,position_.y,position_.z+std::cos(angle)*radius};
            // Don't close distance to take cover.
            if(Vector3Distance(flat(threat),flat(wanted))<threat_distance-2) continue;
            const auto reached=environment.move(position_,wanted);
            if(Vector3Distance(flat(reached),flat(wanted))>1.2F) continue;
            const Vector3 crouched{reached.x,reached.y+chest_height,reached.z};
            if(environment.line_of_sight(threat_eye,crouched)) continue;
            const float cost=radius+random()*.5F;
            if(cost<best) {best=cost;out=reached;found=true;}
        }
        if(found) return true; // nearest ring that has cover wins
    }
    return false;
}

bool CombatBot::find_flank(Vector3 threat,const BotEnvironment& environment,Vector3& out) {
    if(!environment.line_of_sight || !environment.move) return false;
    auto away=flat(Vector3Subtract(position_,threat));
    const float distance=Vector3Length(away);
    if(distance<4) return false;
    away=Vector3Scale(away,1/distance);
    const Vector3 side{-away.z,0,away.x};
    const float range=std::clamp(distance,10.0F,preferred_range());
    float best=1e9F;bool found=false;
    for(const float sign:{-1.0F,1.0F}) for(const float swing:{45.0F,75.0F}) {
        const float a=swing*DEG2RAD;
        const auto direction=Vector3Add(Vector3Scale(away,std::cos(a)),Vector3Scale(side,sign*std::sin(a)));
        const Vector3 wanted=Vector3Add(threat,Vector3Scale(direction,range));
        const auto reached=environment.move(position_,wanted);
        if(Vector3Distance(flat(reached),flat(wanted))>2.0F) continue;
        if(!environment.line_of_sight(eye(reached),eye(threat))) continue;
        const float cost=Vector3Distance(position_,reached)+random()*3;
        if(cost<best) {best=cost;out=reached;found=true;}
    }
    return found;
}

void CombatBot::decide(double now,const BotEnvironment& environment) {
    const auto* contact=target();
    const float h=health_/100.0F;
    const auto& w=combat::definition(weapon_);
    const bool aware=contact && contact->visible && contact->awareness>=1;
    const bool under_fire=now-last_damage_at_<2.0;
    const float since_seen=contact ? static_cast<float>(now-contact->seen_at) : 1e9F;
    const float aggression=difficulty_.aggression;
    const Vector3 threat=contact ? contact->last_seen : damage_from_;
    const bool has_threat=contact || under_fire;
    const float distance=contact ? Vector3Distance(position_,contact->last_seen) : 1e9F;

    struct Option { BotIntent intent; float score; };
    std::array<Option,9> options{};
    std::size_t count=0;
    const auto add=[&](BotIntent intent,float score) {
        if(intent==intent_) score+=.12F; // commitment: avoid dithering between near-equal options
        options[count++]={intent,score};
    };

    if(aware && loaded_+reserve_>0)
        add(BotIntent::Engage,.75F+aggression*.3F+(distance<=effective_range() ? .15F : -.35F)-(h<.4F ? .3F : 0)-(loaded_==0 ? .6F : 0));
    Vector3 cover{},flank{};
    bool cover_found=false,flank_found=false;
    if(intent_==BotIntent::TakeCover && has_threat) {
        // Holding (or still running to) cover: the urge fades once the shooting stops.
        add(BotIntent::TakeCover,.25F+(under_fire ? .45F : 0)+(1-h)*.7F-aggression*.35F+(loaded_==0 ? .35F : 0)-
            std::min(.5F,static_cast<float>(now-intent_since_)*.06F));
    } else if(has_threat && (under_fire || h<.6F || loaded_==0)) {
        const float score=.25F+(under_fire ? .45F : 0)+(1-h)*.7F-aggression*.35F+(loaded_==0 ? .35F : 0);
        if(score>.35F && (cover_found=find_cover(threat,environment,cover))) add(BotIntent::TakeCover,score);
    }
    if(reserve_>0 && loaded_<w.magazine) {
        if(loaded_==0) add(BotIntent::Reload,aware ? .95F : 1.2F);
        else if(!aware && loaded_<w.magazine*.45F) add(BotIntent::Reload,.7F);
    }
    if(h<.55F && medkits_>0 && !aware && now-last_damage_at_>3.0) add(BotIntent::Heal,.8F+(1-h));
    if(contact && !contact->visible && since_seen<8 && h>.5F && loaded_>0)
        add(BotIntent::Push,.38F+aggression*.5F+(now-last_hit_given_at_<5 ? .2F : 0));
    if(contact && !contact->visible && since_seen<10 && h>.4F && loaded_>0) {
        if(intent_==BotIntent::Flank && has_destination_) add(BotIntent::Flank,.42F+aggression*.15F);
        else if((flank_found=find_flank(contact->last_seen,environment,flank))) add(BotIntent::Flank,.42F+aggression*.15F);
    }
    if(has_threat && h<.35F && medkits_==0 && (aware || under_fire) && (!contact || distance<preferred_range()*1.5F))
        add(BotIntent::Retreat,.65F+(1-h)-aggression*.4F);
    if(sound_pending_ || (contact && !contact->visible && since_seen<memory_seconds))
        add(BotIntent::Investigate,.3F);
    add(BotIntent::Roam,.1F);

    std::sort(options.begin(),options.begin()+static_cast<std::ptrdiff_t>(count),[](const Option& a,const Option& b){return a.score>b.score;});
    // Decision quality: sometimes a bot goes with its second-best idea.
    std::size_t choice=0;
    if(count>1 && options[1].score>.2F && random()>difficulty_.decision_quality) choice=1;
    const auto next=options[choice].intent;
    if(next!=intent_) {
        intent_=next;intent_since_=now;stuck_time_=0;
        has_destination_=false;
        switch(next) {
            case BotIntent::TakeCover:destination_=cover;has_destination_=cover_found;break;
            case BotIntent::Flank:destination_=flank;has_destination_=flank_found;break;
            case BotIntent::Push:if(contact) {destination_=contact->last_seen;has_destination_=true;} break;
            case BotIntent::Investigate:
                destination_=sound_pending_ ? heard_ : (contact ? contact->last_seen : position_);has_destination_=true;
                break;
            case BotIntent::Retreat: {
                auto away=flat(Vector3Subtract(position_,threat));
                if(Vector3LengthSqr(away)<.01F) away=forward_of(yaw_+180);
                destination_=Vector3Add(position_,Vector3Scale(Vector3Normalize(away),25));has_destination_=true;
                break;
            }
            default:break;
        }
    } else if(next==BotIntent::Push && contact) {
        destination_=contact->last_seen;has_destination_=true;
    }
    decided_at_=now;
}

void CombatBot::act(float dt,double now,const BotEnvironment& environment,std::vector<BotShot>& shots) {
    const auto& w=combat::definition(weapon_);
    // Timers run whatever the bot is doing.
    cooldown_=std::max(0.0F,cooldown_-dt);
    if(reload_left_>0) {
        reload_left_-=dt;
        if(reload_left_<=0) {
            reload_left_=0;
            const int moved=std::min(w.magazine-loaded_,reserve_);
            loaded_+=moved;reserve_-=moved;
        }
    }
    if(intent_==BotIntent::Reload && reload_left_<=0 && loaded_<w.magazine && reserve_>0) reload_left_=w.reload_seconds;
    if(loaded_==0 && reserve_>0 && reload_left_<=0) reload_left_=w.reload_seconds;

    const auto found=std::find_if(contacts_.begin(),contacts_.end(),[&](const BotContact& c){return c.agent==target_;});
    BotContact* contact=found==contacts_.end() ? nullptr : &*found;
    const bool aware=contact && contact->visible && contact->awareness>=1;

    // ---- movement ----
    Vector3 move_to=position_;bool run=false,want_move=false;
    switch(intent_) {
        case BotIntent::Engage: {
            if(!contact) break;
            const auto offset=flat(Vector3Subtract(contact->last_seen,position_));
            const float distance=Vector3Length(offset);
            const auto toward=distance>.01F ? Vector3Scale(offset,1/distance) : forward_of(yaw_);
            const Vector3 side{-toward.z,0,toward.x};
            // Strafe so the bot is not a stationary target; switch sides every couple of seconds.
            const float phase=std::fmod(static_cast<float>(now)+static_cast<float>(id_)*.73F,2.6F);
            const float strafe=phase<1.3F ? 1.0F : -1.0F;
            Vector3 direction=Vector3Scale(side,strafe*.8F);
            if(distance>preferred_range()*1.3F || distance>effective_range()*.9F) direction=Vector3Add(direction,Vector3Scale(toward,1.2F));
            else if(distance<preferred_range()*.5F) direction=Vector3Add(direction,Vector3Scale(toward,-1.0F));
            move_to=Vector3Add(position_,Vector3Normalize(direction));
            want_move=true;
            break;
        }
        case BotIntent::TakeCover:case BotIntent::Flank:case BotIntent::Push:case BotIntent::Retreat:
            if(has_destination_) {move_to=destination_;want_move=true;run=true;}
            break;
        case BotIntent::Investigate:
            if(has_destination_) {move_to=destination_;want_move=true;}
            break;
        case BotIntent::Roam:
            if(!has_destination_) {
                const float angle=random()*2*PI, radius=std::sqrt(random())*roam_radius_;
                destination_={roam_center_.x+std::sin(angle)*radius,position_.y,roam_center_.z+std::cos(angle)*radius};
                has_destination_=true;
            }
            move_to=destination_;want_move=true;
            break;
        case BotIntent::Heal:
            if(heal_left_<=0 && medkits_>0) heal_left_=heal_seconds;
            break;
        default:break;
    }
    if(intent_!=BotIntent::Heal) heal_left_=0;
    if(heal_left_>0) {
        heal_left_-=dt;want_move=false;
        if(heal_left_<=0) {heal_left_=0;--medkits_;health_=std::min(100.0F,health_+heal_amount);}
    }

    previous_=position_;
    moving_=false;
    if(want_move && environment.move) {
        auto delta=flat(Vector3Subtract(move_to,position_));
        const float remaining=Vector3Length(delta);
        const bool arrived=remaining<.6F && intent_!=BotIntent::Engage;
        if(arrived) {
            has_destination_=false;
            if(intent_==BotIntent::Investigate) sound_pending_=false;
        } else if(remaining>.001F) {
            const float speed=(run ? run_speed : walk_speed)*(reload_left_>0 ? .75F : 1.0F);
            const float step=std::min(remaining,speed*dt);
            const auto wanted=Vector3Add(position_,Vector3Scale(delta,step/remaining));
            position_=environment.move(position_,wanted);
            const float moved=Vector3Length(flat(Vector3Subtract(position_,previous_)));
            moving_=moved>step*.2F;
            stuck_time_=moved<step*.3F ? stuck_time_+dt : std::max(0.0F,stuck_time_-dt);
            if(stuck_time_>1.0F) {
                // Blocked: try somewhere else rather than walking into the wall forever.
                stuck_time_=0;has_destination_=false;
                if(intent_==BotIntent::Investigate) sound_pending_=false;
                if(intent_!=BotIntent::Engage) intent_=BotIntent::Roam;
                const float angle=random()*2*PI;
                destination_={position_.x+std::sin(angle)*8,position_.y,position_.z+std::cos(angle)*8};
                has_destination_=true;
            }
        }
    }
    velocity_=Vector3Scale(Vector3Subtract(position_,previous_),1/dt);

    // ---- facing ----
    float wanted_yaw=yaw_;
    if(contact && (contact->visible || now-contact->seen_at<3)) wanted_yaw=heading_of(flat(Vector3Subtract(contact->last_seen,position_)));
    else if(now-last_damage_at_<1.5) wanted_yaw=heading_of(flat(Vector3Subtract(damage_from_,position_)));
    else if(moving_) wanted_yaw=heading_of(flat(Vector3Subtract(position_,previous_)));
    const float turn=std::clamp(wrap_degrees(wanted_yaw-yaw_),-turn_rate*dt,turn_rate*dt);
    yaw_=wrap_degrees(yaw_+turn);

    // ---- shooting ----
    if(!aware || intent_==BotIntent::Heal) {
        if(contact) contact->tracking=0;
        burst_=0;
        return;
    }
    contact->tracking+=dt;
    const float distance=Vector3Distance(contact->last_seen,position_);
    const float facing_error=std::abs(wrap_degrees(heading_of(flat(Vector3Subtract(contact->last_seen,position_)))-yaw_));
    const bool can_fire=intent_==BotIntent::Engage || intent_==BotIntent::Push || intent_==BotIntent::Flank ||
        intent_==BotIntent::Retreat || intent_==BotIntent::Investigate || intent_==BotIntent::Roam;
    if(!can_fire || facing_error>12 || distance>effective_range() || loaded_<=0 || reload_left_>0 || cooldown_>0) return;

    // Aim: start wide, settle toward the floor while tracking; moving (self or target) costs accuracy.
    const float settle=std::clamp(contact->tracking/std::max(.05F,difficulty_.settle_seconds),0.0F,1.0F);
    float error=difficulty_.aim_floor_deg+(difficulty_.aim_error_deg-difficulty_.aim_floor_deg)*(1-settle);
    if(moving_) error=error*1.4F+.4F;
    const float target_speed=Vector3Length(flat(contact->velocity));
    error+=target_speed*.25F;
    if(difficulty_.lead_targets) error-=target_speed*.15F;
    // Without leading, aim trails a moving target by the bot's perception lag.
    Vector3 aim_point=contact->last_seen;
    if(!difficulty_.lead_targets) aim_point=Vector3Subtract(aim_point,Vector3Scale(contact->velocity,.14F));
    aim_point.y+=chest_height;
    const auto origin=eye(position_);
    auto direction=Vector3Normalize(Vector3Subtract(aim_point,origin));
    // Random deflection inside the error cone.
    const float cone=std::max(.05F,error)*DEG2RAD*std::sqrt(random());
    const float spin=random()*2*PI;
    Vector3 up{0,1,0};
    auto right=Vector3CrossProduct(direction,up);
    if(Vector3LengthSqr(right)<.0001F) right={1,0,0};
    right=Vector3Normalize(right);
    up=Vector3CrossProduct(right,direction);
    const auto deflect=Vector3Add(Vector3Scale(right,std::cos(spin)),Vector3Scale(up,std::sin(spin)));
    direction=Vector3Normalize(Vector3Add(Vector3Scale(direction,std::cos(cone)),Vector3Scale(deflect,std::sin(cone))));

    shots.push_back({id_,origin,direction,w.damage,weapon_});
    --loaded_;
    if(weapon_==combat::WeaponId::Rifle) {
        // Short controlled bursts; the pause lets aim settle again.
        ++burst_;
        if(burst_>=3+static_cast<int>(random()*3)) {burst_=0;cooldown_=.3F+random()*.25F;contact->tracking*=.6F;}
        else cooldown_=w.interval;
    } else cooldown_=w.interval+.08F+random()*.18F;
}
}
