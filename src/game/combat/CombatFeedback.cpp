#include "outland/game/combat/CombatFeedback.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game::combat {

void CombatFeedback::reset() { *this = CombatFeedback{}; }

void CombatFeedback::hit(Vector3 position, float amount, bool headshot) {
    if (!std::isfinite(amount) || amount <= 0) return;
    for (auto& number : numbers_) {
        const float dx = number.position.x - position.x, dz = number.position.z - position.z;
        if (number.life > number_life - .25F && dx * dx + dz * dz < 1.5F * 1.5F && !number.kill) {
            number.amount += amount;
            number.headshot |= headshot;
            number.life = number_life;
            return;
        }
    }
    numbers_.push_back({position, amount, headshot, false, number_life});
    if (numbers_.size() > 24) numbers_.erase(numbers_.begin());
}

void CombatFeedback::kill(Vector3 position) {
    streak_ = since_kill_ < streak_window ? streak_ + 1 : 1;
    since_kill_ = 0;
    kill_marker_ = kill_life;
    for (auto& number : numbers_) {
        const float dx = number.position.x - position.x, dz = number.position.z - position.z;
        if (number.life > number_life - .3F && dx * dx + dz * dz < 2.5F * 2.5F) { number.kill = true; number.life = number_life; return; }
    }
    numbers_.push_back({{position.x, position.y + 1.6F, position.z}, 0, false, true, number_life});
}

void CombatFeedback::hurt(float amount, Vector3 from, bool directional) {
    if (!std::isfinite(amount) || amount <= 0) return;
    flash_ = std::min(1.0F, flash_ + .25F + amount / 40);
    shake_ = std::min(.45F, shake_ + .05F + amount * .006F);
    if (directional) { indicator_ = 1.4F; source_ = from; }
}

void CombatFeedback::blast(float strength) {
    if (std::isfinite(strength) && strength > 0) shake_ = std::min(.8F, shake_ + strength);
}

void CombatFeedback::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    time_ += dt;
    since_kill_ += dt;
    if (since_kill_ > streak_window) streak_ = 0;
    kill_marker_ = std::max(0.0F, kill_marker_ - dt);
    flash_ = std::max(0.0F, flash_ - dt * 1.6F);
    indicator_ = std::max(0.0F, indicator_ - dt);
    shake_ *= std::exp(-dt * 7);
    if (shake_ < .002F) shake_ = 0;
    for (auto& number : numbers_) { number.life -= dt; number.position.y += dt * 1.1F; }
    std::erase_if(numbers_, [](const DamageNumber& n) { return n.life <= 0; });
}

Vector3 CombatFeedback::shake() const {
    if (shake_ <= 0) return {};
    const float t = static_cast<float>(time_);
    return {std::sin(t * 47.0F) * shake_ * .22F, std::sin(t * 61.0F + 1.3F) * shake_ * .16F, std::sin(t * 53.0F + 2.1F) * shake_ * .22F};
}

} // namespace outland::game::combat
