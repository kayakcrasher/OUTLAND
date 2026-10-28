#include "outland/game/sports/BallGame.hpp"
#include "outland/world/Ballparks.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::sports {
namespace {
namespace BP = world::ballpark;

constexpr Vector3 up{0, 1, 0};
// A right-handed batter on the third-base side of the plate, facing it; the catcher's glove; where
// the pitcher lets go; where the bat meets the ball.
constexpr Vector3 batter_box{.95F, 0, -36.1F};
constexpr float batter_yaw = -90;
constexpr Vector3 glove{0, .75F, -37.25F};
constexpr Vector3 release{.35F, 1.9F, -18.5F};
constexpr Vector3 contact{0, .9F, -35.6F};

float clamp01(double v) { return static_cast<float>(std::clamp(v, 0.0, 1.0)); }
float ease(float v) { v = std::clamp(v, 0.0F, 1.0F); return v * v * (3 - 2 * v); }
// 0..1 across [a, b].
float span(double u, double a, double b) { return clamp01((u - a) / (b - a)); }
float yaw_to(Vector3 from, Vector3 to) { return std::atan2(to.x - from.x, to.z - from.z) * RAD2DEG; }
Vector3 base(int k) {
    switch (k) { case 1: return BP::first_base; case 2: return BP::second_base; case 3: return BP::third_base; default: return BP::plate; }
}
// `s` bases round from the plate (0..4).
Vector3 on_path(float s) {
    s = std::clamp(s, 0.0F, 4.0F);
    const int k = std::min(3, static_cast<int>(s));
    return Vector3Lerp(base(k), base(k + 1), s - static_cast<float>(k));
}
Vector3 arc(Vector3 a, Vector3 b, float t, float apex) {
    auto p = Vector3Lerp(a, b, t);
    p.y += 4 * apex * t * (1 - t);
    return p;
}
Vector3 bouncing(Vector3 a, Vector3 b, float t) {
    auto p = Vector3Lerp(a, b, t);
    p.y = .12F + std::abs(std::sin(t * PI * 3)) * .7F * (1 - t);
    return p;
}
Vector3 flat(Vector3 p) { return {p.x, 0, p.z}; }
Vector3 bench_spot(bool home, int k) {
    return BP::along(home, BP::dugout_t0 + .8F + static_cast<float>(k % 10) * 1.15F, BP::bench_d);
}
Vector3 hand(Vector3 feet) { return {feet.x, 1.3F, feet.z}; }

// The bat in a batter's hands: in the stance (swing < 0) up behind the rear shoulder; through a
// swing round from behind, level across the plate, and up over the front shoulder.
Bat bat_for(Vector3 feet, float yaw, float swing) {
    const float a = yaw * DEG2RAD;
    const Vector3 f{std::sin(a), 0, std::cos(a)}, r{-f.z, 0, f.x};
    const float s = std::clamp(swing, 0.0F, 1.0F);
    const float phi = (swing < 0 ? -20.0F : -20.0F + 270.0F * ease(s)) * DEG2RAD;
    const float elevation = (swing < 0 ? 55.0F : s < .45F ? 55.0F * (1 - s / .45F) : 40.0F * (s - .45F) / .55F) * DEG2RAD;
    const Vector3 h = Vector3Add(Vector3Scale(r, std::cos(phi)), Vector3Scale(f, std::sin(phi)));
    Bat bat;
    bat.grip = Vector3Add(Vector3Add(feet, Vector3Scale(up, 1.15F - .15F * std::sin(phi))), Vector3Add(Vector3Scale(h, .25F), Vector3Scale(f, .1F)));
    bat.tip = Vector3Add(bat.grip, Vector3Scale(Vector3Add(Vector3Scale(h, std::cos(elevation)), Vector3Scale(up, std::sin(elevation))), .84F));
    return bat;
}

struct Builder {
    Scene& s;
    Figure& add(Role role, int index, bool home, Vector3 at, float yaw, Motion motion = Motion::Stand, float phase = 0) {
        s.figures.push_back({role, index, home, at, yaw, motion, phase, 0});
        return s.figures.back();
    }
};

void pregame(Scene& s, double t) {
    if (t < -League::pregame) return;
    Builder b{s};
    // Lined up along the lines for the anthem, each club in front of its dugout, facing across.
    for (const bool home : {true, false})
        for (int k = 0; k < 9; ++k)
            b.add(Role::Bench, k, home, BP::along(home, 3.0F + static_cast<float>(k) * 1.4F, -1.2F), BP::facing_field(home));
    b.add(Role::Umpire, 0, false, flat(BP::plate), 0);
}

void postgame(Scene& s, const Fixture& fixture, const Linescore& line, double t) {
    if (t > line.length + League::postgame) return;
    Builder b{s};
    const bool home_won = line.total(true) > line.total(false);
    const float u = static_cast<float>(t - line.length);
    for (const bool home : {true, false}) {
        if (home == home_won) {
            // Piled round the mound, jumping.
            for (int k = 0; k < 9; ++k) {
                const float a = static_cast<float>(k) * 2 * PI / 9;
                const Vector3 at{BP::mound.x + std::sin(a) * 2.2F, 0, BP::mound.z + std::cos(a) * 2.2F};
                b.add(Role::Fielder, k, home, at, yaw_to(at, flat(BP::mound)), Motion::Cheer, std::fmod(u * 3 + static_cast<float>(k) * .13F, 1.0F));
            }
        } else {
            for (int k = 0; k < 9; ++k) b.add(Role::Bench, k, home, bench_spot(home, k), BP::facing_field(home), Motion::Sit);
        }
    }
    (void)fixture;
}

Vector3 spot(int i) { return flat(BP::fielders[static_cast<std::size_t>(i)]); }
float spot_yaw(int i) { return i == 1 ? 0.0F : yaw_to(spot(i), BP::plate); }

// Always there while a half is on: the plate umpire, both benches, the batting side's coaches and
// its man on deck taking practice swings.
void common(Scene& s, bool batting_home, double t) {
    Builder b{s};
    const bool fielding_home = !batting_home;
    b.add(Role::Umpire, 0, false, BP::umpire, 0, Motion::Ready);
    for (int k = 0; k < 3; ++k) b.add(Role::Bench, k, fielding_home, bench_spot(fielding_home, k), BP::facing_field(fielding_home), Motion::Sit);
    for (int k = 3; k < 7; ++k) b.add(Role::Bench, k, batting_home, bench_spot(batting_home, k), BP::facing_field(batting_home), Motion::Sit);
    b.add(Role::Coach, 0, batting_home, BP::first_coach, yaw_to(BP::first_coach, BP::plate));
    b.add(Role::Coach, 1, batting_home, BP::third_coach, yaw_to(BP::third_coach, BP::plate));
    const Vector3 deck = batting_home ? BP::home_on_deck : BP::away_on_deck;
    const float deck_yaw = yaw_to(deck, BP::plate) + 90;
    const float practice = std::fmod(static_cast<float>(t) * .9F, 1.0F);
    b.add(Role::OnDeck, 0, batting_home, deck, deck_yaw, practice < .35F ? Motion::Swing : Motion::Stance, practice / .35F);
}

// The fielders running out from their dugout to their places (u 0..1), the batter walking up.
void take_the_field(Scene& s, bool batting_home, float u) {
    Builder b{s};
    const bool fielding_home = !batting_home;
    for (int i = 0; i < 9; ++i) {
        const Vector3 dugout = bench_spot(fielding_home, i);
        const Vector3 at = Vector3Lerp(dugout, spot(i), u);
        const bool moving = u > 0 && u < 1;
        b.add(Role::Fielder, i, fielding_home, at, moving ? yaw_to(at, spot(i)) : spot_yaw(i), moving ? Motion::Run : (i == 1 ? Motion::Squat : Motion::Ready));
    }
    const Vector3 deck = batting_home ? BP::home_on_deck : BP::away_on_deck;
    b.add(Role::Batter, 0, batting_home, Vector3Lerp(deck, batter_box, ease(u)), batter_yaw, Motion::Stance);
}

// The half over: the fielders in their dugout, the runners left on base back on their bench.
void off_the_field(Scene& s, bool batting_home, std::uint8_t left_on) {
    Builder b{s};
    for (int i = 0; i < 9; ++i) b.add(Role::Fielder, i, !batting_home, bench_spot(!batting_home, i), BP::facing_field(!batting_home), Motion::Sit);
    for (int k = 1; k <= 3; ++k)
        if ((left_on >> (k - 1)) & 1) b.add(Role::Runner, k, batting_home, bench_spot(batting_home, k + 6), BP::facing_field(batting_home), Motion::Sit);
}

// A plate appearance's play, `u` 0..1 through it.
// How a play looks: a double play or an error starts as a grounder like any other, an
// inside-the-park home run as a ball to the wall; where everyone ends up is the play's own.
Play shown(const Play& play) {
    Play p = play;
    if (p.outcome == Outcome::DoublePlay || p.outcome == Outcome::Error) p.outcome = Outcome::Groundout;
    if (p.outcome == Outcome::Single && p.grounder && p.depth < 42) p.outcome = Outcome::Groundout;   // beaten out
    if (p.outcome == Outcome::HomeRun && p.depth < BP::fence_radius) p.outcome = Outcome::Triple;
    return p;
}

void action(Scene& s, const Play& play, double u, bool batting_home, float seconds_per_minute) {
    const Play p = shown(play);
    Builder b{s};
    const bool fielding_home = !batting_home;
    const bool in_play = !(p.outcome == Outcome::Strikeout || p.outcome == Outcome::Walk);
    const auto& adv = p;   // where everyone ends up
    const float a = p.angle * DEG2RAD;
    const Vector3 dir{std::sin(a), 0, std::cos(a)};
    const Vector3 land = Vector3Add(flat(BP::plate), Vector3Scale(dir, p.depth));
    const Vector3 wall = Vector3Add(flat(BP::plate), Vector3Scale(dir, BP::fence_radius - 2.5F));
    // The man the ball goes to after the fielder gets it: first base on a grounder, the cut-off man
    // (short or second) from the outfield, the pitcher on a pop-up.
    const int cutoff = p.outcome == Outcome::Groundout ? 2 : p.fielder >= 6 ? (p.fielder == 8 ? 3 : 4) : 0;
    Vector3 ball{};
    bool ball_shown = false;

    // ---- the fielders
    for (int i = 0; i < 9; ++i) {
        Vector3 at = spot(i);
        float yaw = spot_yaw(i);
        Motion motion = i == 1 ? Motion::Squat : Motion::Ready;
        float phase = 0;
        if (i == 0) {   // the pitcher: the wind-up and the pitch
            if (u >= .08 && u < .42) { motion = Motion::Pitch; phase = span(u, .08, .42); }
            else motion = u < .08 ? Motion::Stand : Motion::Ready;
        }
        if (i == 1 && u > .45 && in_play) motion = Motion::Stand;
        if (in_play && u > Play::contact) {
            if (i == p.fielder) {
                // To the ball: an infielder to the grounder, an outfielder under the fly or after the hit.
                Vector3 to = land;
                double arrive = .50;
                switch (p.outcome) {
                    case Outcome::Flyout: arrive = .62; break;
                    case Outcome::Single: to = Vector3Add(land, Vector3Scale(dir, 4)); arrive = .64; break;
                    case Outcome::Double: case Outcome::Triple: to = wall; arrive = .74; break;
                    case Outcome::HomeRun: to = wall; arrive = .75; break;
                    default: break;
                }
                at = Vector3Lerp(spot(i), to, span(u, Play::contact, arrive));
                yaw = u < arrive ? yaw_to(spot(i), to) : yaw_to(at, BP::plate);
                motion = u < arrive ? Motion::Run : Motion::Ready;
                if (p.outcome == Outcome::Groundout && i == 2) {
                    // The first baseman fields it and runs to the bag himself.
                    if (u > .50) { at = Vector3Lerp(to, flat(BP::first_base), span(u, .50, .66)); motion = u < .66 ? Motion::Run : Motion::Ready; yaw = yaw_to(at, flat(BP::first_base)); }
                    else if (u > .47) motion = Motion::Scoop;
                } else if (p.outcome == Outcome::HomeRun) {
                    if (u >= arrive) { motion = Motion::Stand; yaw = yaw_to(at, Vector3Add(wall, dir)); }
                } else {
                    const double got = p.outcome == Outcome::Groundout ? .50 : p.outcome == Outcome::Flyout ? .62 : p.outcome == Outcome::Single ? .64 : .74;
                    const double throw_at = got + .04;
                    const Vector3 target = cutoff == 2 ? flat(BP::first_base) : spot(cutoff);
                    if (u >= got - .04 && u < throw_at) motion = p.outcome == Outcome::Flyout ? Motion::Catch : Motion::Scoop;
                    else if (u >= throw_at && u < throw_at + .08) { motion = Motion::Throw; phase = span(u, throw_at, throw_at + .08); yaw = yaw_to(at, target); }
                    else if (u >= throw_at + .08) { motion = Motion::Stand; yaw = yaw_to(at, target); }
                }
            } else if (i == cutoff && p.outcome != Outcome::HomeRun) {
                // First base covers the bag; the cut-off man comes out for the relay.
                const Vector3 to = cutoff == 2 ? flat(BP::first_base)
                                 : p.outcome == Outcome::Double || p.outcome == Outcome::Triple ? Vector3Lerp(flat(BP::second_base), wall, .35F)
                                 : cutoff == 0 ? spot(0) : flat(BP::second_base);
                at = Vector3Lerp(spot(i), to, span(u, Play::contact, .6));
                yaw = yaw_to(at, land);
                motion = u < .6 && Vector3Distance(spot(i), to) > .5F ? Motion::Run : Motion::Ready;
                const double caught = p.outcome == Outcome::Groundout ? .64 : p.outcome == Outcome::Flyout || p.outcome == Outcome::Single ? .80 : .90;
                if (u > caught - .03 && u < caught + .05) motion = Motion::Catch;
            } else {
                yaw = yaw_to(at, land);   // everyone else watches the ball
            }
        }
        b.add(Role::Fielder, i, fielding_home, at, yaw, motion, phase);
    }

    // ---- the batter and the runners
    const double go = .42;   // the swing done (or the ball four taken)
    const double home_in = Play::scored;
    // Off to the dugout at a walk once a man is done (he's gone when the next plate appearance starts).
    const auto walk_off = [&](Vector3 from, double since, Vector3& at, float& yaw) {
        const Vector3 bench = bench_spot(batting_home, 7);
        const float seconds = static_cast<float>(std::max(0.0, u - since) * p.action) * seconds_per_minute;
        const float way = Vector3Distance(from, bench);
        at = way > .01F ? Vector3Lerp(from, bench, std::min(1.0F, 1.6F * seconds / way)) : bench;
        yaw = yaw_to(from, bench);
    };
    // A runner from base `from` (0: the batter) to base `to`, then off to the dugout if he scored.
    // Where a man is `s` bases round: off the batter's box at first (he leaves it, not the plate).
    const auto path = [&](float at_s) {
        const Vector3 on = on_path(at_s);
        const float off = 1 - std::clamp(at_s * 4, 0.0F, 1.0F);
        return Vector3Add(on, Vector3Scale(Vector3Subtract(batter_box, flat(BP::plate)), off));
    };
    const float lead_bases = 2.2F / BP::base_path;
    const auto runner = [&](int from, int to, double start, double end, Role role) {
        if (to < 0 && from == 0) return;
        // Thrown out (or forced out): down the line to the next bag, then off the field.
        if (to < 0) {
            Vector3 at;
            float yaw;
            Motion motion;
            const double out_at = start + .25;
            if (u < out_at) {
                const float k = static_cast<float>(span(u, start, out_at));
                at = on_path(static_cast<float>(from) + .9F * k);
                yaw = yaw_to(at, base(from + 1));
                motion = u > start ? Motion::Run : Motion::Stand;
            } else { walk_off(on_path(static_cast<float>(from) + .9F), out_at, at, yaw); motion = Motion::Walk; }
            b.add(role, 4 + from, batting_home, at, yaw, motion);
            return;
        }
        // A lead off the bag till the pitch is in (the batter none).
        const float lead = from == 0 ? 0 : u < Play::contact || to != from ? ease(span(u, .05, .25)) * lead_bases
                                                                          : (1 - ease(span(u, .45, .65))) * lead_bases;
        const float s0 = static_cast<float>(from) + lead;
        Vector3 at = path(s0);
        Motion motion = Motion::Stand;
        float yaw = yaw_to(at, on_path(s0 + .5F));
        int index = from;
        if (to != from && u >= start) {
            const float k = span(u, start, end);
            const float now = s0 + (static_cast<float>(to) - s0) * k;
            at = path(now);
            motion = k < 1 ? Motion::Run : Motion::Stand;
            yaw = yaw_to(at, on_path(std::min(4.0F, now + .2F)));
            if (to == 4 && u > end) { walk_off(flat(BP::plate), end, at, yaw); motion = Motion::Walk; index = 4 + from; }   // in: off he goes
        }
        b.add(role, index, batting_home, at, yaw, motion);
    };
    for (int k = 1; k <= 3; ++k)
        if ((p.bases >> (k - 1)) & 1)
            runner(k, adv.to[static_cast<std::size_t>(k)], p.outcome == Outcome::Flyout ? .64 : .38, home_in, Role::Runner);

    const bool swings = p.outcome != Outcome::Walk;
    const float swing = static_cast<float>(span(u, .33, .42));
    if (u < .42 || (!in_play && p.outcome == Outcome::Strikeout && u < .46)) {
        const bool swinging = swings && u >= .33;
        b.add(Role::Batter, 0, batting_home, batter_box, batter_yaw, swinging ? Motion::Swing : Motion::Stance, swinging ? swing : 0);
    } else {
        if (adv.to[0] >= 1) runner(0, adv.to[0], go, p.outcome == Outcome::HomeRun ? .98 : home_in, Role::Batter);
        else {
            // Out: down the line as far as it takes, then back to the dugout.
            const double turn = p.outcome == Outcome::Groundout ? .66 : p.outcome == Outcome::Flyout ? .62 : .45;
            const float reach = p.outcome == Outcome::Groundout ? .97F : p.outcome == Outcome::Flyout ? .55F : 0;
            Vector3 at;
            float yaw;
            Motion motion;
            if (u < turn) { at = path(reach * span(u, go, turn)); yaw = yaw_to(flat(BP::plate), flat(BP::first_base)); motion = Motion::Run; }
            else { walk_off(path(reach), turn + .02, at, yaw); motion = Motion::Walk; }
            b.add(Role::Batter, motion == Motion::Walk ? 1 : 0, batting_home, at, yaw, motion);   // (1: done, walking off)
        }
    }

    // ---- the ball
    if (u >= Play::release && u < Play::contact) {
        ball = arc(release, {0, .85F, -36.2F}, span(u, Play::release, Play::contact), .25F);
        ball_shown = true;
    } else if (u >= Play::contact && !in_play) {
        // In the mitt, then lobbed back to the pitcher.
        if (u < .385) ball = Vector3Lerp({0, .85F, -36.2F}, glove, span(u, Play::contact, .385));
        else if (u < .55) ball = glove;
        else ball = arc(glove, {BP::mound.x, 1.4F, BP::mound.z - .4F}, span(u, .55, .63), 2.0F);
        ball_shown = u < .63;
    } else if (u >= Play::contact) {
        ball_shown = true;
        const auto in_glove = [&](int who) {
            for (const auto& f : s.figures) if (f.role == Role::Fielder && f.index == who) return Vector3Add(f.at, {0, 1.0F, 0});
            return land;
        };
        const Vector3 target_hand = cutoff == 2 ? Vector3{BP::first_base.x, 1.3F, BP::first_base.z} : hand(spot(cutoff));
        switch (p.outcome) {
            case Outcome::Groundout: {
                const Vector3 to{land.x, .3F, land.z};
                if (u < .50) ball = bouncing(contact, to, span(u, Play::contact, .50));
                else if (p.fielder == 2) ball = in_glove(2);
                else if (u < .57) ball = in_glove(p.fielder);
                else if (u < .65) ball = arc(hand(land), target_hand, span(u, .57, .65), 1.2F);
                else ball = in_glove(2);
                ball_shown = u < .92;
                break;
            }
            case Outcome::Flyout: {
                const float apex = p.apex > 1 ? p.apex : p.depth > 40 ? 26.0F : 16.0F;
                if (u < .62) ball = arc(contact, {land.x, 1.8F, land.z}, span(u, Play::contact, .62), apex);
                else if (u < .70) ball = in_glove(p.fielder);
                else if (u < .80) ball = arc(hand(land), target_hand, span(u, .70, .80), 3.0F);
                else ball = in_glove(cutoff);
                ball_shown = u < .92;
                break;
            }
            case Outcome::Single: {
                const Vector3 rolled = Vector3Add(land, Vector3Scale(dir, 4));
                if (u < .52) ball = arc(contact, {land.x, .3F, land.z}, span(u, Play::contact, .52), 2.5F);
                else if (u < .64) ball = Vector3Lerp({land.x, .15F, land.z}, {rolled.x, .12F, rolled.z}, span(u, .52, .64));
                else if (u < .70) ball = in_glove(p.fielder);
                else if (u < .80) ball = arc(hand(rolled), target_hand, span(u, .70, .80), 2.5F);
                else ball = in_glove(cutoff);
                ball_shown = u < .95;
                break;
            }
            case Outcome::Double: case Outcome::Triple: {
                if (u < .55) ball = arc(contact, {land.x, 1.0F, land.z}, span(u, Play::contact, .55), 9.0F);
                else if (u < .62) ball = bouncing({land.x, 1.0F, land.z}, {wall.x, .2F, wall.z}, span(u, .55, .62));
                else if (u < .74) ball = {wall.x, .15F, wall.z};
                else if (u < .80) ball = in_glove(p.fielder);
                else if (u < .90) ball = arc(hand(wall), target_hand, span(u, .80, .90), 4.0F);
                else ball = in_glove(cutoff);
                ball_shown = u < .95;
                break;
            }
            case Outcome::HomeRun:
                ball = arc(contact, {land.x, 4.0F, land.z}, span(u, Play::contact, .62), 30.0F);
                ball_shown = u < .62;
                break;
            default: break;
        }
    }
    s.ball = ball_shown;
    s.ball_at = ball;

    // ---- the tracer: where the batted ball went, from the bat to where it came down (or was caught)
    if (in_play && u >= Play::contact) {
        double end = 0;
        const auto flight = [&](double v) -> Vector3 {
            switch (p.outcome) {
                case Outcome::Groundout: return bouncing(contact, {land.x, .3F, land.z}, span(v, Play::contact, .50));
                case Outcome::Flyout:
                    return arc(contact, {land.x, 1.8F, land.z}, span(v, Play::contact, .62), p.apex > 1 ? p.apex : p.depth > 40 ? 26.0F : 16.0F);
                case Outcome::Single: {
                    const Vector3 rolled = Vector3Add(land, Vector3Scale(dir, 4));
                    if (v < .52) return arc(contact, {land.x, .3F, land.z}, span(v, Play::contact, .52), 2.5F);
                    return Vector3Lerp({land.x, .15F, land.z}, {rolled.x, .12F, rolled.z}, span(v, .52, .64));
                }
                case Outcome::Double: case Outcome::Triple:
                    if (v < .55) return arc(contact, {land.x, 1.0F, land.z}, span(v, Play::contact, .55), 9.0F);
                    return bouncing({land.x, 1.0F, land.z}, {wall.x, .2F, wall.z}, span(v, .55, .62));
                case Outcome::HomeRun: return arc(contact, {land.x, 4.0F, land.z}, span(v, Play::contact, .62), 30.0F);
                default: return contact;
            }
        };
        switch (p.outcome) {
            case Outcome::Groundout: end = .50; break;
            case Outcome::Single: end = .64; break;
            case Outcome::Flyout: case Outcome::Double: case Outcome::Triple: case Outcome::HomeRun: end = .62; break;
            default: break;
        }
        if (end > Play::contact) {
            constexpr int samples = 32;
            const double upto = std::min(u, end);
            for (int k = 0; k <= samples; ++k) s.trail.push_back(flight(Play::contact + (upto - Play::contact) * k / samples));
        }
    }
}

// Where someone in `from` ends up in `to`: the same man (a runner by the base he reached; the man
// on deck is the next batter).
int partner(const Scene& from, const Figure& f, const std::vector<int>& reached) {
    for (int i = 0; i < static_cast<int>(from.figures.size()); ++i) {
        const auto& g = from.figures[static_cast<std::size_t>(i)];
        if (g.home != f.home && f.role != Role::Umpire) continue;
        if (f.role == Role::Runner) { if (reached[static_cast<std::size_t>(i)] == f.index) return i; continue; }
        if (f.role == Role::Batter) { if (g.role == Role::OnDeck) return i; continue; }
        if (g.role == f.role && g.index == f.index) return i;
    }
    return -1;
}

// After a play: everyone from where it left them to where they're wanted next (`to`), at a jog -
// `seconds` after the play ended. Those with nowhere to be (a run scored, the batter out) carry on
// to the dugout at a walk.
Scene regroup(const Scene& from, const std::vector<int>& reached, const Scene& to, float seconds, bool batting_home) {
    Scene s;
    std::vector<unsigned char> used(from.figures.size());
    for (const auto& f : to.figures) {
        Figure g = f;
        const int i = partner(from, f, reached);
        if (i >= 0) {
            used[static_cast<std::size_t>(i)] = 1;
            const Vector3 a = from.figures[static_cast<std::size_t>(i)].at;
            const float way = Vector3Distance(a, f.at);
            const float k = way > .05F ? std::min(1.0F, 6.5F * seconds / way) : 1.0F;
            g.at = Vector3Lerp(a, f.at, k);
            if (k < 1) { g.motion = Motion::Run; g.yaw = yaw_to(a, f.at); g.phase = 0; }
        }
        s.figures.push_back(g);
    }
    for (std::size_t i = 0; i < from.figures.size(); ++i) {
        if (used[i]) continue;
        Figure g = from.figures[i];
        if (g.role != Role::Batter && g.role != Role::Runner) continue;
        const Vector3 bench = bench_spot(batting_home, 7);
        const float way = Vector3Distance(g.at, bench);
        if (way < .3F) continue;
        g.at = Vector3Lerp(g.at, bench, std::min(1.0F, 1.6F * seconds / way));
        g.motion = Motion::Walk;
        g.yaw = yaw_to(g.at, bench);
        s.figures.push_back(g);
    }
    return s;
}

// The base each runner (and the batter) in a play's scene reached, -1 none.
std::vector<int> reached_bases(const Scene& sc, const Play& p) {
    const auto& a = p;
    std::vector<int> out(sc.figures.size(), -1);
    for (std::size_t i = 0; i < sc.figures.size(); ++i) {
        const auto& f = sc.figures[i];
        if (f.role == Role::Runner && f.index >= 1 && f.index <= 3) out[i] = a.to[static_cast<std::size_t>(f.index)];
        if (f.role == Role::Batter) out[i] = a.to[0];
        if (out[i] == 4) out[i] = -1;
    }
    return out;
}

// The bats: in the batter's and the man on deck's hands (a dropped one by the plate).
void bats(Scene& s, const Play* p, double u) {
    for (const auto& f : s.figures) {
        if (f.role == Role::OnDeck) s.bats.push_back(bat_for(f.at, f.yaw, f.motion == Motion::Swing ? f.phase : -1));
        if (f.role == Role::Batter && (f.motion == Motion::Stance || f.motion == Motion::Swing))
            s.bats.push_back(bat_for(f.at, f.yaw, f.motion == Motion::Swing ? f.phase : -1));
    }
    if (p && u > .42 && u <= 1 && p->outcome != Outcome::Strikeout) s.bats.push_back({{1.0F, .04F, -36.5F}, {1.75F, .04F, -35.9F}});
}

Scene raw_scene(const Fixture& fixture, const Linescore& line, double t, float seconds_per_minute) {
    Scene s;
    if (line.plays.empty()) return s;
    if (t < 0) { pregame(s, t); return s; }
    if (t >= line.length) { postgame(s, fixture, line, t); return s; }
    const int half = std::max(0, line.half_at(t));
    const int inning = half / 2 + 1;
    const bool batting_home = half % 2 == 1;
    const double half_start = line.halves[static_cast<std::size_t>(half)];
    int first = -1, last = -1, current = -1;
    for (int i = 0; i < static_cast<int>(line.plays.size()); ++i) {
        const auto& p = line.plays[static_cast<std::size_t>(i)];
        if (p.inning != inning || p.home != batting_home) continue;
        if (first < 0) first = i;
        last = i;
        if (t >= p.at) current = i;
    }
    // Before the first pitch of the half: the change-over.
    if (current < 0) {
        common(s, batting_home, t);
        take_the_field(s, batting_home, span(t, half_start, half_start + League::change_over * .85));
        bats(s, nullptr, 0);
        return s;
    }
    const auto& p = line.plays[static_cast<std::size_t>(current)];
    const double u = (t - p.at) / std::max(.01, p.action);
    if (u < 1) {
        s.play = current;
        common(s, batting_home, t);
        action(s, p, u, batting_home, seconds_per_minute);
        bats(s, &p, u);
        return s;
    }
    // The play over: back to places for the next pitch, or (the half over) off the field.
    Scene ended;
    common(ended, batting_home, t);
    action(ended, p, .999, batting_home, seconds_per_minute);
    const auto reached = reached_bases(ended, p);
    Scene next;
    common(next, batting_home, t);
    if (current < last) action(next, line.plays[static_cast<std::size_t>(current) + 1], 0, batting_home, seconds_per_minute);
    else off_the_field(next, batting_home, p.after);
    const float since = static_cast<float>((t - p.at - p.action) * seconds_per_minute);
    s = regroup(ended, reached, next, since, batting_home);
    // The tracer lingers a few seconds after the play, fading.
    constexpr float linger = 6;
    if (since < linger) { s.trail = ended.trail; s.trail_fade = 1 - since / linger; }
    if (current < last) s.play = current;
    bats(s, nullptr, 0);
    (void)first;
    return s;
}
}

