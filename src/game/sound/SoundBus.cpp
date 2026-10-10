#include "outland/game/sound/SoundBus.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::sound {

const char* sound_kind_name(SoundKind kind) {
    switch(kind) {
        case SoundKind::Gunshot:return "gunshot";
        case SoundKind::Footstep:return "footstep";
        case SoundKind::Engine:return "engine";
        case SoundKind::Horn:return "horn";
        case SoundKind::Shout:return "shout";
        default:return "impact";
    }
}

std::uint64_t SoundBus::emit(SoundKind kind,Vector3 position,float radius,int source,double time) {
    if(!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
       !std::isfinite(radius) || radius<=0 || !std::isfinite(time)) return 0;
    events_.push_back({kind,position,radius,source,time,++serial_});
    // A runaway emitter must not grow the list without bound between advances.
    if(events_.size()>4096) events_.erase(events_.begin(),events_.begin()+1024);
    return serial_;
}

void SoundBus::advance(double now) {
    if(!std::isfinite(now)) return;
    std::erase_if(events_,[&](const SoundEvent& e){return now-e.time>lifetime || e.time>now+lifetime;});
}

void SoundBus::clear() { events_.clear(); }

bool SoundBus::audible(const SoundEvent& event,Vector3 listener,float hearing) {
    if(!std::isfinite(hearing) || hearing<=0) return false;
    const float reach=event.radius*hearing;
    return Vector3DistanceSqr(event.position,listener)<=reach*reach;
}

float SoundBus::loudness(const SoundEvent& event,Vector3 listener,float hearing) {
    if(!std::isfinite(hearing) || hearing<=0) return 0;
    const float reach=event.radius*hearing;
    return std::clamp(1-Vector3Distance(event.position,listener)/reach,0.0F,1.0F);
}
}
