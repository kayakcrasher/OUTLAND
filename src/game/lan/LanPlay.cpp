#include "outland/game/lan/LanPlay.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::lan {
namespace {
float wrap_degrees(float d) {
    while (d > 180) d -= 360;
    while (d < -180) d += 360;
    return d;
}
}

void LanPlay::clear(characters::NpcSystem* npcs) {
    if (npcs) for (const auto& r : remotes_) {
        const int index = npcs->find_bot(body_base + r.slot);
        if (index >= 0) npcs->set_bot_hidden(body_base + r.slot, true);
    }
    remotes_.clear();
    last_cars_.clear();
    mirrored_cars_.clear();
}

Remote* LanPlay::find(std::uint8_t slot) {
    for (auto& r : remotes_) if (r.slot == slot) return &r;
    return nullptr;
}

void LanPlay::received(std::uint8_t slot, const std::string& name, const net::PlayerState& state, double now) {
    Remote* r = find(slot);
    if (!r) {remotes_.push_back({}); r = &remotes_.back(); r->slot = slot;}
    if (r->known && state.time < r->state.time && r->state.time - state.time < 30) return; // late, out of order
    if (state.flags & net::StateFiring) ++r->fired;   // shots in the 1/20 s before this state
    r->last_flags = state.flags;
    r->name = name;
    r->state = state;
    r->heard = now;
    if (!r->known) {r->shown = state.feet; r->shown_yaw = state.yaw * RAD2DEG; r->known = true;}
}

void LanPlay::remove(std::uint8_t slot, characters::NpcSystem& npcs, vehicles::VehicleSystem& cars) {
    if (Remote* r = find(slot)) {
        if (r->car >= 0 && r->car < static_cast<int>(cars.vehicles().size())) cars.release(cars.vehicles()[static_cast<std::size_t>(r->car)].id);
        npcs.set_bot_hidden(body_base + slot, true);
        npcs.set_bot(body_base + slot, r->shown, r->shown_yaw, 0, characters::NpcState::Dead);
    }
    std::erase_if(remotes_, [&](const Remote& r) { return r.slot == slot; });
}

std::vector<std::pair<Vector3, std::uint8_t>> LanPlay::take_shots() {
    std::vector<std::pair<Vector3, std::uint8_t>> out;
    for (auto& r : remotes_) {
        if (r.fired > 0 && r.known) out.emplace_back(Vector3{r.shown.x, r.shown.y + 1.4F, r.shown.z}, r.state.weapon);
        r.fired = 0;
    }
    return out;
}

int LanPlay::alive() const {
    int n = 0;
    for (const auto& r : remotes_) n += r.known && (r.state.flags & net::StateAlive);
    return n;
}

std::vector<ai::OtherPlayer> LanPlay::others() const {
    std::vector<ai::OtherPlayer> out;
    for (const auto& r : remotes_) {
        if (!r.known) continue;
        // In the air (the drop) nobody can be shot at yet.
        const bool flying = (r.state.flags & net::StateDropping) != 0;
        out.push_back({body_base + r.slot, r.shown, r.state.velocity, (r.state.flags & net::StateAlive) && !flying,
                       (r.state.flags & net::StateDriving) != 0});
    }
    return out;
}

void LanPlay::update(float dt, characters::NpcSystem& npcs, const characters::CharacterRegistry* registry,
                     vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    for (auto& r : remotes_) {
        if (!r.known) continue;
        const auto& s = r.state;
        // Smooth toward where they are now (their last state, carried on by its velocity a little).
        const Vector3 target = Vector3Add(s.feet, Vector3Scale(s.velocity, .05F));
        const float follow = 1 - std::exp(-14 * std::max(0.0F, dt));
        r.shown = Vector3Distance(r.shown, target) > 8 ? target : Vector3Lerp(r.shown, target, follow);
        r.shown_yaw += wrap_degrees(s.yaw * RAD2DEG - r.shown_yaw) * follow;
        const int body = body_base + r.slot;
        if (npcs.find_bot(body) < 0) {
            std::string character = "bot";
            if (registry) {
                const auto seed = static_cast<std::uint64_t>(r.slot) * 2654435761ULL + 17;
                const auto* definition = registry->choose(characters::CharacterPool::Civilian, seed);
                if (!definition) definition = registry->choose(characters::CharacterPool::Hostile, seed);
                if (definition) character = definition->id;
            }
            npcs.spawn_bot(body, character, r.shown, r.shown_yaw);
        }
        const bool alive = (s.flags & net::StateAlive) != 0, driving = (s.flags & net::StateDriving) != 0;
        const float speed = std::hypot(s.velocity.x, s.velocity.z);
        const auto state = !alive ? characters::NpcState::Dead : speed > 4 ? characters::NpcState::Chase :
                           speed > .3F ? characters::NpcState::Wander : characters::NpcState::Idle;
        // NpcSystem's yaw is the body's facing in degrees, the player's yaw turned round.
        npcs.set_bot(body, r.shown, r.shown_yaw + 180, s.health, state);
        npcs.set_bot_weapon(body, s.weapon);
        npcs.set_bot_hidden(body, driving || (s.flags & net::StateDropping) != 0);
        // Their car goes where they drive it.
        const int car = driving ? s.vehicle : -1;
        if (r.car >= 0 && r.car != car && r.car < static_cast<int>(cars.vehicles().size()))
            cars.release(cars.vehicles()[static_cast<std::size_t>(r.car)].id);
        r.car = car;
        if (car >= 0 && car < static_cast<int>(cars.vehicles().size())) {
            const auto& vehicle = cars.vehicles()[static_cast<std::size_t>(car)];
            const Vector3 at = Vector3Lerp(vehicle.position, Vector3Add(s.car, Vector3Scale(s.velocity, .05F)), follow);
            const float yaw = vehicle.yaw + wrap_degrees(s.car_yaw - vehicle.yaw) * follow;
            cars.glide(region, vehicle.id, at, yaw, s.car_speed, 0, dt);
        }
    }
}

