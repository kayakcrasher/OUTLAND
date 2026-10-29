#include "outland/game/racing/StreetRace.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <algorithm>
#include <cmath>
#include <raymath.h>

namespace outland::game::racing {

namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
constexpr float quarter = .25F;           // of the ring
constexpr float checkpoint_spacing = 420;  // metres between flares
constexpr float run_out = 150;             // road past the finish
} // namespace

bool StreetRace::setup(const std::vector<Vector3>& ring, Vector3 meet) {
    track_ = {};
    checkpoints_.clear();
    meet_ = meet;
    if (ring.size() < 8) return false;
    float circumference = 0;
    for (std::size_t i = 0; i < ring.size(); ++i) circumference += flat(ring[i], ring[(i + 1) % ring.size()]);
    std::size_t first = 0;
    for (std::size_t i = 1; i < ring.size(); ++i)
        if (flat(ring[i], meet) < flat(ring[first], meet)) first = i;
    // A quarter of the way round, in the direction the points run (and on past the finish, so the
    // field crosses the line flat out instead of braking for the end of the road).
    float run = 0;
    for (std::size_t k = 0; k <= ring.size() && run < circumference * quarter + run_out; ++k) {
        const Vector3 p = ring[(first + k) % ring.size()];
        if (!track_.points.empty()) run += flat(track_.points.back(), p);
        track_.points.push_back(p);
    }
    if (track_.points.size() < 3) return false;
    track_.lane.assign(track_.points.size() - 1, lane);
    track_.length = run;
    track_.complete = true;
    finish_at_ = std::max(100.0F, run - run_out);
    for (float d = checkpoint_spacing; d < finish_at_ - 60; d += checkpoint_spacing) checkpoints_.push_back(d);
    checkpoints_.push_back(finish_at_);   // the finish
    // Fulmo stands at the meet, a few metres toward the road.
    organiser_ = Vector3Add(meet, Vector3Scale(Vector3Normalize(Vector3Subtract(start(), meet)), 6));
    stage_ = Stage::Closed;
    return true;
}

Vector3 StreetRace::start() const { return track_.complete ? world::roads::route_point(track_, 0) : meet_; }
float StreetRace::start_yaw() const {
    float yaw = 0;
    if (track_.complete) world::roads::route_point(track_, 0, &yaw);
    return yaw;
}
Vector3 StreetRace::finish() const { return track_.complete ? world::roads::route_point(track_, finish_at_) : meet_; }

Vector3 StreetRace::grid(int i, float* yaw) const {
    // The racers in pairs behind the line, in the two lanes; the player's slot is the first.
    const int row = (i + 1) / 2, column = (i + 1) % 2;
    world::roads::Route lined = track_;
    lined.lane.assign(lined.lane.size(), column ? world::highways::fast_lane : world::highways::slow_lane);
    return world::roads::route_point(lined, std::max(0.0F, 10.0F - 9.0F * static_cast<float>(row)), yaw);
}

std::string StreetRace::talk(std::int64_t money, bool& paid) {
    paid = false;
    if (stage_ == Stage::Closed) return "Fulmo: \"Nothing to see. Come back after nine, when the road's empty.\"";
    if (stage_ == Stage::Lining || stage_ == Stage::Countdown) return "Fulmo: \"Line up at the flare. We're waiting on you.\"";
    if (stage_ == Stage::Racing) return "";
    if (stage_ == Stage::Done) return "Fulmo: \"Give the boys a minute to get back.\"";
    if (stage_ == Stage::Open) {
        stage_ = Stage::Offered;
        return "Fulmo: \"A quarter of the ring, flare to flare, first one past the last flare takes the pot. It's a thousand to get in. USE again if you've got it.\"";
    }
    // Offered.
    if (money < bet) { stage_ = Stage::Open; return "Fulmo: \"A thousand, friend. Come back when you've got it.\""; }
    paid = true;
    stage_ = Stage::Lining;
    return "Fulmo: \"Money's in. Get in your car and line up at the flare - the boys are rolling up now.\"";
}

int StreetRace::position() const {
    int ahead = 0;
    for (const auto& r : racers_)
        if (r.finished ? !player_finished_ || r.finish_time < clock_ : r.progress > player_progress_) ++ahead;
    return ahead + 1;
}

Vector3 StreetRace::target() const {
    switch (stage_) {
        case Stage::Lining:
        case Stage::Countdown: return grid(0);
        case Stage::Racing:
            if (next_cp_ < static_cast<int>(checkpoints_.size()))
                return world::roads::route_point(track_, checkpoints_[static_cast<std::size_t>(next_cp_)]);
            return finish();
        default: return organiser_;
    }
}

void StreetRace::reset_field(vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    for (auto& r : racers_) {
        cars.clear_autopilot(r.vehicle);
        cars.place(region, r.vehicle, r.parked, r.parked_yaw);
        r.progress = r.stuck = r.reverse = 0;
        r.attempts = 0;
        r.finished = false;
    }
}

