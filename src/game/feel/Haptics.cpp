#include "outland/game/feel/Haptics.hpp"
#include "outland/platform/Haptics.hpp"
#include <algorithm>
#include <utility>

namespace outland::game::feel {

Haptics::Haptics(Sink sink) : sink_(sink ? std::move(sink) : Sink{[](int ms, float s) { platform::vibrate(ms, s); }}) {}

void Haptics::request(int milliseconds, float strength) {
    if (!enabled_ || milliseconds <= 0 || strength <= 0) return;
    strength = std::clamp(strength, 0.0F, 1.0F);
    if (static_cast<float>(milliseconds) * strength > static_cast<float>(pending_ms_) * pending_strength_) {
        pending_ms_ = milliseconds;
        pending_strength_ = strength;
    }
}

void Haptics::shot(combat::WeaponId weapon) {
    switch (weapon) {
    case combat::WeaponId::Pistol: request(22, .55F); break;
    case combat::WeaponId::Rifle: request(18, .5F); break;
    case combat::WeaponId::Smg: request(14, .4F); break;
    case combat::WeaponId::Shotgun: request(45, .9F); break;
    case combat::WeaponId::Sniper: request(55, 1.0F); break;
    case combat::WeaponId::DoubleBarrel: request(60, 1.0F); break;
    case combat::WeaponId::AutoShotgun: request(30, .75F); break;
    case combat::WeaponId::Rocket: request(90, 1.0F); break;
    case combat::WeaponId::MachinePistol: request(12, .35F); break;
    }
}

void Haptics::hit_taken(float damage) {
    if (damage < 1) return;
    request(std::min(160, 45 + static_cast<int>(damage * 2.5F)), .6F + damage / 60);
}

void Haptics::hit_given(bool kill) {
    if (kill) request(70, .8F);
}

void Haptics::crash(float impact) {
    if (impact < 3) return;
    request(std::min(260, 30 + static_cast<int>(impact * 9)), impact / 22);
}

void Haptics::explosion(float distance) {
    if (distance >= 40) return;
    const float near = 1 - distance / 40;
    request(40 + static_cast<int>(240 * near), .3F + .7F * near);
}

void Haptics::land(float speed) {
    if (speed < 9) return;
    request(std::min(90, 20 + static_cast<int>((speed - 9) * 7)), .35F + (speed - 9) / 20);
}

void Haptics::update(float dt) {
    busy_ = std::max(0.0F, busy_ - dt);
    if (busy_ <= 0) busy_weight_ = 0;
    if (pending_ms_ > 0) {
        const float weight = static_cast<float>(pending_ms_) * pending_strength_;
        // A weaker buzz waits for (or is lost behind) a stronger one still running.
        if (weight >= busy_weight_ * .8F) {
            sink_(pending_ms_, pending_strength_);
            busy_ = static_cast<float>(pending_ms_) / 1000;
            busy_weight_ = weight;
        }
    }
    pending_ms_ = 0;
    pending_strength_ = 0;
}

} // namespace outland::game::feel
