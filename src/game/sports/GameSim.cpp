#include "outland/game/sports/GameSim.hpp"
#include "outland/world/Ballparks.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace outland::game::sports {
namespace {
namespace BP = world::ballpark;

struct Rng {
    std::uint64_t s;
    float next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return static_cast<float>(s >> 40) / static_cast<float>(1ULL << 24);
    }
    float normal(float mean, float spread) {
        const float u = std::max(1e-6F, next()), v = next();
        return mean + spread * std::sqrt(-2 * std::log(u)) * std::cos(2 * PI * v);
    }
};
float clamp01(float v) { return std::clamp(v, 0.0F, 1.0F); }

// The field, from above (the park's frame: the plate at z -36, centre field +z).
struct P2 { float x, z; };
float dist(P2 a, P2 b) { return std::hypot(a.x - b.x, a.z - b.z); }
// `d` metres out from the plate, `spray` degrees off the line to second (+ toward third).
P2 out_from_plate(float d, float spray) { const float a = spray * DEG2RAD; return {std::sin(a) * d, BP::plate.z + std::cos(a) * d}; }
P2 spot(int f) { return {BP::fielders[static_cast<std::size_t>(f)].x, BP::fielders[static_cast<std::size_t>(f)].z}; }
P2 bag(int b) {
    switch (b) {
        case 1: return {BP::first_base.x, BP::first_base.z};
        case 2: return {BP::second_base.x, BP::second_base.z};
        case 3: return {BP::third_base.x, BP::third_base.z};
        default: return {BP::plate.x, BP::plate.z};
    }
}
constexpr float base_path = BP::base_path;

float run_speed(const Player& p) { return 6.8F + 1.8F * p.speed; }                       // m/s flat out (a runner)
// A fielder going after a ball: slower than flat out (the first steps, the angle, the glove down),
// an infielder sideways slower still.
float range_speed(const Player& p, bool outfield) { return outfield ? 4.6F + 1.4F * p.speed : 4.6F + 1.4F * p.speed; }
float throw_speed(const Player& p, bool outfield) { return (outfield ? 25.0F : 22.0F) + 8.0F * p.arm; }   // a high-school arm
constexpr float transfer = .9F;   // glove to throwing hand to the throw

// A ball in the air: launched at `ev` m/s, `la` degrees up from a metre off the ground, slowed by
// the air. How far it carries, how long it's up, how high it goes, and how high it is as it
// crosses the fence (-1: it comes down short).
struct Flight { float distance{0}, hang{0}, apex{1}, at_fence{-1}; };
Flight fly(float ev, float la) {
    constexpr float drag = .0053F, g = 9.81F, dt = .02F;
    float vx = ev * std::cos(la * DEG2RAD), vy = ev * std::sin(la * DEG2RAD), x = 0, y = 1.0F, t = 0;
    Flight f;
    while (t < 14) {
        const float v = std::hypot(vx, vy);
        vx -= drag * v * vx * dt;
        vy -= (g + drag * v * vy) * dt;
        const float was = x;
        x += vx * dt; y += vy * dt; t += dt;
        f.apex = std::max(f.apex, y);
        if (was < BP::fence_radius && x >= BP::fence_radius && f.at_fence < 0) f.at_fence = std::max(0.0F, y);
        if (y <= 0) break;
    }
    f.distance = x; f.hang = t;
    return f;
}

struct Side {
    const Roster& roster;
    int order{0};           // who's up
    int pitcher{0};         // Roster::staff
    int pitches{0};         // thrown by the man on the mound
};

// The man at a fielding spot (the pitcher fields as himself: his arm is his fastball).
Player fielder(const Side& side, int f) {
    if (f == 0) {
        const auto& p = side.roster.staff[static_cast<std::size_t>(side.pitcher)];
        Player man; man.name = p.name; man.arm = p.velocity; man.glove = .5F; man.speed = .4F;
        return man;
    }
    return side.roster.lineup[static_cast<std::size_t>(side.roster.position[static_cast<std::size_t>(f)])];
}

