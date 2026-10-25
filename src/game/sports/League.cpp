#include "outland/game/sports/League.hpp"
#include "outland/world/Ballparks.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace outland::game::sports {
namespace {
// Old rivalries: city against country, the two coasts, the backwoods against the highlands.
constexpr std::array<std::array<float, 5>, 5> old_rivalries{{
    {0, .4F, .3F, .2F, .5F},
    {.4F, 0, .2F, .2F, .6F},
    {.3F, .2F, 0, .5F, .2F},
    {.2F, .2F, .5F, 0, .3F},
    {.5F, .6F, .2F, .3F, 0},
}};
struct Rng {
    std::uint64_t s;
    float next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return static_cast<float>(s >> 40) / static_cast<float>(1ULL << 24);
    }
};
// Runs in a half-inning: mostly nothing, now and then a big inning; better hitters against a
// weaker pitching side score more.
int runs_in_half(Rng& rng, float bat, float pitch) {
    const float edge = (bat - pitch) * .35F;
    const std::array<float, 6> p{.70F - edge, .15F + edge * .4F, .08F + edge * .3F, .04F + edge * .2F, .02F + edge * .1F, .01F};
    float u = rng.next(), sum = 0;
    for (std::size_t k = 0; k < p.size(); ++k) { sum += p[k]; if (u < sum) return static_cast<int>(k); }
    return 5;
}

