#include "outland/game/career/Crew.hpp"
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
bool contains(const std::vector<int>& list, int id) { return std::find(list.begin(), list.end(), id) != list.end(); }
}

void Crew::start(ai::BattleRoyaleBots& bots, Vector3 park, std::uint64_t seed) {
    clear();
    seed_ = seed;
    auto& bot = bots.add_bot(park, combat::WeaponId::Rifle, ai::BotLevel::Hard, seed + 9103ULL);
    partner_ = bot.id();
    bots.retire(partner_);
    bots.set_hidden(partner_, true);
}

bool Crew::recruit(int bot) {
    if (bot <= 0 || contains(members_, bot) || static_cast<int>(members_.size()) >= max_gang) return false;
    members_.push_back(bot);
    return true;
}

bool Crew::assign_partner(Vector3 at, float yaw, ai::BattleRoyaleBots& bots, double now) {
    if (partner_ < 0 || partner_on_) return false;
    bots.respawn(partner_, at, yaw, seed_ + static_cast<std::uint64_t>(now * 10));
    bots.set_hidden(partner_, false);
    bots.set_asleep(partner_, false);
    if (auto* bot = bots.find(partner_)) bot->set_roam_center(at, follow_radius);
    partner_on_ = true;
    return true;
}

void Crew::dismiss_partner(ai::BattleRoyaleBots& bots) {
    if (partner_ < 0) return;
    std::erase(aboard_, partner_);
    bots.set_mounted(partner_, false);
    bots.retire(partner_);
    bots.set_hidden(partner_, true);
    partner_on_ = false;
}

std::vector<int> Crew::dismiss(ai::BattleRoyaleBots& bots) {
    dismiss_partner(bots);
    for (const int id : members_) {
        bots.set_mounted(id, false);
        bots.set_hidden(id, false);
    }
    auto gone = std::move(members_);
    members_.clear();
    aboard_.clear();
    ride_car_.clear();
    ride_driver_ = -1;
    return gone;
}

bool Crew::member(int bot) const { return contains(members_, bot) || (partner_on_ && bot == partner_); }

std::vector<int> Crew::everyone() const {
    std::vector<int> out = members_;
    if (partner_on_ && partner_ >= 0) out.push_back(partner_);
    return out;
}

bool Crew::aboard(int bot) const { return contains(aboard_, bot); }

void Crew::get_out(Vector3 car, float yaw, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region) {
    // Out of the doors either side and behind, wherever there is room.
    const float a = yaw * DEG2RAD;
    const Vector3 side{std::cos(a), 0, -std::sin(a)}, back{-std::sin(a), 0, -std::cos(a)};
    for (std::size_t k = 0; k < aboard_.size(); ++k) {
        const int id = aboard_[k];
        bots.set_mounted(id, false);
        bots.set_hidden(id, false);
        bots.set_asleep(id, false);
        auto* bot = bots.find(id);
        if (!bot) continue;
        const float along = static_cast<float>(k / 2) * 1.6F + 1.0F, first = k % 2 == 0 ? -2.4F : 2.4F;
        Vector3 at = ground(car, region);
        for (const float s : {first, -first, first * 1.5F, -first * 1.5F}) {
            const Vector3 p = ground({car.x + side.x * s + back.x * along, car.y, car.z + side.z * s + back.z * along}, region);
            if (world::physics::WorldCollision::body_blocked(p, p.y, region, .35F)) continue;
            at = p;
            break;
        }
        bot->set_position(at);
        bot->set_yaw(yaw);
    }
    aboard_.clear();
}