// The runners: who's on (lineup slots, -1 empty), base by base.
struct Bases {
    std::array<int, 4> on{{-1, -1, -1, -1}};   // [1..3]
    [[nodiscard]] std::uint8_t mask() const {
        return static_cast<std::uint8_t>((on[1] >= 0 ? 1 : 0) | (on[2] >= 0 ? 2 : 0) | (on[3] >= 0 ? 4 : 0));
    }
};

// Everybody runs: the ball's back in at `back` seconds, in a fielder's hand at `from`, who throws
// at `arm` m/s. The runners (and the batter, `batter_min` bases at least) take what they safely can
// - and now and then try for one more and get thrown out. `go[k]`: when each starts.
void race(Rng& rng, const Side& batting, const Bases& bases, int batter, int batter_min, P2 from, float back, float arm,
          const std::array<float, 4>& go, Play& p) {
    int ahead = 5;   // the base the man in front ended on (5: nobody in front)
    for (int k = 3; k >= 0; --k) {
        const int who = k == 0 ? batter : bases.on[static_cast<std::size_t>(k)];
        if (who < 0) continue;
        const float speed = run_speed(batting.roster.lineup[static_cast<std::size_t>(who)]);
        // Forced along: every base behind him full (the batter's always forced).
        bool forced = true;
        for (int j = 1; j < k; ++j) forced = forced && bases.on[static_cast<std::size_t>(j)] >= 0;
        const int least = k == 0 ? batter_min : forced ? k + 1 : k;
        const int cap = ahead == 5 || ahead == 4 ? 4 : ahead - 1;
        int dest = std::min(least, cap);
        bool out = false;
        for (int m = std::max(k + 1, dest + 1); m <= cap; ++m) {
            const float run = go[static_cast<std::size_t>(k)] + base_path * static_cast<float>(m - k) / speed + .3F * static_cast<float>(m - k - 1);
            const float ball = back + .4F + dist(from, bag(m)) / arm + (dist(from, bag(m)) > 50 ? .8F : 0);   // a long one needs the relay
            const float margin = ball - run;
            if (margin > .3F) { dest = m; continue; }
            if (margin > -.15F && rng.next() < .35F) {   // waved round
                if (margin + rng.normal(0, .3F) > 0) dest = m;
                else out = true;
            }
            break;
        }
        if (out) { p.to[static_cast<std::size_t>(k)] = -1; ++p.outs_made; ahead = std::min(ahead, dest + 1); continue; }
        p.to[static_cast<std::size_t>(k)] = dest;
        ahead = dest;
    }
}

