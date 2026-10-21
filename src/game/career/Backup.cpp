#include "outland/game/career/Backup.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/ai/BotDrivers.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::career {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 ground(Vector3 p, const world::VerdaRegion& region) {
    const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
    const float probe = std::max(terrain, p.y) + 1.2F;
    return {p.x, world::physics::WorldCollision::ground_height({p.x, probe, p.z}, probe, region), p.z};
}
}

void Backup::start(ai::BattleRoyaleBots& bots, Vector3 park, std::uint64_t seed) {
    ids_.clear();
    phase_ = BackupPhase::Idle;
    ready_at_ = 0;
    seed_ = seed;
    for (int k = 0; k < officers; ++k) {
        auto& bot = bots.add_bot(park, combat::WeaponId::Rifle, ai::BotLevel::Hard, seed + 7001ULL * static_cast<std::uint64_t>(k + 1));
        ids_.push_back(bot.id());
        bots.retire(bot.id());
        bots.set_hidden(bot.id(), true);
    }
}

bool Backup::officer(int bot) const {
    return std::find(ids_.begin(), ids_.end(), bot) != ids_.end();
}

std::string Backup::call(Vector3 player, double now, Vector3 station, float yaw, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                         vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    if (ids_.empty()) return "Dispatch: no units to send";
    if (phase_ == BackupPhase::Driving) return "Dispatch: backup is on its way - hold on";
    if (phase_ == BackupPhase::OnScene) return "Backup is with you";
    if (now < ready_at_) return "Dispatch: no units free - " + std::to_string(static_cast<int>(std::ceil(ready_at_ - now))) + " s";
    // On their feet at the station...
    for (std::size_t k = 0; k < ids_.size(); ++k) {
        const Vector3 at = ground(Vector3Add(station, {static_cast<float>(k) * 1.2F, 0, 0}), region);
        bots.respawn(ids_[k], at, yaw, seed_ + static_cast<std::uint64_t>(now * 10) + k);
        bots.set_asleep(ids_[k], false);
    }
    // ...and into the squad car: the one from last time, or a new one at the station.
    const auto usable = [&] { return !car_.empty() && cars.find(car_) >= 0; };
    bool driving = false;
    if (usable()) driving = drivers.dispatch(bots, cars, region, ids_, cars.vehicles()[static_cast<std::size_t>(cars.find(car_))].position, player, 1e6F, car_);
    if (!driving) {
        std::vector<std::string> before;
        for (const auto& car : cars.vehicles()) before.push_back(car.id);
        if (cars.spawn(region, station, yaw, "police"))
            for (const auto& car : cars.vehicles())
                if (std::find(before.begin(), before.end(), car.id) == before.end()) { car_ = car.id; break; }
        if (usable()) driving = drivers.dispatch(bots, cars, region, ids_, station, player, 1e6F, car_);
    }
    phase_ = driving ? BackupPhase::Driving : BackupPhase::OnScene;
    last_moved_ = now;
    last_seen_ = {1e9F, 0, 1e9F};
    until_ = now + stay;
    if (driving) return "Dispatch: backup rolling from the station - two officers";
    on_foot(player, station, bots, region);
    return "Dispatch: two officers on foot, right round the corner";
}

void Backup::on_foot(Vector3 player, Vector3 from, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region) {
    // No road for a car (too close, or stuck in traffic): they come running from round the corner.
    Vector3 away = Vector3Subtract(from, player);
    away.y = 0;
    away = Vector3Length(away) > 1 ? Vector3Normalize(away) : Vector3{0, 0, 1};
    for (std::size_t k = 0; k < ids_.size(); ++k) {
        const Vector3 at = ground(Vector3Add(player, Vector3Scale(away, 30 + static_cast<float>(k) * 2)), region);
        if (auto* bot = bots.find(ids_[k]); bot && bot->alive()) { bot->set_position(at); bot->set_roam_center(player, follow_radius); }
    }
}

std::string Backup::update(Vector3 player, double now, bool player_alive, bool fighting, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                           vehicles::VehicleSystem& cars, const world::VerdaRegion& region) {
    if (phase_ == BackupPhase::Idle) return {};
    if (!player_alive) { dismiss(bots, now); return {}; }
    int standing = 0;
    bool riding = false;
    for (const int id : ids_) {
        const auto* bot = bots.find(id);
        if (bot && bot->alive()) ++standing;
        if (drivers.ride_of(id)) riding = true;
    }
    if (standing == 0) { dismiss(bots, now); return "Your backup is down - dispatch will send more when it can"; }
    std::string said;
    if (phase_ == BackupPhase::Driving) {
        if (riding) {
            // Stuck in traffic (no headway for a while): they leave the car and come on foot.
            const auto* bot = bots.find(ids_.front());
            const Vector3 at = bot ? bot->position() : player;
            if (flat(at, last_seen_) > 4) { last_seen_ = at; last_moved_ = now; }
            if (now - last_moved_ < stuck_seconds) return {};
            for (const int id : ids_) drivers.cancel(id, bots, cars);
            on_foot(player, at, bots, region);
        }
        // Out of the car: on the scene.
        phase_ = BackupPhase::OnScene;
        until_ = now + stay;
        said = "Backup is here";
    }
    // At your side: they keep close and fight whatever comes.
    if (fighting) until_ = std::max(until_, now + 30);
    for (const int id : ids_) {
        auto* bot = bots.find(id);
        if (!bot || !bot->alive() || bots.mounted(id)) continue;
        if (flat(bot->position(), player) > 120) bot->set_position(player);   // left far behind: they catch up
        bot->set_roam_center(player, follow_radius);
    }
    if (now >= until_) { dismiss(bots, now); return "Backup heads back to the station"; }
    return said;
}

void Backup::dismiss(ai::BattleRoyaleBots& bots, double now) {
    for (const int id : ids_) {
        bots.set_mounted(id, false);
        bots.retire(id);
        bots.set_hidden(id, true);
    }
    if (phase_ != BackupPhase::Idle) ready_at_ = now + cooldown;
    phase_ = BackupPhase::Idle;
}

} // namespace outland::game::career
