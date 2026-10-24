#include "outland/game/mafia/Endgame.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace outland::game::mafia {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
bool has(const std::vector<int>& ids, int bot) { return std::find(ids.begin(), ids.end(), bot) != ids.end(); }
// Round a point, a step out on each side: a squad stands spread out, not in one spot.
Vector3 spread(Vector3 at, int k, float step) {
    const float a = static_cast<float>(k) * 2.39996F;   // the golden angle
    const float r = step * std::sqrt(static_cast<float>(k) + .5F);
    return {at.x + std::cos(a) * r, at.y, at.z + std::sin(a) * r};
}
}

void Endgame::start(ai::BattleRoyaleBots& bots, Vector3 steps, Vector3 hall, float yaw, const std::vector<Vector3>& posts,
                    const std::vector<std::string>& towns, std::uint64_t seed) {
    *this = {};
    steps_ = steps;
    hall_ = hall;
    yaw_ = yaw;
    seed_ = seed;
    const auto hidden = [&](Vector3 at, combat::WeaponId weapon, ai::BotLevel level, std::uint64_t s) {
        auto& bot = bots.add_bot(at, weapon, level, s);
        bots.retire(bot.id());
        bots.set_hidden(bot.id(), true);
        return bot.id();
    };
    for (int k = 0; k < guard_count; ++k)
        guards_.push_back(hidden(spread(steps, k, 1.6F), k % 3 == 2 ? combat::WeaponId::Smg : combat::WeaponId::Rifle, ai::BotLevel::Medium,
                                 seed + 4111ULL * static_cast<std::uint64_t>(k + 1)));
    for (std::size_t g = 0; g < posts.size(); ++g) {
        Garrison garrison;
        garrison.town = g < towns.size() ? towns[g] : std::string("Verda");
        garrison.post = posts[g];
        for (int k = 0; k < squad; ++k)
            garrison.soldiers.push_back(hidden(spread(posts[g], k, 2.2F), k == 0 ? combat::WeaponId::Smg : combat::WeaponId::Rifle, ai::BotLevel::Medium,
                                               seed + 7919ULL * static_cast<std::uint64_t>(g * squad + static_cast<std::size_t>(k) + 1)));
        garrisons_.push_back(std::move(garrison));
    }
}

bool Endgame::guard(int bot) const { return has(guards_, bot); }

bool Endgame::soldier(int bot) const {
    return std::any_of(garrisons_.begin(), garrisons_.end(), [&](const Garrison& g) { return has(g.soldiers, bot); });
}

bool Endgame::hostile(int a, int b) const {
    if (a == b) return false;
    if (soldier(a) || soldier(b)) return phase_ == EndPhase::Occupied && !(soldier(a) && soldier(b));
    if (guard(a) || guard(b)) return phase_ == EndPhase::Siege && !(guard(a) && guard(b));
    return false;
}

int Endgame::garrisons_left() const {
    return static_cast<int>(std::count_if(garrisons_.begin(), garrisons_.end(), [](const Garrison& g) { return !g.freed; }));
}

int Endgame::guards_standing(ai::BattleRoyaleBots& bots) const {
    int n = 0;
    for (const int id : guards_) if (const auto* bot = bots.find(id); bot && bot->alive() && !bots.hidden(id)) ++n;
    return n;
}

void Endgame::deploy_guard(ai::BattleRoyaleBots& bots) {
    for (std::size_t k = 0; k < guards_.size(); ++k) {
        const Vector3 at = spread(steps_, static_cast<int>(k), 1.6F);
        bots.respawn(guards_[k], at, yaw_, seed_ + 31ULL * k);
        bots.set_hidden(guards_[k], false);
        if (auto* bot = bots.find(guards_[k])) bot->set_roam_center(steps_, 6);
    }
}

void Endgame::land(ai::BattleRoyaleBots& bots, double now) {
    phase_ = EndPhase::Occupied;
    for (auto& g : garrisons_) {
        if (g.freed) continue;
        for (std::size_t k = 0; k < g.soldiers.size(); ++k) {
            const Vector3 at = spread(g.post, static_cast<int>(k), 2.2F);
            bots.respawn(g.soldiers[k], at, static_cast<float>(k) * 72.0F, seed_ + static_cast<std::uint64_t>(now * 10) + 977ULL * k);
            bots.set_hidden(g.soldiers[k], false);
            if (auto* bot = bots.find(g.soldiers[k])) bot->set_roam_center(g.post, post_radius);
        }
    }
}

void Endgame::stand_down(const std::vector<int>& ids, ai::BattleRoyaleBots& bots) {
    for (const int id : ids) { bots.retire(id); bots.set_hidden(id, true); }
}