// A ball put in play (or fouled away): where it goes, who gets to it, how it ends. Returns false
// for a foul ball that nobody catches (the plate appearance goes on).
bool in_play(Rng& rng, Side& batting, Side& fielding, const Bases& bases, int outs, float quality, float power, Play& p) {
    const int batter = batting.order;
    const Player& hitter = batting.roster.lineup[static_cast<std::size_t>(batter)];
    const float ev = 21.0F + 17.0F * quality + 6.0F * power * quality;                // m/s off the bat (~70 mph on average, 98 at best)
    const float la = rng.normal(8.0F + 14.0F * power * quality, 17.0F);                 // degrees
    const float spray = rng.normal(5.0F + 10.0F * (quality - .5F), 21.0F);              // a right-hander pulls
    p.angle = spray;

    if (std::abs(spray) > 45) {
        // Foul: a pop-up the catcher or a corner man runs under, or into the seats.
        if (la < 30) return false;
        const auto f = fly(ev, la);
        const P2 lands = out_from_plate(f.distance, spray);
        if (f.distance > 40) return false;
        for (const int who : {1, 2, 5}) {
            const float need = .4F + std::max(0.0F, dist(spot(who), lands) - 1.2F) / range_speed(fielder(fielding, who), false);
            if (need < f.hang - .2F && rng.next() < .9F) {
                p.outcome = Outcome::Flyout; p.fielder = who; p.depth = f.distance; p.apex = f.apex; p.hang = f.hang;
                p.outs_made = 1;
                for (int k = 1; k <= 3; ++k) if (bases.on[static_cast<std::size_t>(k)] >= 0) p.to[static_cast<std::size_t>(k)] = k;
                return true;
            }
        }
        return false;
    }

    for (int k = 1; k <= 3; ++k) if (bases.on[static_cast<std::size_t>(k)] >= 0) p.to[static_cast<std::size_t>(k)] = k;
    const float batter_speed = run_speed(hitter);
    const float to_first = 1.0F + base_path / batter_speed;

    if (la < 8) {
        // ---- On the ground: skipping out, slowing; the first fielder who can cut it off has it.
        p.grounder = true;
        const float v0 = ev * std::cos(la * DEG2RAD) * .78F, slow = 5.5F;
        const float stop_t = v0 / slow, stop_d = v0 * v0 / (2 * slow);
        int got = -1;
        float t_got = 0, d_got = 0;
        bool through = false;
        for (float t = 0; t <= stop_t + .02F; t += .04F) {
            const float tt = std::min(t, stop_t);
            const float d = v0 * tt - .5F * slow * tt * tt;
            if (d > 44) { through = true; break; }
            const P2 at = out_from_plate(d, spray);
            for (const int f : {1, 0, 5, 4, 3, 2}) {
                if (f == 1 && d > 7) continue;
                const float need = .35F + std::max(0.0F, dist(spot(f), at) - 1.0F) / range_speed(fielder(fielding, f), false);
                if (need <= t) { got = f; t_got = t; d_got = d; break; }
            }
            if (got >= 0) break;
        }
        if (!through && got < 0) {
            // Died in the infield grass: whoever's quickest to it.
            const P2 at = out_from_plate(stop_d, spray);
            float best = 1e9F;
            for (const int f : {0, 1, 2, 3, 4, 5}) {
                const float need = .35F + dist(spot(f), at) / range_speed(fielder(fielding, f), false);
                if (need < best) { best = need; got = f; }
            }
            t_got = std::max(stop_t, best); d_got = stop_d;
        }
        if (!through) {
            const P2 at = out_from_plate(d_got, spray);
            const Player man = fielder(fielding, got);
            p.fielder = got; p.depth = d_got;
            // A boot: everybody moves up one.
            if (rng.next() < .045F * (1.25F - man.glove)) {
                p.outcome = Outcome::Error;
                p.to[0] = 1;
                for (int k = 1; k <= 3; ++k)
                    if (bases.on[static_cast<std::size_t>(k)] >= 0) p.to[static_cast<std::size_t>(k)] = std::min(4, k + 1);
                return true;
            }
            const float arm = throw_speed(man, false);
            // Two: the lead runner forced at second, then the relay to first.
            if (bases.on[1] >= 0 && outs < 2) {
                const float runner = .1F + (base_path - 3.0F) / run_speed(batting.roster.lineup[static_cast<std::size_t>(bases.on[1])]);   // off his lead
                const float at_second = t_got + (got == 3 || got == 4 ? .6F : transfer) + dist(at, bag(2)) / arm;
                if (at_second + .1F < runner) {
                    const int pivot = got == 4 || got == 5 ? 3 : 4;
                    const float at_first = at_second + 1.1F + base_path / throw_speed(fielder(fielding, pivot), false);   // the turn
                    p.to[1] = -1;
                    for (int k = 2; k <= 3; ++k)   // the others move up when forced (or hold)
                        if (bases.on[static_cast<std::size_t>(k)] >= 0) {
                            bool forced = true;
                            for (int j = 1; j < k; ++j) forced = forced && bases.on[static_cast<std::size_t>(j)] >= 0;
                            p.to[static_cast<std::size_t>(k)] = forced ? k + 1 : k;
                        }
                    if (at_first < to_first) { p.outcome = Outcome::DoublePlay; p.to[0] = -1; p.outs_made = 2; }
                    else { p.outcome = Outcome::Groundout; p.to[0] = 1; p.outs_made = 1; }   // the fielder's choice
                    if (outs + p.outs_made >= 3) for (int k = 1; k <= 3; ++k) if (p.to[static_cast<std::size_t>(k)] == 4) p.to[static_cast<std::size_t>(k)] = 3;
                    return true;
                }
            }
            const float at_first = got == 2 && dist(at, bag(1)) < 7 ? t_got + .3F + dist(at, bag(1)) / run_speed(man)
                                                                    : t_got + transfer + dist(at, bag(1)) / arm;
            if (to_first < at_first) {
                // Beat it out: an infield single, the forced runners up one.
                p.outcome = Outcome::Single; p.to[0] = 1;
                for (int k = 1; k <= 3; ++k)
                    if (bases.on[static_cast<std::size_t>(k)] >= 0) {
                        bool forced = true;
                        for (int j = 1; j < k; ++j) forced = forced && bases.on[static_cast<std::size_t>(j)] >= 0;
                        p.to[static_cast<std::size_t>(k)] = forced ? k + 1 : k;
                    }
                return true;
            }
            // Out at first; with fewer than two out the runners move up behind the play.
            p.outcome = Outcome::Groundout; p.to[0] = -1; p.outs_made = 1;
            if (outs < 2) {
                if (bases.on[1] >= 0) p.to[1] = 2;
                if (bases.on[2] >= 0) p.to[2] = spray < 0 || t_got > 1.8F || bases.on[1] >= 0 ? 3 : 2;
                if (bases.on[3] >= 0) {
                    const float runner = .3F + base_path / run_speed(batting.roster.lineup[static_cast<std::size_t>(bases.on[3])]);
                    const float home_throw = t_got + transfer + dist(at, bag(4)) / arm;
                    p.to[3] = runner + .3F < home_throw || (bases.on[2] >= 0 && p.to[2] == 3) ? 4 : 3;
                }
            }
            return true;
        }
        // Through the infield: the outfielder cuts it off.
        float best = 1e9F;
        int who = 7;
        P2 where{};
        for (const int f : {6, 7, 8}) {
            for (float t = 0; t <= stop_t + .02F; t += .05F) {
                const float tt = std::min(t, stop_t);
                const P2 at = out_from_plate(v0 * tt - .5F * slow * tt * tt, spray);
                const float need = .5F + std::max(0.0F, dist(spot(f), at) - 1.0F) / range_speed(fielder(fielding, f), true);
                if (need <= t || t >= stop_t) { if (std::max(t, need) < best) { best = std::max(t, need); who = f; where = at; } break; }
            }
        }
        p.fielder = who; p.depth = dist(where, bag(4));
        const std::array<float, 4> go{{0, outs == 2 ? 0 : .2F, outs == 2 ? 0 : .2F, outs == 2 ? 0 : .2F}};
        race(rng, batting, bases, batter, 1, where, best + .8F, throw_speed(fielder(fielding, who), true), {{1.0F, go[1], go[2], go[3]}}, p);
    } else {
        // ---- In the air.
        const auto f = fly(ev, la);
        p.apex = f.apex; p.hang = f.hang; p.depth = f.distance;
        if (f.at_fence > 2.6F) {
            // Gone: everybody trots home.
            p.outcome = Outcome::HomeRun; p.to[0] = 4;
            for (int k = 1; k <= 3; ++k) if (bases.on[static_cast<std::size_t>(k)] >= 0) p.to[static_cast<std::size_t>(k)] = 4;
            p.fielder = spray > 15 ? 6 : spray < -15 ? 8 : 7;
            return true;
        }
        const bool off_the_wall = f.at_fence >= 0;
        const float carry = off_the_wall ? BP::fence_radius - .5F : f.distance;
        const P2 lands = out_from_plate(carry, spray);
        // Who gets under it, and does he hold on.
        int who = -1;
        float margin = -1e9F;
        for (int k = 0; k < 9; ++k) {
            if ((k == 1 && carry > 25) || (k == 0 && carry > 30)) continue;
            const float need = (k >= 6 ? .7F : .4F) + std::max(0.0F, dist(spot(k), lands) - 1.3F) / range_speed(fielder(fielding, k), k >= 6);
            if (f.hang - need > margin) { margin = f.hang - need; who = k; }
        }
        const Player man = fielder(fielding, who);
        bool caught = false;
        if (!off_the_wall) {
            if (margin > .25F) caught = rng.next() > .015F * (1.3F - man.glove);
            else if (margin > -.3F) caught = rng.next() < .3F + margin;   // the dive
        }
        p.fielder = who;
        if (caught) {
            p.outcome = Outcome::Flyout; p.to[0] = -1; p.outs_made = 1;
            if (outs < 2) {
                // Tagging up: third (and second, on a deep one) against the throw.
                const float arm = throw_speed(man, who >= 6);
                for (const int k : {3, 2}) {
                    const int who_on = bases.on[static_cast<std::size_t>(k)];
                    if (who_on < 0 || (k == 2 && carry < 60) || (k == 2 && p.to[3] == 3)) continue;
                    const float run = .3F + base_path / run_speed(batting.roster.lineup[static_cast<std::size_t>(who_on)]);
                    const float ball = .5F + dist(lands, bag(k + 1)) / arm;
                    const float edge = ball - run;
                    if (edge > .2F) p.to[static_cast<std::size_t>(k)] = k + 1;
                    else if (edge > -.2F && rng.next() < .4F) {
                        if (edge + rng.normal(0, .25F) > 0) p.to[static_cast<std::size_t>(k)] = k + 1;
                        else { p.to[static_cast<std::size_t>(k)] = -1; ++p.outs_made; }
                    }
                }
            }
            return true;
        }
        // Down: off the wall or onto the grass, rolling on; the nearest man runs it down.
        const float roll = off_the_wall ? 3.0F : std::clamp(.22F * ev * std::cos(la * DEG2RAD), 2.0F, 18.0F);
        const float reach = std::min(carry + roll, BP::fence_radius - 1.5F);
        const P2 rests = out_from_plate(reach, spray);
        const float ball_there = f.hang + roll / std::max(4.0F, .5F * ev * std::cos(la * DEG2RAD));
        float best = 1e9F;
        for (int k = 2; k < 9; ++k) {
            if (reach > 45 && k < 6) continue;
            const float need = (k >= 6 ? .5F : .4F) + dist(spot(k), rests) / range_speed(fielder(fielding, k), k >= 6);
            const float has = std::max(ball_there, need);
            if (has < best) { best = has; who = k; }
        }
        if (off_the_wall || reach > BP::fence_radius - 6) best += .8F;   // dug out of the corner, off the wall
        p.fielder = who; p.depth = reach;
        // Runners hold up to see it drop (not with two out).
        const float wait = outs == 2 ? 0 : std::min(f.hang * .5F, 2.0F);
        race(rng, batting, bases, batter, 1, rests, best + .8F, throw_speed(fielder(fielding, who), who >= 6), {{1.0F, wait, wait, wait}}, p);
    }
    switch (p.to[0]) {
        case 2: p.outcome = Outcome::Double; break;
        case 3: p.outcome = Outcome::Triple; break;
        case 4: p.outcome = Outcome::HomeRun; break;   // inside the park
        default: p.outcome = Outcome::Single; break;
    }
    if (p.to[0] < 0) p.outcome = Outcome::Groundout;
    return true;
}

