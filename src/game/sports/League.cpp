#include "outland/game/sports/League.hpp"
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
    int h = 0, a = 0;
    for (int inning = 1;; ++inning) {
        const double top_start = (inning - 1) * 2 * half_inning, bottom_start = top_start + half_inning;
        int r = runs_in_half(rng, away.strength, home.strength + edge);
        if (inning >= 15 && r == 0 && h == a) r = rng.next() < .5F ? 1 : 0;   // the long night ends
        out.away.push_back(r);
        for (int k = 0; k < r; ++k) out.runs.emplace_back(top_start + half_inning * (k + 1) / (r + 1), false);
        a += r;
        if (inning >= 9 && h > a) { out.home.push_back(-1); out.length = bottom_start; break; }   // no need for the bottom half
        r = runs_in_half(rng, home.strength + edge, away.strength);
        if (inning >= 15 && h + r == a) r += 1;
        if (inning >= 9 && h + r > a) r = std::min(r, a - h + 1);   // the winning run ends it
        out.home.push_back(r);
        for (int k = 0; k < r; ++k) out.runs.emplace_back(bottom_start + half_inning * (k + 1) / (r + 1), true);
        h += r;
        if (inning >= 9 && h != a) { out.length = bottom_start + half_inning; break; }
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
    const int half = static_cast<int>(t / half_inning);
    live.inning = half / 2 + 1;
    live.top = half % 2 == 0;
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