// One half-inning, plate appearance by plate appearance, bringing in exactly `need` runs: three
// outs (or, for a walk-off, until the winning run is in). `edge`: batters over pitchers.
std::vector<Play> plan_half(Rng& rng, int need, bool walkoff, float edge, int& order, bool home, int inning, double start) {
    std::vector<Play> out;
    constexpr std::array<Outcome, 8> kinds{Outcome::Strikeout, Outcome::Walk, Outcome::Groundout, Outcome::Flyout,
                                            Outcome::Single, Outcome::Double, Outcome::Triple, Outcome::HomeRun};
    const float hit = 1 + 2 * edge, miss = 1 - edge;
    const std::array<float, 8> base{.21F * miss, .08F, .30F * miss, .23F * miss, .12F * hit, .04F * hit, .006F * hit, .025F * hit};
    int outs = 0, runs = 0;
    std::uint8_t bases = 0;
    while (outs < 3 && !(walkoff && runs >= need)) {
        const int n = static_cast<int>(out.size());
        std::array<float, 8> w{};
        float total = 0;
        for (std::size_t k = 0; k < kinds.size(); ++k) {
            const auto a = advance(bases, kinds[k], outs);
            bool ok = runs + a.runs <= need;
            if (a.out && outs == 2 && runs < need) ok = false;     // the half isn't over till its runs are in
            if (!a.out && runs >= need && n >= 7) ok = false;      // ...and once they are, it winds up
            float weight = base[k];
            if (runs < need && n >= 8) { if (a.out) ok = false; else if (a.runs > 0) weight *= 25; }
            w[k] = ok && n < 16 ? weight : 0;
            total += w[k];
        }
        Outcome pick = Outcome::Strikeout;
        if (total > 0) {
            float u = rng.next() * total;
            for (std::size_t k = 0; k < kinds.size(); ++k) { if (u < w[k]) { pick = kinds[k]; break; } u -= w[k]; pick = kinds[k]; }
        } else if (runs < need) {
            // The most it can bring in without going past.
            int best = -1;
            for (const auto k : {Outcome::HomeRun, Outcome::Triple, Outcome::Double, Outcome::Single, Outcome::Walk}) {
                const int r = advance(bases, k, outs).runs;
                if (runs + r <= need && r > best) { best = r; pick = k; }
            }
        }
        const auto a = advance(bases, pick, outs);
        Play p;
        p.home = home; p.inning = inning; p.batter = order; p.outcome = pick; p.outs = outs;
        p.bases = bases; p.after = a.after; p.runs = a.runs;
        // Where the ball goes and who gets to it (angles + toward third base).
        const auto infielder = [](float angle) { return angle < -24 ? 2 : angle < -3 ? 3 : angle < 20 ? 4 : 5; };
        const auto outfielder = [](float angle) { return angle < -15 ? 8 : angle > 15 ? 6 : 7; };
        const auto spread = [&](float lo, float hi) { return lo + (hi - lo) * rng.next(); };
        const float sign = rng.next() < .5F ? -1.0F : 1.0F;
        switch (pick) {
            case Outcome::Strikeout: case Outcome::Walk: p.fielder = 1; break;
            case Outcome::Groundout:
                if (rng.next() < .08F) { p.angle = spread(-6, 6); p.depth = 15; p.fielder = 0; }
                else { p.angle = spread(-40, 40); p.fielder = infielder(p.angle); p.depth = p.fielder == 2 || p.fielder == 5 ? 26 : 31; }
                break;
            case Outcome::Flyout:
                if (rng.next() < .25F) { p.angle = spread(-35, 35); p.depth = spread(18, 32); p.fielder = infielder(p.angle); }
                else { p.angle = spread(-40, 40); p.depth = spread(58, 80); p.fielder = outfielder(p.angle); }
                break;
            case Outcome::Single: p.angle = spread(-38, 38); p.depth = spread(48, 62); p.fielder = outfielder(p.angle); break;
            case Outcome::Double:
                p.angle = rng.next() < .6F ? sign * spread(28, 42) : sign * spread(12, 24);
                p.depth = spread(76, 82); p.fielder = outfielder(p.angle); break;
            case Outcome::Triple: p.angle = sign * spread(34, 43); p.depth = 82; p.fielder = outfielder(p.angle); break;
            case Outcome::HomeRun: p.angle = spread(-38, 38); p.depth = spread(96, 112); p.fielder = outfielder(p.angle); break;
        }
        out.push_back(p);
        outs += a.out ? 1 : 0;
        runs += a.runs;
        bases = a.after;
        order = (order + 1) % 9;
    }
    // The clock: after the change-over, each in turn, as long as its running takes.
    double t = start + League::change_over;
    for (auto& p : out) {
        int furthest = 0;
        const auto a = advance(p.bases, p.outcome, p.outs);
        for (int k = 0; k <= 3; ++k) {
            const int to = a.to[static_cast<std::size_t>(k)];
            if (to > k && (k == 0 || ((p.bases >> (k - 1)) & 1))) furthest = std::max(furthest, to - k);
        }
        double weight = 1.4 + .9 * furthest;   // a hit: the ball run down, the runners round
        switch (p.outcome) {
            case Outcome::Strikeout: weight = 1.0; break;
            case Outcome::Walk: weight = 2.0; break;
            case Outcome::Groundout: weight = 2.2 + .5 * furthest; break;
            case Outcome::Flyout: weight = 1.8 + .6 * furthest; break;
            case Outcome::HomeRun: weight = 3.6; break;
            default: break;
        }
        // Then back to their places: the fielder who ran the ball down jogs back (and an outfield
        // hit brings the cut-off man out too).
        namespace BP = world::ballpark;
        const float angle = p.angle * DEG2RAD;
        const Vector3 dir{std::sin(angle), 0, std::cos(angle)};
        const auto from_plate = [&](float metres) { return Vector3Add({BP::plate.x, 0, BP::plate.z}, Vector3Scale(dir, metres)); };
        Vector3 chased = from_plate(p.depth);
        if (p.outcome == Outcome::Single) chased = from_plate(p.depth + 4);
        if (p.outcome == Outcome::Double || p.outcome == Outcome::Triple || p.outcome == Outcome::HomeRun) chased = from_plate(BP::fence_radius - 2.5F);
        const Vector3 spot = BP::fielders[static_cast<std::size_t>(std::clamp(p.fielder, 0, 8))];
        float back = p.outcome == Outcome::Strikeout || p.outcome == Outcome::Walk ? 0 : Vector3Distance({spot.x, 0, spot.z}, chased);
        if (p.fielder >= 6 && p.outcome != Outcome::Flyout) back = std::max(back, 22.0F);
        p.at = t;
        p.action = League::plate_appearance * weight;
        p.length = p.action + (std::max(back / 6.5F, 2.0F) + 1.0) / League::real_seconds_per_minute;   // (and the next man up from the on-deck circle)
        t += p.length;
    }
    return out;
}
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
        case Outcome::Strikeout: return "strikes out"; case Outcome::Walk: return "walks";
        case Outcome::Groundout: return "grounds out"; case Outcome::Flyout: return "flies out";
        case Outcome::Single: return "singles"; case Outcome::Double: return "doubles";
        case Outcome::Triple: return "triples"; case Outcome::HomeRun: return "homers";
    }
    return "";
}