// One man's turn at the plate, pitch by pitch.
Play plate_appearance(Rng& rng, Side& batting, Side& fielding, Bases& bases, int outs) {
    // The starter tires; the bullpen comes in.
    {
        const auto& now = fielding.roster.staff[static_cast<std::size_t>(fielding.pitcher)];
        if (fielding.pitcher < 2 && static_cast<float>(fielding.pitches) > 55.0F + 45.0F * now.stamina) { ++fielding.pitcher; fielding.pitches = 0; }
    }
    const Player& hitter = batting.roster.lineup[static_cast<std::size_t>(batting.order)];
    const Pitcher& arm = fielding.roster.staff[static_cast<std::size_t>(fielding.pitcher)];
    Play p;
    p.batter = batting.order; p.outs = outs; p.bases = bases.mask(); p.pitcher = fielding.pitcher;
    p.fielder = 1;
    int balls = 0, strikes = 0, pitches = 0;
    bool done = false;
    while (!done) {
        ++pitches; ++fielding.pitches;
        const float limit = 55.0F + 45.0F * arm.stamina;
        const float tired = clamp01((static_cast<float>(fielding.pitches) - .75F * limit) / (.4F * limit));
        const float control = arm.control * (1 - .35F * tired);
        const float stuff = (arm.stuff * .7F + arm.velocity * .3F) * (1 - .3F * tired);
        const bool zone = rng.next() < .34F + .26F * control + (balls == 3 ? .12F : 0) - (strikes == 2 && balls < 2 ? .08F : 0);
        float swing = zone ? .58F + .14F * hitter.contact + (strikes == 2 ? .14F : 0) : .34F - .24F * hitter.eye + (strikes == 2 ? .12F : 0);
        if (balls == 3 && strikes == 0) swing *= .35F;
        if (rng.next() >= swing) {
            if (zone) ++strikes; else ++balls;
        } else {
            const float contact = zone ? .80F + .15F * hitter.contact - .18F * stuff : .52F + .2F * hitter.contact - .24F * stuff;
            if (rng.next() >= contact) ++strikes;
            else if (rng.next() < .40F) { if (strikes < 2) ++strikes; }      // fouled back
            else {
                const float quality = clamp01(rng.normal(.47F + .3F * (hitter.contact - .5F) - .28F * (stuff - .5F) + (zone ? .04F : -.12F), .2F));
                Play hit = p;
                if (in_play(rng, batting, fielding, bases, outs, quality, hitter.power, hit)) { p = hit; done = true; }
                else if (strikes < 2) ++strikes;                                  // foul ball
            }
        }
        if (!done && strikes >= 3) {
            p.outcome = Outcome::Strikeout; p.to[0] = -1; p.outs_made = 1;
            for (int k = 1; k <= 3; ++k) if (bases.on[static_cast<std::size_t>(k)] >= 0) p.to[static_cast<std::size_t>(k)] = k;
            done = true;
        }
        if (!done && balls >= 4) {
            p.outcome = Outcome::Walk; p.to[0] = 1;
            for (int k = 1; k <= 3; ++k)
                if (bases.on[static_cast<std::size_t>(k)] >= 0) {
                    bool forced = true;
                    for (int j = 1; j < k; ++j) forced = forced && bases.on[static_cast<std::size_t>(j)] >= 0;
                    p.to[static_cast<std::size_t>(k)] = forced ? k + 1 : k;
                }
            done = true;
        }
        if (!done && pitches >= 24) {   // an endless string of fouls: call it a strikeout
            p.outcome = Outcome::Strikeout; p.to[0] = -1; p.outs_made = 1;
            for (int k = 1; k <= 3; ++k) if (bases.on[static_cast<std::size_t>(k)] >= 0) p.to[static_cast<std::size_t>(k)] = k;
            done = true;
        }
    }
    p.balls = std::min(balls, 3); p.strikes = std::min(strikes, 2); p.pitches = pitches;
    // The third out ends it: nobody scores behind a force or the batter out at first.
    if (outs + p.outs_made >= 3 && (p.to[0] < 0 || p.outcome == Outcome::DoublePlay || p.outcome == Outcome::Groundout))
        for (int k = 1; k <= 3; ++k) if (p.to[static_cast<std::size_t>(k)] == 4) p.to[static_cast<std::size_t>(k)] = std::min(3, k + 1);
    // Who's where now.
    Bases next;
    for (int k = 0; k <= 3; ++k) {
        const int who = k == 0 ? batting.order : bases.on[static_cast<std::size_t>(k)];
        const int to = p.to[static_cast<std::size_t>(k)];
        if (who < 0 || to < 1) continue;
        if (to == 4) { ++p.runs; continue; }
        next.on[static_cast<std::size_t>(to)] = who;
    }
    bases = next;
    p.after = bases.mask();
    batting.order = (batting.order + 1) % 9;
    return p;
}

} // namespace