void Endgame::gang_won(int gang, double now, ai::BattleRoyaleBots& bots) {
    if (phase_ != EndPhase::None || garrisons_.empty()) return;
    if (gang == 0 && !guards_.empty()) {
        phase_ = EndPhase::Siege;
        deploy_guard(bots);
        return;
    }
    phase_ = EndPhase::Landing;
    land_at_ = now + landing_delay;
}

std::string Endgame::update(Vector3 player, bool player_alive, double now, ai::BattleRoyaleBots& bots) {
    const auto wake = [&](const std::vector<int>& ids) {
        for (const int id : ids)
            if (const auto* bot = bots.find(id); bot && bot->alive() && !bots.hidden(id)) bots.set_asleep(id, flat(bot->position(), player) > wake_radius);
    };
    switch (phase_) {
        case EndPhase::Siege:
            wake(guards_);
            if (guards_standing(bots) == 0 && player_alive && flat(player, hall_) < hall_reach && std::abs(player.y - hall_.y) < 3) {
                stand_down(guards_, bots);
                phase_ = EndPhase::Landing;
                land_at_ = now + landing_delay;
                return "REGISTARO DE VERDA HAS FALLEN - the Green Hand hold the parliament. Verda is one nation under the Hand";
            }
            return {};
        case EndPhase::Landing:
            if (now < land_at_) return {};
            land(bots, now);
            return "RUSSIAN TROOPS HAVE COME ASHORE - \"to restore order\". They hold " + std::to_string(garrisons_left()) +
                   " towns. Drive them into the sea";
        case EndPhase::Occupied: {
            std::string said;
            for (auto& g : garrisons_) {
                if (g.freed) continue;
                wake(g.soldiers);
                const bool standing = std::any_of(g.soldiers.begin(), g.soldiers.end(), [&](int id) {
                    const auto* bot = bots.find(id);
                    return bot && bot->alive() && !bots.hidden(id); });
                if (standing) continue;
                g.freed = true;
                said = g.town + " IS FREE - " + std::to_string(garrisons_left()) + " to go";
            }
            if (garrisons_left() == 0) {
                phase_ = EndPhase::Free;
                pay_ += liberation_pay;
                for (const auto& g : garrisons_) stand_down(g.soldiers, bots);
                return "VERDA IS FREE - the last of the occupiers is gone. Verda belongs to Verdans";
            }
            return said;
        }
        default: return {};
    }
}

Vector3 Endgame::objective(Vector3 player) const {
    if (phase_ == EndPhase::Siege) return hall_;
    const Garrison* best = nullptr;
    for (const auto& g : garrisons_)
        if (!g.freed && (!best || flat(g.post, player) < flat(best->post, player))) best = &g;
    return best ? best->post : player;
}

std::string Endgame::objective_town(Vector3 player) const {
    if (phase_ == EndPhase::Siege) return "Registaro de Verda";
    const Garrison* best = nullptr;
    for (const auto& g : garrisons_)
        if (!g.freed && (!best || flat(g.post, player) < flat(best->post, player))) best = &g;
    return best ? best->town : std::string();
}

std::string Endgame::serialise() const {
    std::ostringstream out;
    out << "endgame 1 " << static_cast<int>(phase_) << ' ' << garrisons_.size();
    for (const auto& g : garrisons_) out << ' ' << (g.freed ? 1 : 0);
    out << '\n';
    return out.str();
}

bool Endgame::parse(const std::string& text, ai::BattleRoyaleBots& bots) {
    std::istringstream in(text);
    std::string tag;
    int version = 0, phase = 0;
    std::size_t count = 0;
    if (!(in >> tag >> version >> phase >> count) || tag != "endgame" || version != 1 || phase < 0 || phase > static_cast<int>(EndPhase::Free))
        return false;
    std::vector<bool> freed;
    for (std::size_t i = 0; i < count; ++i) {
        int f = 0;
        if (!(in >> f)) return false;
        freed.push_back(f != 0);
    }
    for (std::size_t i = 0; i < garrisons_.size() && i < freed.size(); ++i) garrisons_[i].freed = freed[i];
    phase_ = static_cast<EndPhase>(phase);
    // Back where it stood: the guard on the steps, or the troops in the towns still held (at
    // full strength - they were reinforced while you were away). A landing due lands now.
    if (phase_ == EndPhase::Siege) deploy_guard(bots);
    if (phase_ == EndPhase::Landing || phase_ == EndPhase::Occupied) land(bots, 0);
    if (phase_ == EndPhase::Occupied && garrisons_left() == 0) phase_ = EndPhase::Free;
    return true;
}

} // namespace outland::game::mafia