Advance advance(std::uint8_t bases, Outcome outcome, int outs) {
    Advance a;
    const auto on = [&](int base) { return (bases >> (base - 1)) & 1; };
    for (int k = 1; k <= 3; ++k) if (on(k)) a.to[static_cast<std::size_t>(k)] = k;
    const auto move = [&](int base, int by) { if (on(base)) a.to[static_cast<std::size_t>(base)] = std::min(4, base + by); };
    switch (outcome) {
        case Outcome::Strikeout: a.out = true; break;
        case Outcome::Walk:
            a.to[0] = 1;
            if (on(1)) { a.to[1] = 2; if (on(2)) { a.to[2] = 3; if (on(3)) a.to[3] = 4; } }
            break;
        case Outcome::Groundout:
            a.out = true;
            if (outs < 2) for (int k = 3; k >= 1; --k) move(k, 1);   // a productive out: everyone moves up
            break;
        case Outcome::Flyout:
            a.out = true;
            if (outs < 2) move(3, 1);                                // the sacrifice fly
            break;
        case Outcome::Single: a.to[0] = 1; move(1, 1); move(2, 2); move(3, 1); break;
        case Outcome::Double: a.to[0] = 2; move(1, 2); move(2, 2); move(3, 1); break;
        case Outcome::Triple: a.to[0] = 3; move(1, 3); move(2, 2); move(3, 1); break;
        case Outcome::HomeRun: a.to[0] = 4; move(1, 3); move(2, 2); move(3, 1); break;
    }
    for (const int to : a.to) {
        if (to == 4) ++a.runs;
        else if (to >= 1) a.after |= static_cast<std::uint8_t>(1 << (to - 1));
    }
    return a;
}

int Linescore::play_at(double t) const {
    for (int i = 0; i < static_cast<int>(plays.size()); ++i) {
        const auto& p = plays[static_cast<std::size_t>(i)];
        if (t >= p.at && t < p.at + p.length) return i;
        if (p.at > t) break;
    }
    return -1;
}

int Linescore::half_at(double t) const {
    if (halves.empty() || t < halves.front()) return -1;
    const auto it = std::upper_bound(halves.begin(), halves.end(), t);
    return static_cast<int>(it - halves.begin()) - 1;
}

double Linescore::half_end(int half) const {
    if (half < 0) return 0;
    return half + 1 < static_cast<int>(halves.size()) ? halves[static_cast<std::size_t>(half) + 1] : length;
}

int Linescore::total(bool home_team) const {
    int n = 0;
    for (const int r : home_team ? home : away) n += std::max(0, r);
    return n;
}

std::uint64_t League::mix(std::uint64_t a, std::uint64_t b) const {
    std::uint64_t h = seed_ ^ (a * 0x9e3779b97f4a7c15ULL) ^ (b * 0xc2b2ae3d27d4eb4fULL) ^ (static_cast<std::uint64_t>(season_) * 0x165667b19e3779f9ULL);
    h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;
    return h | 1;
}

void League::start(int day, std::uint64_t seed) {
    season_ = 1;
    start_day_ = day;
    seed_ = seed | 1;
    for (int a = 0; a < team_count; ++a)
        for (int b = 0; b < team_count; ++b) rivalry_[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)] = old_rivalries[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
    processed_ = -1;
    schedule();
}