SimulatedGame simulate_game(const Roster& home, const Roster& away, std::uint64_t seed, int innings) {
    Rng rng{seed | 1};
    for (int i = 0; i < 8; ++i) rng.next();
    Side home_side{home}, away_side{away};
    SimulatedGame game;
    int home_runs = 0, away_runs = 0;
    const auto half = [&](int inning, bool home_bats) {
        Side& batting = home_bats ? home_side : away_side;
        Side& fielding = home_bats ? away_side : home_side;
        std::vector<Play> plays;
        Bases bases;
        int outs = 0;
        // Extra innings: a runner on second to start (the man who made the last out).
        if (inning > innings) bases.on[2] = (batting.order + 8) % 9;
        while (outs < 3) {
            Play p = plate_appearance(rng, batting, fielding, bases, outs);
            p.inning = inning; p.home = home_bats;
            outs += p.outs_made;
            (home_bats ? home_runs : away_runs) += p.runs;
            plays.push_back(p);
            if (home_bats && inning >= innings && home_runs > away_runs) break;   // the walk-off
        }
        game.halves.push_back(std::move(plays));
    };
    for (int inning = 1; inning <= 60; ++inning) {
        half(inning, false);
        if (inning >= innings && home_runs > away_runs) break;    // no need for the bottom half
        half(inning, true);
        if (inning >= innings && home_runs != away_runs) break;
    }
    return game;
}

