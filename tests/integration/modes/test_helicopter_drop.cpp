#include "outland/game/aircraft/HelicopterDrop.hpp"
#include "outland/game/ModeSession.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include "outland/world/VerdaLayout.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <string>

// The Battle Royale drop: eight routes straight across Verda, a row of three helicopters, a
// jumper's freefall and canopy (smooth, steerable, never a hard landing), bots that land where
// they planned, and the zone's phases closing on random circles inside each other, on land.
using namespace outland;
using namespace outland::game::aircraft;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "[FAIL] " << what << '\n'; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
} // namespace

int main() {
    world::terrain::TerrainHeight::set_coastal_layout(true);
    // Routes: all eight headings turn up, each a straight line over the sea at both ends, high
    // above every hill, crossing the island.
    std::set<int> headings;
    for (std::uint64_t seed = 1; seed <= 400; ++seed) {
        const auto path = make_flight(seed);
        headings.insert(path.heading);
        const auto d = heading_direction(path.heading);
        check(std::abs(path.direction.x - d.x) < 1e-5F && std::abs(path.direction.z - d.y) < 1e-5F, "route follows its heading");
        check(std::abs(Vector3Length(path.direction) - 1) < 1e-4F && std::abs(Vector3DotProduct(path.direction, path.side)) < 1e-5F, "route axes");
        check(path.land_from > 0 && path.land_to < path.length && path.land_to - path.land_from > 2000, "route crosses the island");
        check(world::terrain::TerrainHeight::sample(path.start.x, path.start.z) < world::layout::sea_level + 1, "starts over the sea");
        float highest = -1e9F;
        for (float s = 0; s <= path.length; s += 25) {
            const auto p = path.point(s);
            highest = std::max(highest, world::terrain::TerrainHeight::sample(p.x, p.z));
        }
        check(path.altitude - highest > 400, "flies high above the hills");
        check(std::abs(path.along(path.point(1234)) - 1234) < .01F && std::abs(path.offset(Vector3Add(path.point(500), Vector3Scale(path.side, 30))) - 30) < .01F, "along/offset");
    }
    check(headings.size() == heading_count, "all eight routes are used");
    check(std::string(heading_name(4)) == "SOUTH-WEST TO NORTH-EAST" && heading_direction(4).x > 0 && heading_direction(4).y < 0, "south-west to north-east heads north-east (-z is north)");

    // The row of three: side by side across the line, the middle one ahead, moving at speed.
    const auto path = make_flight(7);
    for (float t : {5.0F, 20.0F, 60.0F}) {
        const auto l = helicopter_position(path, 0, t), m = helicopter_position(path, 1, t), r = helicopter_position(path, 2, t);
        check(std::abs(path.offset(l) + Formation::spacing) < .5F && std::abs(path.offset(m)) < .5F && std::abs(path.offset(r) - Formation::spacing) < .5F, "three abreast");
        check(path.along(m) > path.along(l) && path.along(m) > path.along(r), "the middle one leads");
    }
    check(std::abs(path.along(helicopter_position(path, 1, 10)) - path.speed * 10) < .5F, "flies at its speed");
    check(last_jump_time(path) * path.speed < path.land_to && last_jump_time(path) * path.speed > path.land_to - 100, "everyone jumps over the last land");

    // A jump left alone: smooth acceleration to terminal speed (no jolt), the canopy opening by
    // itself low down, a slow landing on the ground passed in.
    const float ground = 20;
    {
        Skydiver diver;
        diver.jump({0, 650, 0}, {30, 0, 0}, 0);
        check(diver.stage() == DiveStage::Freefall && diver.airborne(), "jumped");
        float previous = 0, top = 0, max_jerk = 0, opened_at = -1;
        int steps = 0;
        while (diver.airborne() && steps < 20000) {
            diver.update(1.0F / 60, {}, ground);
            const float down = -diver.velocity().y;
            if (diver.airborne()) max_jerk = std::max(max_jerk, std::abs(down - previous) * 60);
            previous = down;
            top = std::max(top, down);
            if (diver.opened_now()) opened_at = diver.height_above(ground);
            ++steps;
        }
        check(top > Skydiver::terminal - 2 && top < Skydiver::terminal + 3, "falls at terminal speed");
        check(opened_at > 0 && opened_at <= Skydiver::auto_open + 1 && opened_at > Skydiver::auto_open - 5, "opens by itself low down");
        check(diver.stage() == DiveStage::Landed && diver.landing_speed() < 8, "lands softly");
        check(std::abs(diver.position().y - 1 - ground) < .01F, "on the ground");
        check(max_jerk < 120, "no jolts (deceleration under 12 g)");
        const float seconds = steps / 60.0F;
        check(seconds > 30 && seconds < 75, "a drop lasts half a minute to a minute or so");
    }
    // Steering: forward glides much further than falling straight; looking down and forward
    // dives faster than a plain fall; the canopy can be opened on request (not straight away).
    {
        Skydiver glide, straight, dive;
        for (auto* d : {&glide, &straight, &dive}) d->jump({0, 650, 0}, {}, 0);
        DiveInput forward; forward.forward = 1;
        DiveInput down = forward; down.pitch = -1.2F;
        for (int i = 0; i < 60 * 6; ++i) {
            glide.update(1.0F / 60, forward, ground);
            straight.update(1.0F / 60, {}, ground);
            dive.update(1.0F / 60, down, ground);
        }
        check(flat(glide.position(), {}) > 120 && flat(straight.position(), {}) < 10, "forward glides");
        check(glide.position().y > straight.position().y && dive.position().y < straight.position().y, "gliding falls slower, diving faster");
        check(dive.body_pitch() > glide.body_pitch(), "head-down in a dive");
        DiveInput open; open.deploy = true;
        Skydiver early;
        early.jump({0, 650, 0}, {}, 0);
        early.update(.1F, open, ground);
        check(early.stage() == DiveStage::Freefall, "the canopy cannot open in the doorway");
        for (int i = 0; i < 20; ++i) early.update(.1F, open, ground);
        check(early.stage() == DiveStage::Canopy, "the canopy opens on request");
        // Under the canopy: it turns toward where the player looks, banks into the turn,
        // brakes and speeds up.
        DiveInput turn; turn.yaw = 1.5F;
        for (int i = 0; i < 60 * 3; ++i) early.update(1.0F / 60, turn, ground);
        check(early.canopy() == 1 && std::abs(early.heading() - 1.5F) < .4F, "steers where the player looks");
        DiveInput brake; brake.forward = -1; brake.yaw = early.heading();
        DiveInput fast; fast.forward = 1; fast.yaw = early.heading();
        Skydiver a = early, b = early;
        for (int i = 0; i < 60 * 4; ++i) { a.update(1.0F / 60, brake, ground); b.update(1.0F / 60, fast, ground); }
        check(Vector3Length({a.velocity().x, 0, a.velocity().z}) < 6 && Vector3Length({b.velocity().x, 0, b.velocity().z}) > 14, "brake and speed up");
        check(-a.velocity().y < -b.velocity().y, "braking sinks slower");
    }
    // Reach: a full glide covers well over the bots' landing reach.
    {
        Skydiver far;
        far.jump({0, 650, 0}, {}, 0);
        DiveInput forward; forward.forward = 1;
        while (far.airborne()) far.update(1.0F / 30, forward, ground);
        check(flat(far.position(), {}) > drop_reach, "a full glide reaches past the bots' drop reach");
    }

    // Bots: each leaves a helicopter, falls, opens and lands exactly where planned, in order.
    {
        std::vector<std::pair<int, Vector3>> landings;
        for (int i = 0; i < 23; ++i) {
            const float s = path.land_from + 200 + i * (path.land_to - path.land_from - 400) / 23;
            const auto p = Vector3Add(path.point(s), Vector3Scale(path.side, static_cast<float>((i % 7) - 3) * 180));
            landings.emplace_back(i + 1, Vector3{p.x, world::terrain::TerrainHeight::sample(p.x, p.z), p.z});
        }
        const auto jumps = plan_bot_jumps(path, landings, 99);
        check(jumps.size() == landings.size(), "a jump for every bot");
        for (const auto& j : jumps) {
            check(j.jump_time > 0 && j.jump_time < j.open_time && j.open_time < j.land_time, "jump, open, land in order");
            check(j.jump_time <= last_jump_time(path), "nobody rides past the last land");
            check(bot_in_air(j, j.jump_time - 1).stage == DiveStage::Aboard, "aboard before the jump");
            check(bot_in_air(j, (j.jump_time + j.open_time) / 2).stage == DiveStage::Freefall, "then falling");
            check(bot_in_air(j, (j.open_time + j.land_time) / 2).stage == DiveStage::Canopy, "then under a canopy");
            const auto down = bot_in_air(j, j.land_time);
            check(down.stage == DiveStage::Landed && flat(down.position, j.landing) < .01F, "down where planned");
            float last_y = 1e9F;
            for (float t = j.jump_time; t < j.land_time; t += .5F) {
                const auto air = bot_in_air(j, t);
                check(air.position.y <= last_y + .01F, "never climbs");
                last_y = air.position.y;
            }
            const int side = path.offset(j.landing) < -20 ? 0 : path.offset(j.landing) > 20 ? 2 : 1;
            check(j.helicopter == side, "leaves the helicopter on its side");
        }
    }

    // The zone: phases wait, then close on the next circle, each wholly inside the last, centred
    // on land; damage rises; the circle ends closed.
    {
        game::ModeSession session;
        session.start(game::GameMode::BattleRoyale, 23, game::ai::BotLevel::Medium);
        const auto land = [](float x, float z) { return world::terrain::TerrainHeight::sample(x, z) > world::layout::sea_level + 4; };
        session.plan_circles(31337, land);
        game::ai::BattleRoyaleBots bots;
        auto& keeper = bots.add_bot({0, 0, 0}, game::combat::WeaponId::Rifle, game::ai::BotLevel::Easy, 1);
        const int keeper_id = keeper.id();
        // Someone has to stay alive (and inside) for the match to run its course.
        const auto keep = [&](game::ai::BattleRoyaleBots& field, int id, Vector2 at) {
            if (auto* bot = field.find(id)) { bot->set_position({at.x, 0, at.y}); bot->set_health(100); }
        };
        game::Health health;
        const auto& phases = session.circle.phases;
        auto z = session.zone();
        check(z.phase == 0 && !z.shrinking && z.radius == session.circle.start_radius && std::abs(z.remaining - phases[0].wait) < .01F, "starts waiting at full size");
        Vector2 last_center = z.center;
        float last_radius = z.radius, last_damage = 0;
        int seen_phase = -1;
        bool saw_shrinking = false;
        const float total = [&] { float t = 0; for (const auto& p : phases) t += p.wait + p.shrink; return t; }();
        for (float t = 0; t < total + 5; t += .25F) {
            keep(bots, keeper_id, z.center);
            session.update(.25F, {z.center.x, 0, z.center.y}, health, bots, false);
            z = session.zone();
            if (session.terminal()) break;
            check(z.radius <= last_radius + 1e-3F, "the circle never grows");
            check(std::hypot(z.center.x - last_center.x, z.center.y - last_center.y) <= last_radius - z.radius + .5F, "each circle stays inside the last");
            check(std::hypot(z.next_center.x - z.center.x, z.next_center.y - z.center.y) <= z.radius - z.next_radius + .5F, "the next circle lies inside this one");
            if (z.phase != seen_phase) {
                check(z.damage >= last_damage, "the gas bites harder each phase");
                if (z.phase < static_cast<int>(phases.size())) check(land(z.next_center.x, z.next_center.y), "circles close on land");
                last_damage = z.damage;
                seen_phase = z.phase;
            }
            saw_shrinking |= z.shrinking;
            last_center = z.center;
            last_radius = z.radius;
            health.reset();
        }
        check(saw_shrinking && seen_phase >= static_cast<int>(phases.size()) - 1, "runs through every phase");
        check(session.zone().radius < 1, "closes completely at the end");
        // Different matches, different circles; the same seed, the same circles.
        game::ModeSession a, b, c;
        for (auto* s : {&a, &b, &c}) s->start(game::GameMode::BattleRoyale, 1, game::ai::BotLevel::Easy);
        a.plan_circles(5, land); b.plan_circles(5, land); c.plan_circles(6, land);
        for (auto* s : {&a, &b, &c}) s->update(.1F, {}, health, bots, true);
        const auto end = [&](game::ModeSession& s) { game::ModeSession copy = s; game::ai::BattleRoyaleBots field; game::Health h;
            const int id = field.add_bot({0, 0, 0}, game::combat::WeaponId::Rifle, game::ai::BotLevel::Easy, 1).id();
            for (int i = 0; i < 4000 && !copy.terminal(); ++i) {
                keep(field, id, copy.center());
                copy.update(.25F, {copy.center().x, 0, copy.center().y}, h, field, false); h.reset();
            }
            return copy.center(); };
        const auto ea = end(a), eb = end(b), ec = end(c);
        check(ea.x == eb.x && ea.y == eb.y && (ea.x != ec.x || ea.y != ec.y), "seeded circles");
        // Outside: you are hurt; inside: you are not.
        game::ModeSession hurt;
        hurt.start(game::GameMode::BattleRoyale, 1, game::ai::BotLevel::Easy);
        hurt.plan_circles(9, land);
        game::Health inside, outside;
        bots.add_bot({0, 0, 0}, game::combat::WeaponId::Rifle, game::ai::BotLevel::Easy, 1);
        hurt.update(.25F, {0, 0, 0}, inside, bots, false);
        check(inside.current() == inside.maximum(), "safe inside");
        hurt.update(.25F, {5000, 0, 0}, outside, bots, false);
        check(outside.current() < outside.maximum(), "hurt outside");
    }
    if (failures) return 1;
    std::cout << "[PASS] Battle Royale drop: 8 routes, 3 helicopters, freefall, canopy, bot jumps; zone phases on land\n";
}