void League::schedule() {
    fixtures_.clear();
    // The circle method: five teams and a bye, five rounds; then the same with home and away swapped.
    std::array<int, 6> ring{0, 1, 2, 3, 4, 5};
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 3; ++i) {
            const int a = ring[static_cast<std::size_t>(i)], b = ring[static_cast<std::size_t>(5 - i)];
            if (a == 5 || b == 5) continue;
            const bool a_home = (round + i) % 2 == 0;
            fixtures_.push_back({a_home ? a : b, a_home ? b : a, round, false});
        }
        std::rotate(ring.begin() + 1, ring.begin() + 5, ring.end());
    }
    const std::size_t first_half = fixtures_.size();
    for (std::size_t k = 0; k < first_half; ++k) {
        const auto f = fixtures_[k];
        fixtures_.push_back({f.away, f.home, f.day + 5, false});
    }
    // The final: the top two of the table once the regular season is played (the leader at home).
    const auto table = standings(1e18);
    fixtures_.push_back({table[0].team, table[1].team, final_day, true});
}

double League::first_pitch_at(int fixture) const {
    if (fixture < 0 || fixture >= static_cast<int>(fixtures_.size())) return 1e18;
    return (static_cast<double>(start_day_) + fixtures_[static_cast<std::size_t>(fixture)].day) * day_minutes + first_pitch;
}

Linescore League::linescore(int fixture) const {
    Linescore out;
    if (fixture < 0 || fixture >= static_cast<int>(fixtures_.size())) return out;
    const auto& f = fixtures_[static_cast<std::size_t>(fixture)];
    Rng rng{mix(static_cast<std::uint64_t>(fixture) + 1, 17)};
    const auto& home = teams[static_cast<std::size_t>(f.home)];
    const auto& away = teams[static_cast<std::size_t>(f.away)];
    const float edge = .03F;   // home field
    int h = 0, a = 0, away_order = 0, home_order = 0;
    const auto half = [&](int inning, bool home_bats, int need, bool walkoff, double start) {
        Rng plays{mix(static_cast<std::uint64_t>(fixture) + 1, 1000 + static_cast<std::uint64_t>(inning) * 2 + (home_bats ? 1 : 0))};
        const float lean = home_bats ? home.strength + edge - away.strength : away.strength - home.strength - edge;
        const auto list = plan_half(plays, need, walkoff, lean, home_bats ? home_order : away_order, home_bats, inning, start);
        for (const auto& p : list) {
            for (int k = 0; k < p.runs; ++k) out.runs.emplace_back(p.run_time(), home_bats);
            out.plays.push_back(p);
        }
        out.halves.push_back(start);
        return list.empty() ? start + League::change_over : list.back().at + list.back().length;
    };
    double clock = 0;
    for (int inning = 1;; ++inning) {
        int r = runs_in_half(rng, away.strength, home.strength + edge);
        if (inning >= innings + 5 && r == 0 && h == a) r = rng.next() < .5F ? 1 : 0;   // the long night ends
        out.away.push_back(r);
        clock = half(inning, false, r, false, clock) + wind_down;
        a += r;
        if (inning >= innings && h > a) { out.home.push_back(-1); out.length = clock; break; }   // no need for the bottom half
        r = runs_in_half(rng, home.strength + edge, away.strength);
        if (inning >= innings + 5 && h + r == a) r += 1;
        const bool walkoff = inning >= innings && h + r > a;
        if (walkoff) r = std::min(r, a - h + 1);   // the winning run ends it
        out.home.push_back(r);
        const double end = half(inning, true, r, walkoff, clock);
        clock = end + wind_down;
        h += r;
        if (inning >= innings && h != a) { out.length = walkoff ? end + 1.0 : clock; break; }
    }
    std::sort(out.runs.begin(), out.runs.end());
    return out;
}

