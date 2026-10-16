#include "outland/game/career/TrainingCamp.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::career {
namespace {
Vector3 forward_of(float yaw) { const float r = yaw * DEG2RAD; return {std::sin(r), 0, std::cos(r)}; }
Vector3 right_of(float yaw) { const float r = yaw * DEG2RAD; return {std::cos(r), 0, -std::sin(r)}; }
}

float TrainingCamp::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / 16777216.0F;
}

Vector3 TrainingCamp::local(float x, float z) const {
    return Vector3Add(line_, Vector3Add(Vector3Scale(right_of(yaw_), x), Vector3Scale(forward_of(yaw_), z)));
}

bool TrainingCamp::at_line(Vector3 feet) const {
    return placed_ && std::hypot(feet.x - line_.x, feet.z - line_.z) < line_radius && std::abs(feet.y - line_.y) < 4;
}

bool TrainingCamp::start(int drill) {
    if (!placed_ || drill < 0 || drill >= static_cast<int>(drills.size()) ||
        phase_ == CampPhase::Countdown || phase_ == CampPhase::Running)
        return false;
    drill_ = drill;
    hits_ = shown_ = shots_ = 0;
    target_ = {};
    phase_ = CampPhase::Countdown;
    clock_ = countdown;
    say(std::string(drills[static_cast<std::size_t>(drill)].name) + ": " + drills[static_cast<std::size_t>(drill)].brief, 6);
    return true;
}

void TrainingCamp::abort(const char* why) {
    if (phase_ != CampPhase::Countdown && phase_ != CampPhase::Running) return;
    phase_ = CampPhase::Idle;
    target_ = {};
    say(why);
}

int TrainingCamp::next_drill() const {
    for (std::size_t d = 0; d < drills.size(); ++d)
        if (!passed_[d]) return static_cast<int>(d);
    return 0;
}

void TrainingCamp::pop() {
    const auto& d = drills[static_cast<std::size_t>(drill_)];
    int slot = static_cast<int>(random() * slots);
    if (slot == slot_) slot = (slot + 1 + static_cast<int>(random() * (slots - 1))) % slots;
    slot_ = std::clamp(slot, 0, slots - 1);
    slide_ = d.moving ? (random() * 2 - 1) * slide_span * .5F : 0;
    slide_dir_ = random() < .5F ? -1.0F : 1.0F;
    target_ = {};
    target_.base = local(d.moving ? slide_ : slot_x(slot_), d.distance);
    target_.base.y = slot_ground_[static_cast<std::size_t>(drill_)][static_cast<std::size_t>(slot_)];
    target_.up = true;
    clock_ = d.window;
    ++shown_;
}

void TrainingCamp::finish() {
    const auto& d = drills[static_cast<std::size_t>(drill_)];
    const auto index = static_cast<std::size_t>(drill_);
    const bool was = graduated();
    best_[index] = std::max(best_[index], hits_);
    const int accuracy = shots_ > 0 ? static_cast<int>(std::lround(100.0F * static_cast<float>(hits_) / static_cast<float>(shots_))) : 0;
    phase_ = CampPhase::Result;
    target_ = {};
    if (hits_ >= d.pass) {
        passed_[index] = true;
        if (!was && graduated()) {
            graduated_now_ = true;
            say("ALL DRILLS QUALIFIED - you graduate. " + std::to_string(hits_) + "/" + std::to_string(d.count) + " hits, " +
                std::to_string(accuracy) + "% accuracy", 8);
        } else {
            say(std::string("QUALIFIED: ") + d.name + " - " + std::to_string(hits_) + "/" + std::to_string(d.count) + " hits, " +
                std::to_string(accuracy) + "% accuracy. USE for the next drill", 7);
        }
    } else {
        say("Not this time: " + std::to_string(hits_) + "/" + std::to_string(d.count) + " (need " + std::to_string(d.pass) +
            "), " + std::to_string(accuracy) + "% accuracy. USE to go again", 7);
    }
}

void TrainingCamp::update(float dt, Vector3 player, int shots) {
    if (!std::isfinite(dt) || dt < 0) return;
    message_time_ = std::max(0.0F, message_time_ - dt);
    if (phase_ == CampPhase::Countdown) {
        if (!at_line(player)) { abort("Back to the firing line - drill stopped"); return; }
        clock_ -= dt;
        if (clock_ <= 0) { phase_ = CampPhase::Running; pop(); }
        return;
    }
    if (phase_ != CampPhase::Running) return;
    if (!at_line(player)) { abort("You left the firing line - drill stopped"); return; }
    shots_ += std::max(0, shots);
    const auto& d = drills[static_cast<std::size_t>(drill_)];
    const float step = dt / rise_seconds;
    clock_ -= dt;
    if (target_.up) {
        target_.raise = std::min(1.0F, target_.raise + step);
        if (d.moving) {
            slide_ += slide_dir_ * slide_speed * dt;
            if (std::abs(slide_) > slide_span) { slide_ = std::copysign(slide_span, slide_); slide_dir_ = -slide_dir_; }
            const float y = target_.base.y;
            target_.base = local(slide_, d.distance);
            target_.base.y = y;
        }
        if (target_.hit || clock_ <= 0) { target_.up = false; clock_ = gap; }
        return;
    }
    target_.raise = std::max(0.0F, target_.raise - step);
    if (clock_ > 0) return;
    if (shown_ < d.count) pop();
    else finish();
}

combat::BulletHit TrainingCamp::trace(Vector3 a, Vector3 b) const {
    combat::BulletHit none;
    if (phase_ != CampPhase::Running || !target_.up || target_.hit || target_.raise < .6F) return none;
    const Vector3 right = right_of(yaw_), forward = forward_of(yaw_);
    const auto to_local = [&](Vector3 p) {
        const Vector3 r = Vector3Subtract(p, target_.base);
        return Vector3{Vector3DotProduct(r, right), r.y, Vector3DotProduct(r, forward)};
    };
    const Vector3 la = to_local(a), lb = to_local(b), delta = Vector3Subtract(lb, la);
    const Vector3 low{-target_width * .5F, .15F, -.06F}, high{target_width * .5F, target_height, .06F};
    float t0 = 0, t1 = 1;
    for (int axis = 0; axis < 3; ++axis) {
        const float s = axis == 0 ? la.x : axis == 1 ? la.y : la.z;
        const float d = axis == 0 ? delta.x : axis == 1 ? delta.y : delta.z;
        const float lo = axis == 0 ? low.x : axis == 1 ? low.y : low.z;
        const float hi = axis == 0 ? high.x : axis == 1 ? high.y : high.z;
        if (std::abs(d) < 1e-7F) { if (s < lo || s > hi) return none; continue; }
        float enter = (lo - s) / d, leave = (hi - s) / d;
        if (enter > leave) std::swap(enter, leave);
        t0 = std::max(t0, enter); t1 = std::min(t1, leave);
        if (t0 > t1) return none;
    }
    combat::BulletHit hit;
    hit.kind = combat::HitKind::Target;
    hit.fraction = t0;
    hit.position = Vector3Lerp(a, b, t0);
    hit.normal = Vector3Scale(forward, Vector3DotProduct(delta, Vector3{0, 0, 1}) > 0 ? -1.0F : 1.0F);
    hit.target = 0;
    hit.headshot = (la.y + delta.y * t0) >= head_from;
    return hit;
}

void TrainingCamp::hit(int, bool headshot) {
    if (phase_ != CampPhase::Running || !target_.up || target_.hit) return;
    target_.hit = true;
    target_.headshot = headshot;
    ++hits_;
}

} // namespace outland::game::career