Scene scene(const Fixture& fixture, const Linescore& line, double t, float seconds_per_minute) {
    // How fast everyone is going: where they'll be a moment later.
    constexpr double step = .02;
    Scene now = raw_scene(fixture, line, t, seconds_per_minute);
    const Scene next = raw_scene(fixture, line, t + step, seconds_per_minute);
    // (Runners are known by the base they're on, and the batter becomes one: match them by where
    // they are. Someone who isn't there a moment later - off the field, or a new man - stands.)
    for (auto& f : now.figures) {
        const bool by_place = f.role == Role::Runner || f.role == Role::Batter;
        float best = 1e9F;
        for (const auto& h : next.figures) {
            if (h.home != f.home) continue;
            const bool match = by_place ? (h.role == Role::Runner || h.role == Role::Batter) : (h.role == f.role && h.index == f.index);
            if (match) best = std::min(best, Vector3Distance(flat(f.at), flat(h.at)));
        }
        const float speed = best / static_cast<float>(step * std::max(.1F, seconds_per_minute));
        f.speed = speed < 25 ? speed : 0;
        if ((f.motion == Motion::Run || f.motion == Motion::Walk) && f.speed < .25F) f.motion = Motion::Stand;
    }
    return now;
}

bool crack(const Linescore& line, double t0, double t1) {
    for (const auto& p : line.plays) {
        if (p.outcome == Outcome::Strikeout || p.outcome == Outcome::Walk) continue;
        const double at = p.at + p.length * Play::contact;
        if (at > t0 && at <= t1) return true;
        if (p.at > t1) break;
    }
    return false;
}

} // namespace outland::game::sports