Live League::live_fixture(int fixture, double now) const {
    Live live;
    live.fixture = fixture;
    if (fixture < 0 || fixture >= static_cast<int>(fixtures_.size())) { live.phase = Phase::Quiet; return live; }
    const double t = now - first_pitch_at(fixture);
    const auto line = linescore(fixture);
    if (t < -pregame || t > line.length + postgame) { live.phase = Phase::Quiet; return live; }
    if (t < 0) { live.phase = Phase::Pregame; return live; }
    for (const auto& [at, home] : line.runs) if (at <= t) (home ? live.home_runs : live.away_runs)++;
    if (t >= line.length) {
        live.phase = Phase::Over;
        live.inning = static_cast<int>(line.away.size());
        live.top = false;
        return live;
    }
    live.phase = Phase::Playing;
    const int half = std::max(0, line.half_at(t));
    live.inning = half / 2 + 1;
    live.top = half % 2 == 0;
    for (const auto& p : line.plays) {
        if (p.at > t) break;
        if (p.inning != live.inning || p.home == live.top) continue;
        const bool done = t >= p.at + p.action * Play::scored;
        live.outs = p.outs + (done && p.out() ? 1 : 0);
        live.bases = done ? p.after : p.bases;
    }
    return live;
}

Live League::live(int park, double now) const {
    const int day = season_day(now);
    for (int i = 0; i < static_cast<int>(fixtures_.size()); ++i) {
        const auto& f = fixtures_[static_cast<std::size_t>(i)];
        if (f.home != park || f.day != day) continue;
        const auto l = live_fixture(i, now);
        if (l.phase != Phase::Quiet) return l;
    }
    return {};
}

int League::season_day(double now) const {
    return static_cast<int>(std::floor(now / day_minutes)) - start_day_;
}

std::vector<Standing> League::standings(double now) const {
    std::vector<Standing> table(teams.size());
    for (int t = 0; t < team_count; ++t) table[static_cast<std::size_t>(t)].team = t;
    for (int i = 0; i < static_cast<int>(fixtures_.size()); ++i) {
        const auto& f = fixtures_[static_cast<std::size_t>(i)];
        if (f.final) continue;
        const auto line = linescore(i);
        if (now < first_pitch_at(i) + line.length) continue;
        const int h = line.total(true), a = line.total(false);
        auto& home = table[static_cast<std::size_t>(f.home)];
        auto& away = table[static_cast<std::size_t>(f.away)];
        (h > a ? home.wins : home.losses)++;
        (a > h ? away.wins : away.losses)++;
        home.runs_for += h; home.runs_against += a;
        away.runs_for += a; away.runs_against += h;
    }
    std::stable_sort(table.begin(), table.end(), [](const Standing& x, const Standing& y) {
        if (x.wins != y.wins) return x.wins > y.wins;
        return x.runs_for - x.runs_against > y.runs_for - y.runs_against;
    });
    return table;
}

std::pair<int, int> League::finalists(double now) const {
    if (fixtures_.empty() || !fixtures_.back().final) return {-1, -1};
    for (int i = 0; i + 1 < static_cast<int>(fixtures_.size()); ++i)
        if (now < first_pitch_at(i) + linescore(i).length) return {-1, -1};
    return {fixtures_.back().home, fixtures_.back().away};
}

int League::champion(double now) const {
    if (fixtures_.empty() || !fixtures_.back().final) return -1;
    const int i = static_cast<int>(fixtures_.size()) - 1;
    const auto line = linescore(i);
    if (now < first_pitch_at(i) + line.length) return -1;
    return line.total(true) > line.total(false) ? fixtures_.back().home : fixtures_.back().away;
}

int League::next_fixture(int team, double now) const {
    for (int i = 0; i < static_cast<int>(fixtures_.size()); ++i) {
        const auto& f = fixtures_[static_cast<std::size_t>(i)];
        if ((f.home == team || f.away == team) && now < first_pitch_at(i) + linescore(i).length) return i;
    }
    return -1;
}