void StreetRace::update(const Frame& f, vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    if (!track_.complete || !std::isfinite(f.dt) || f.dt <= 0) return;
    const float dt = std::min(f.dt, .1F);
    const bool night = StreetRace::night(f.hour);
    switch (stage_) {
        case Stage::Closed:
            if (night) stage_ = Stage::Open;
            break;
        case Stage::Open:
        case Stage::Offered:
            if (!night) stage_ = Stage::Closed;
            if (stage_ == Stage::Offered && flat(f.car, organiser_) > 25) stage_ = Stage::Open;
            break;
        case Stage::Lining: {
            // The field rolls up to the grid; the player lines up in the first slot.
            for (std::size_t i = 0; i < racers_.size(); ++i) {
                float yaw = 0;
                const Vector3 at = grid(static_cast<int>(i) + 1, &yaw);
                const int v = cars.find(racers_[i].vehicle);
                if (v >= 0 && flat(cars.vehicles()[static_cast<std::size_t>(v)].position, at) > 3) cars.place(region, racers_[i].vehicle, at, yaw);
                cars.set_autopilot(racers_[i].vehicle, {0, 0, true});
            }
            if (f.driving && flat(f.car, grid(0)) < 14 && std::abs(f.speed) < 4) { stage_ = Stage::Countdown; countdown_ = countdown_seconds; }
            if (flat(f.car, start()) > 900) { stage_ = Stage::Open; result_ = Result::Forfeit; reset_field(cars, region); }
            break;
        }
        case Stage::Countdown:
            for (auto& r : racers_) cars.set_autopilot(r.vehicle, {0, 0, true});
            countdown_ -= dt;
            if (countdown_ <= 0) {
                stage_ = Stage::Racing;
                clock_ = 0;
                away_ = 0;
                next_cp_ = 0;
                player_progress_ = 0;
                player_finished_ = false;
                winner_.clear();
                for (std::size_t i = 0; i < racers_.size(); ++i) {
                    auto& r = racers_[i];
                    r.route = track_;
                    r.route.lane.assign(r.route.lane.size(), (i + 1) % 2 ? world::highways::fast_lane : world::highways::slow_lane);
                    r.progress = r.stuck = r.reverse = 0;
                    r.attempts = 0;
                    r.finished = false;
                }
            }
            break;
        case Stage::Racing: {
            clock_ += dt;
            // The player: through the flares in turn; off the road or out of the car too long, out.
            player_progress_ = world::roads::route_progress(track_, f.car, player_progress_);
            if (next_cp_ < static_cast<int>(checkpoints_.size())) {
                const Vector3 flare = world::roads::route_point(track_, checkpoints_[static_cast<std::size_t>(next_cp_)]);
                if (f.driving && flat(f.car, flare) < checkpoint_radius) ++next_cp_;
            }
            // (Progress counts no further than the next flare.)
            if (next_cp_ < static_cast<int>(checkpoints_.size()))
                player_progress_ = std::min(player_progress_, checkpoints_[static_cast<std::size_t>(next_cp_)] + checkpoint_radius);
            const bool lost_track = flat(f.car, world::roads::route_point(track_, player_progress_)) > 250;
            away_ = !f.driving || lost_track ? away_ + dt : 0;
            if (!player_finished_ && next_cp_ >= static_cast<int>(checkpoints_.size())) {
                player_finished_ = true;
                const int place = position();
                result_ = place == 1 ? Result::Won : Result::Lost;
                if (place == 1) winner_ = "you";
            }
            if (!player_finished_ && away_ > 15) { result_ = Result::Forfeit; player_finished_ = true; }
            // The field: each at its pace, quicker behind the player and easing off well ahead.
            bool all_in = true;
            for (auto& r : racers_) {
                if (r.finished) { cars.set_autopilot(r.vehicle, {0, 0, true}); continue; }
                all_in = false;
                const float gap = r.progress - player_progress_;
                const float catchup = player_finished_ ? 1 : gap > 150 ? .86F : gap < -150 ? 1.12F : 1;
                ai::steer_along(r.route, r.progress, r.stuck, r.reverse, r.attempts, r.vehicle, cars, dt, r.pace * catchup, r.pace * .8F);
                if (r.progress >= finish_at_) {
                    r.finished = true;
                    r.finish_time = clock_;
                    if (winner_.empty()) winner_ = r.name;
                }
            }
            if (player_finished_ && (all_in || clock_ > 400)) { stage_ = Stage::Done; done_for_ = 0; }
            else if (all_in && !player_finished_) {
                // Everyone else is home: the player came last.
                result_ = Result::Lost;
                player_finished_ = true;
                stage_ = Stage::Done;
                done_for_ = 0;
            }
            break;
        }
        case Stage::Done:
            done_for_ += dt;
            for (auto& r : racers_) cars.set_autopilot(r.vehicle, {0, 0, true});
            // The boys drive back to the meet (out of sight: put back when the player is well away).
            if (done_for_ > 20 && flat(f.car, meet_) > 200 && flat(f.car, finish()) > 200) {
                reset_field(cars, region);
                stage_ = night ? Stage::Open : Stage::Closed;
            }
            break;
    }
}

} // namespace outland::game::racing