std::vector<int> Crew::update(const Frame& frame, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region, std::string& said) {
    std::vector<int> fell;
    // The fallen leave the crew (a gang member goes back to the Mafio: he is back at his hideout later).
    for (const int id : std::vector<int>(members_)) {
        const auto* bot = bots.find(id);
        if (bot && bot->alive()) continue;
        std::erase(members_, id);
        std::erase(aboard_, id);
        fell.push_back(id);
        said = "One of your crew is down";
    }
    if (partner_on_) {
        const auto* bot = bots.find(partner_);
        if (!bot || !bot->alive()) {
            partner_on_ = false;
            std::erase(aboard_, partner_);
            bots.set_hidden(partner_, true);
            said = "Your partner is down - the desk will give you another";
        }
    }
    if (!frame.alive) {
        if (!aboard_.empty()) get_out(last_car_, last_yaw_, bots, region);
        driving_.clear();
        return fell;
    }
    // Riding with the crew: the ride (game::ai::BotDrivers) carries them.
    if (frame.riding) return fell;
    const auto crew = everyone();
    if (!frame.driving.empty()) {
        // The player drove off: those close enough got in (they ride out of sight).
        if (driving_ != frame.driving) {
            if (!aboard_.empty()) get_out(last_car_, last_yaw_, bots, region);
            for (const int id : crew) {
                const auto* bot = bots.find(id);
                if (!bot || bots.mounted(id) || flat(bot->position(), frame.car) > board_radius) continue;
                aboard_.push_back(id);
                bots.set_mounted(id, true);
                bots.set_hidden(id, true);
                bots.set_asleep(id, true);
            }
        }
        driving_ = frame.driving;
        last_car_ = frame.car;
        last_yaw_ = frame.car_yaw;
        for (const int id : aboard_)
            if (auto* bot = bots.find(id)) bot->set_position(frame.car);
    } else if (!driving_.empty()) {
        // Out of the car: so are they.
        get_out(last_car_, last_yaw_, bots, region);
        driving_.clear();
    }
    if (!driving_.empty()) return fell;
    // On foot: they keep close; left far behind (on foot, across the island), they catch up.
    for (const int id : crew) {
        auto* bot = bots.find(id);
        if (!bot || !bot->alive() || bots.mounted(id)) continue;
        bots.set_asleep(id, false);
        if (flat(bot->position(), frame.player) > catch_up) {
            const float a = static_cast<float>(id) * 1.7F;
            bot->set_position(ground({frame.player.x + std::sin(a) * 6, frame.player.y, frame.player.z + std::cos(a) * 6}, region));
        }
        bot->set_roam_center(frame.player, follow_radius);
    }
    return fell;
}

int Crew::ride(const std::string& vehicle, Vector3 car, Vector3 goal, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
               vehicles::VehicleSystem& cars, world::VerdaRegion& region, float reach) {
    if (!ride_car_.empty() && riding(drivers)) return -1;
    std::vector<int> crew;
    for (const int id : everyone()) {
        const auto* bot = bots.find(id);
        if (bot && bot->alive() && !bots.mounted(id) && flat(bot->position(), car) <= reach) crew.push_back(id);
    }
    if (crew.empty()) return -1;
    // The nearest takes the wheel.
    std::sort(crew.begin(), crew.end(), [&](int a, int b) { return flat(bots.find(a)->position(), car) < flat(bots.find(b)->position(), car); });
    if (!drivers.dispatch(bots, cars, region, crew, car, goal, reach + 10, vehicle)) return -1;
    for (const int id : crew) {
        bots.set_hidden(id, true);
        bots.set_asleep(id, false);
    }
    ride_car_ = vehicle;
    ride_driver_ = crew.front();
    return ride_driver_;
}

bool Crew::riding(const ai::BotDrivers& drivers) const {
    return ride_driver_ >= 0 && drivers.ride_of(ride_driver_) != nullptr;
}

void Crew::end_ride(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars, const world::VerdaRegion& region) {
    if (ride_driver_ < 0) return;
    const int v = cars.find(ride_car_);
    std::vector<int> aboard;
    if (const auto* ride = drivers.ride_of(ride_driver_)) {
        aboard.push_back(ride->bot);
        aboard.insert(aboard.end(), ride->passengers.begin(), ride->passengers.end());
        drivers.cancel(ride_driver_, bots, cars);
        // Pulled over here: everyone out of the doors.
        if (v >= 0) {
            const auto& car = cars.vehicles()[static_cast<std::size_t>(v)];
            aboard_ = aboard;
            get_out(car.position, car.yaw, bots, region);
        }
    } else {
        aboard = everyone();
    }
    for (const int id : aboard) bots.set_hidden(id, false);
    ride_car_.clear();
    ride_driver_ = -1;
}

} // namespace outland::game::career