Roster make_roster(float strength, std::uint64_t seed) {
    static constexpr std::array<const char*, 24> first{{"Ivan", "Petro", "Marko", "Jozefo", "Paŭlo", "Tomaso", "Andreo", "Mikaelo",
        "Luko", "Stefano", "Karlo", "Davido", "Johano", "Rikardo", "Viktoro", "Aleksandro", "Danielo", "Martino", "Nikolao",
        "Filipo", "Bruno", "Oskaro", "Teodoro", "Simono"}};
    static constexpr std::array<const char*, 24> family{{"Kovač", "Novak", "Horvat", "Petrov", "Ŝmidt", "Kamenski", "Rossi",
        "Silva", "Lindqvist", "Moreno", "Fischer", "Kowalski", "Popescu", "Marinov", "Baker", "Verdano", "Laŭro", "Ŝtono",
        "Ferro", "Rivero", "Monto", "Arbaro", "Kampo", "Valo"}};
    Rng rng{seed | 1};
    for (int i = 0; i < 8; ++i) rng.next();
    const auto name = [&] {
        return std::string(first[static_cast<std::size_t>(rng.next() * 23.999F)]) + " " + family[static_cast<std::size_t>(rng.next() * 23.999F)];
    };
    const float mean = .5F + (strength - .5F) * 2.0F;
    const auto rating = [&] { return clamp01(rng.normal(mean, .13F)); };
    Roster r;
    for (auto& p : r.lineup) {
        p.name = name();
        p.contact = rating(); p.power = rating(); p.eye = rating();
        p.speed = rating(); p.arm = rating(); p.glove = rating();
    }
    // The batting order: the best bats up top.
    std::sort(r.lineup.begin(), r.lineup.end(), [](const Player& a, const Player& b) {
        return a.contact + a.eye + a.power > b.contact + b.eye + b.power;
    });
    // In the field: the best arm catches, the best glove and arm at short, the fastest in centre;
    // the weakest glove is the designated hitter; the rest fill in.
    std::array<bool, 9> placed{};
    const auto pick = [&](auto score) {
        int best = -1;
        float most = -1e9F;
        for (int i = 0; i < 9; ++i)
            if (!placed[static_cast<std::size_t>(i)] && score(r.lineup[static_cast<std::size_t>(i)]) > most) { most = score(r.lineup[static_cast<std::size_t>(i)]); best = i; }
        placed[static_cast<std::size_t>(best)] = true;
        return best;
    };
    pick([](const Player& p) { return -p.glove - p.arm; });   // the designated hitter
    r.position[1] = pick([](const Player& p) { return p.arm; });
    r.position[4] = pick([](const Player& p) { return p.glove + p.arm; });
    r.position[7] = pick([](const Player& p) { return p.speed + p.glove * .5F; });
    r.position[3] = pick([](const Player& p) { return p.glove; });
    r.position[5] = pick([](const Player& p) { return p.arm + p.glove * .5F; });
    r.position[6] = pick([](const Player& p) { return p.speed; });
    r.position[8] = pick([](const Player& p) { return p.arm; });
    r.position[2] = pick([](const Player& p) { return p.glove * .5F + p.power; });
    for (std::size_t k = 0; k < r.staff.size(); ++k) {
        auto& p = r.staff[k];
        p.name = name();
        p.velocity = rating(); p.control = rating(); p.stuff = rating();
        p.stamina = k == 0 ? clamp01(rng.normal(.75F, .1F)) : clamp01(rng.normal(.35F, .1F));
    }
    return r;
}

} // namespace outland::game::sports