void LanPlay::draw_names(const Camera3D& camera, int width, int height) const {
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    for (const auto& r : remotes_) {
        if (!r.known || (r.state.flags & net::StateDriving)) continue;
        const Vector3 head{r.shown.x, r.shown.y + 2.15F, r.shown.z};
        const Vector3 to = Vector3Subtract(head, camera.position);
        const float distance = Vector3Length(to);
        if (distance > 180 || Vector3DotProduct(to, forward) <= 0) continue;
        const Vector2 at = GetWorldToScreenEx(head, camera, width, height);
        const int size = std::clamp(static_cast<int>(22 - distance * .06F), 11, 20);
        const char* text = r.name.c_str();
        const int w = MeasureText(text, size);
        const Color ink = (r.state.flags & net::StateAlive) ? Color{120, 220, 255, 235} : Color{200, 200, 200, 160};
        DrawText(text, static_cast<int>(at.x) - w / 2 + 1, static_cast<int>(at.y) + 1, size, Fade(BLACK, .6F));
        DrawText(text, static_cast<int>(at.x) - w / 2, static_cast<int>(at.y), size, ink);
    }
}

net::BotSnapshot LanPlay::snapshot(const ai::BattleRoyaleBots& bots, const vehicles::VehicleSystem& cars, float elapsed) {
    net::BotSnapshot out;
    out.elapsed = elapsed;
    for (const auto& bot : bots.bots()) {
        if (bot.id() < 0 || bot.id() > 65535) continue;
        const float speed = std::hypot(bot.velocity().x, bot.velocity().z);
        std::uint8_t flags = 0;
        if (bot.moving()) flags |= net::BotMoving;
        if (speed > ai::CombatBot::walk_speed + .6F) flags |= net::BotRunning;
        if (bots.mounted(bot.id())) flags |= net::BotMounted;
        // A shot lately: its tracer (shorter-lived than the time between snapshots) starts at this bot's gun.
        for (const auto& tracer : bots.tracers())
            if (std::hypot(tracer.start.x - bot.position().x, tracer.start.z - bot.position().z) < 2.5F) {flags |= net::BotFiring; break;}
        out.bots.push_back({static_cast<std::uint16_t>(bot.id()), bot.position(), bot.yaw(),
                            static_cast<std::uint8_t>(std::clamp(bot.health(), 0.0F, 255.0F)), flags});
    }
    const auto& vehicles = cars.vehicles();
    last_cars_.resize(vehicles.size(), Vector3{1e9F, 0, 0});
    for (std::size_t i = 0; i < vehicles.size() && i < 65535; ++i) {
        const auto& v = vehicles[i];
        if (Vector3Distance(v.position, last_cars_[i]) < .05F && std::abs(v.speed) < .05F) continue;
        last_cars_[i] = v.position;
        out.cars.push_back({static_cast<std::uint16_t>(i), v.position, v.yaw, v.speed});
    }
    return out;
}

void LanPlay::apply(const net::BotSnapshot& snapshot, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars,
                    world::VerdaRegion& region, float dt) {
    const float step = std::max(dt, 1.0F / snapshot_rate);
    for (const auto& b : snapshot.bots) {
        auto* bot = bots.find(b.id);
        if (!bot) continue;
        bot->puppet(b.feet, b.yaw, (b.flags & net::BotMoving) ? step : 0);
        bot->set_health(b.health);
        bots.set_mounted(b.id, (b.flags & net::BotMounted) != 0);
    }
    for (const auto& c : snapshot.cars) {
        if (c.index >= cars.vehicles().size()) continue;
        const auto& id = cars.vehicles()[c.index].id;
        // A car this player is driving is theirs, not the host's.
        if (cars.driver() && cars.driver()->id == id) continue;
        cars.glide(region, id, c.position, c.yaw, c.speed, 0, step);
        if (std::find(mirrored_cars_.begin(), mirrored_cars_.end(), static_cast<int>(c.index)) == mirrored_cars_.end())
            mirrored_cars_.push_back(c.index);
    }
}

}