float League::rivalry(int a, int b) const {
    if (a < 0 || b < 0 || a >= team_count || b >= team_count || a == b) return 0;
    return rivalry_[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
}

float League::travelling(int fixture) const {
    if (fixture < 0 || fixture >= static_cast<int>(fixtures_.size())) return 0;
    const auto& f = fixtures_[static_cast<std::size_t>(fixture)];
    const float base = teams[static_cast<std::size_t>(f.away)].travel * (.6F + rivalry(f.home, f.away) * .8F);
    return std::clamp(base * (f.final ? 1.5F : 1.0F), .1F, 1.0F);
}

double League::fight_at(int fixture) const {
    if (fixture < 0 || fixture >= static_cast<int>(fixtures_.size())) return -1;
    const auto& f = fixtures_[static_cast<std::size_t>(fixture)];
    Rng rng{mix(static_cast<std::uint64_t>(fixture) + 1, 99)};
    const float chance = f.final ? 1.0F : .2F + rivalry(f.home, f.away) * .5F;
    const float roll = rng.next(), when = rng.next();
    if (roll >= chance) return -1;
    const double length = linescore(fixture).length;
    return 20 + (length - 30) * when;
}

bool League::rioting(int park, double now) const {
    if (fixtures_.empty() || !fixtures_.back().final || fixtures_.back().home != park) return false;
    const int i = static_cast<int>(fixtures_.size()) - 1;
    const double end = first_pitch_at(i) + linescore(i).length;
    return now >= end && now < end + riot_minutes;
}

std::vector<Event> League::update(double now) {
    std::vector<Event> out;
    if (fixtures_.empty()) return out;
    while (now >= (static_cast<double>(start_day_) + season_days) * day_minutes) {
        start_day_ += season_days;
        ++season_;
        schedule();
        out.push_back({EventKind::NewSeason, -1, -1});
        processed_ = std::max(processed_, static_cast<double>(start_day_) * day_minutes);
    }
    if (processed_ < 0 || now - processed_ > day_minutes) { processed_ = now; return out; }   // (loaded, or a long sleep: no flood)
    const auto between = [&](double t) { return t > processed_ && t <= now; };
    for (int i = 0; i < static_cast<int>(fixtures_.size()); ++i) {
        const auto& f = fixtures_[static_cast<std::size_t>(i)];
        const double start = first_pitch_at(i);
        if (start - processed_ > day_minutes || now - start > day_minutes) continue;
        const auto line = linescore(i);
        if (between(start)) out.push_back({EventKind::GameOn, i, f.home});
        for (const auto& [at, home] : line.runs)
            if (between(start + at)) out.push_back({EventKind::Run, i, home ? f.home : f.away});
        if (const double fight = fight_at(i); fight >= 0 && between(start + fight)) {
            out.push_back({EventKind::Fight, i, f.away});
            auto& r = rivalry_[static_cast<std::size_t>(f.home)][static_cast<std::size_t>(f.away)];
            r = std::min(1.0F, r + .1F);
            rivalry_[static_cast<std::size_t>(f.away)][static_cast<std::size_t>(f.home)] = r;
        }
        if (between(start + line.length)) {
            const int winner = line.total(true) > line.total(false) ? f.home : f.away;
            out.push_back({EventKind::GameOver, i, winner});
            if (f.final) {
                out.push_back({EventKind::Champion, i, winner});
                out.push_back({EventKind::Riot, i, winner == f.home ? f.away : f.home});   // the losers' fans riot
                auto& r = rivalry_[static_cast<std::size_t>(f.home)][static_cast<std::size_t>(f.away)];
                r = std::min(1.0F, r + .2F);
                rivalry_[static_cast<std::size_t>(f.away)][static_cast<std::size_t>(f.home)] = r;
            }
        }
    }
    processed_ = now;
    return out;
}

std::string League::serialise() const {
    std::ostringstream out;
    out << "league 1 " << season_ << ' ' << start_day_ << ' ' << seed_;
    for (const auto& row : rivalry_) for (const float r : row) out << ' ' << r;
    out << '\n';
    return out.str();
}

bool League::parse(const std::string& text) {
    std::istringstream in(text);
    std::string tag;
    int version = 0, season = 0, start = 0;
    std::uint64_t seed = 0;
    if (!(in >> tag >> version >> season >> start >> seed) || tag != "league" || version != 1 || season < 1) return false;
    std::array<std::array<float, 5>, 5> rivalry{};
    for (auto& row : rivalry) for (auto& r : row) if (!(in >> r)) return false;
    season_ = season;
    start_day_ = start;
    seed_ = seed | 1;
    rivalry_ = rivalry;
    processed_ = -1;
    schedule();
    return true;
}

} // namespace outland::game::sports
