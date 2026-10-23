#include "outland/game/lan/SquadRevive.hpp"
#include "outland/game/lan/LanPlay.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game::lan {

namespace {
bool down(const Remote& r) { return r.known && (r.state.flags & net::StateAlive) && (r.state.status & net::StatusDowned); }
bool standing(const Remote& r) { return r.known && (r.state.flags & net::StateAlive) && !(r.state.status & net::StatusDowned); }
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

bool SquadRevive::update(float dt, Health& health, bool squad_play, int standing_teammates, bool helped) {
    health.set_downable(squad_play && standing_teammates > 0 && !health.downed());
    if (!health.downed()) { progress_ = 0; pool_ = health.alive() ? Health::down_pool : 0; return false; }
    if (standing_teammates <= 0) {
        // Nobody left to pick them up: out.
        health.damage(health.current() + 1);
        progress_ = 0;
        return false;
    }
    if (helped) {
        progress_ += dt / revive_seconds;
        if (progress_ >= 1) {
            health.get_up(get_up_health);
            progress_ = 0;
            return true;
        }
    } else {
        progress_ = std::max(0.0F, progress_ - dt / 1.5F);
        health.damage(Health::down_pool / bleed_seconds * dt);
    }
    pool_ = health.current();
    return false;
}

float SquadRevive::bleed() const { return std::clamp(pool_ / Health::down_pool, 0.0F, 1.0F); }

std::uint8_t SquadRevive::downed_near(Vector3 feet, const std::vector<Remote>& remotes,
                                      const std::function<bool(std::uint8_t)>& teammate, float within) {
    std::uint8_t best = nobody;
    float nearest = within;
    for (const auto& r : remotes) {
        if (!down(r) || !teammate(r.slot) || std::abs(r.shown.y - feet.y) > 2.5F) continue;
        if (const float d = flat(r.shown, feet); d <= nearest) { nearest = d; best = r.slot; }
    }
    return best;
}

bool SquadRevive::helped_by(std::uint8_t me, Vector3 feet, const std::vector<Remote>& remotes,
                            const std::function<bool(std::uint8_t)>& teammate) {
    return std::any_of(remotes.begin(), remotes.end(), [&](const Remote& r) {
        return standing(r) && teammate(r.slot) && r.state.reviving == me && flat(r.shown, feet) <= keep_reach + 1;
    });
}

}
