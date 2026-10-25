// La Verda Basballigo: the schedule (a double round robin, two games a day, then the final),
// games settled by the seed and read off the clock, the table, travelling fans, fights, the
// championship and its riot, a new season, and the save. And the parks on the published map,
// where the game finds them (world::ballpark::find: their design origins, not the bounds' centres).
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/sports/BallGame.hpp"
#include "outland/game/sports/League.hpp"
#include "outland/world/Ballparks.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace outland::game::sports;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    League league;
    league.start(2, 1234);
    const auto& games = league.fixtures();
    check(games.size() == 21 && games.back().final, "twenty league games and the final");
    // Every pair twice, once at each park; two games a day; nobody plays twice in a day.
    for (int a = 0; a < team_count; ++a)
        for (int b = 0; b < team_count; ++b) {
            if (a == b) continue;
            int n = 0;
            for (const auto& g : games) n += !g.final && g.home == a && g.away == b;
            check(n == 1, "each team hosts each other once");
        }
    for (int day = 0; day < League::regular_days; ++day) {
        std::set<int> playing;
        int n = 0;
        for (const auto& g : games)
            if (!g.final && g.day == day) { ++n; check(playing.insert(g.home).second && playing.insert(g.away).second, "one game a day each"); }
        check(n == 2, "two games every day");
    }

    // A game: settled by the seed, the same every time; nine innings or more, never a tie.
    for (int i = 0; i < static_cast<int>(games.size()); ++i) {
        const auto line = league.linescore(i);
        check(line.away.size() >= League::innings && line.total(true) != line.total(false), "a real ball game");
        check(line.length > 120 && line.length < 900, "a few hours of island time: " + std::to_string(line.length));
        // Every run comes from a plate appearance; every half is three outs (bar a walk-off) and its
        // runs add up to the line score; the batting order turns over.
        for (std::size_t inning = 1; inning <= line.away.size(); ++inning)
            for (const bool home : {false, true}) {
                const int want = (home ? line.home : line.away)[inning - 1];
                if (want < 0) continue;
                int runs = 0, outs = 0;
                std::uint8_t bases = inning > static_cast<std::size_t>(League::innings) ? 2 : 0;   // extra innings: a man on second
                for (const auto& p : line.plays) {
                    if (p.inning != static_cast<int>(inning) || p.home != home) continue;
                    check(p.bases == bases && p.outs == outs, "each plate appearance picks up where the last left off");
                    // The record adds up: who's on after it, the runs, the outs.
                    std::uint8_t after = 0;
                    int scored = 0, outs_made = 0;
                    for (int k = 0; k <= 3; ++k) {
                        const bool there = k == 0 || ((p.bases >> (k - 1)) & 1);
                        const int to = p.to[static_cast<std::size_t>(k)];
                        if (!there) { check(to == -1, "nobody where nobody was"); continue; }
                        check(to == -1 || to >= k, "nobody runs backwards");
                        if (to == 4) ++scored; else if (to >= 1) after |= static_cast<std::uint8_t>(1 << (to - 1)); else ++outs_made;
                    }
                    check(after == p.after && scored == p.runs && outs_made == p.outs_made, "runners move by the rules");
                    check(p.balls <= 3 && p.strikes <= 2 && p.pitches >= 1, "a count");
                    runs += p.runs; outs += p.outs_made; bases = p.after;
                    check(p.action >= League::plate_appearance * .9 - 1e-6 && p.action <= League::plate_appearance * 4.1 + 1e-6 && p.length > p.action && p.length < p.action + 10, "a plate appearance takes a few island minutes");
                }
                check(runs == want, "the plays bring in the half's runs");
                const bool walkoff = home && static_cast<int>(inning) == static_cast<int>(line.away.size()) && line.total(true) > line.total(false);
                check(outs == 3 || (walkoff && outs < 3), "three outs a half (or a walk-off): " + std::to_string(outs));
            }
        for (std::size_t k = 1; k < line.plays.size(); ++k)
            check(line.plays[k].at >= line.plays[k - 1].at + line.plays[k - 1].length - 1e-9, "one at a time");
        int scored = 0;
        for (const auto& p : line.plays) scored += p.runs;
        check(scored == static_cast<int>(line.runs.size()), "each run has its play");
    }
    // The game as it is played: nine fielders and a batter all through a plate appearance; the
    // ball leaves the pitcher's hand, reaches the plate, and (put in play) lands where the play says;
    // every runner on base is out there; the teams line up before and celebrate after.
    {
        const auto line = league.linescore(0);
        const auto& fixture = games[0];
        int checked = 0, pitches = 0, in_play = 0, runners_seen = 0, cracks = 0;
        for (const auto& p : line.plays) {
            for (const double u : {.05, .2, .33, .45, .6, .8, .95, 1.1}) {
                const auto sc = scene(fixture, line, p.at + std::min(p.action * u, p.length - .01));
                int fielders = 0, batters = 0, runners = 0;
                for (const auto& f : sc.figures) {
                    fielders += f.role == Role::Fielder;
                    batters += f.role == Role::Batter;
                    runners += f.role == Role::Runner;
                    check(std::abs(f.at.x) < 68 && f.at.z > -76 && f.at.z < 64, "everyone inside the park");
                    check(std::isfinite(f.speed) && f.speed < 16, "nobody runs faster than a man can: " + std::to_string(f.speed));
                }
                check(fielders == 9 && (u > 1 || (sc.play >= 0 && batters == 1)), "nine fielders and a batter");
                if (u < .3) check(runners == __builtin_popcount(p.bases), "every runner on base");
                runners_seen += runners;
                if (u > .3 && u < .37) { check(sc.ball && sc.ball_at.z < -17 && sc.ball_at.z > -37, "the pitch on its way"); ++pitches; }
                if (sc.ball) check(std::abs(sc.ball_at.x) < 110 && sc.ball_at.y > -.1F && sc.ball_at.y < 40, "the ball somewhere it can be");
                ++checked;
            }
            if (!p.out() || p.outcome == Outcome::Groundout || p.outcome == Outcome::Flyout)
                if (p.outcome != Outcome::Walk && p.outcome != Outcome::Strikeout) {
                    ++in_play;
                    cracks += crack(line, p.at, p.at + p.length);
                }
        }
        check(checked > 100 && pitches == static_cast<int>(line.plays.size()) && cracks == in_play, "pitched, hit and fielded");
        (void)runners_seen;
        int lined_up = 0;
        for (const auto& f : scene(fixture, line, -10).figures) lined_up += f.role == Role::Bench;
        check(lined_up == 18, "both teams lined up before first pitch");
        int cheering = 0;
        for (const auto& f : scene(fixture, line, line.length + 5).figures) cheering += f.motion == Motion::Cheer;
        check(cheering == 9, "the winners mob the mound");
        check(scene(fixture, line, -League::pregame - 5).figures.empty() && scene(fixture, line, line.length + League::postgame + 5).figures.empty(), "an empty field otherwise");
    }
    // Real baseball out of the simulation: over many games, the numbers of high-school ball - and
    // the better club wins more often, but not always.
    {
        int pa = 0, k = 0, bb = 0, hits = 0, hr = 0, dp = 0, errors = 0, ab = 0, runs = 0, games_played = 0, extra = 0;
        std::array<int, team_count> wins{};
        for (int season = 0; season < 6; ++season) {
            League l;
            l.start(2, 9000 + static_cast<std::uint64_t>(season) * 7);
            for (int i = 0; i < static_cast<int>(l.fixtures().size()); ++i) {
                const auto line = l.linescore(i);
                ++games_played;
                runs += line.total(true) + line.total(false);
                extra += line.away.size() > static_cast<std::size_t>(League::innings);
                const auto& g = l.fixtures()[static_cast<std::size_t>(i)];
                ++wins[static_cast<std::size_t>(line.total(true) > line.total(false) ? g.home : g.away)];
                for (const auto& p : line.plays) {
                    ++pa;
                    k += p.outcome == Outcome::Strikeout;
                    bb += p.outcome == Outcome::Walk;
                    hr += p.outcome == Outcome::HomeRun;
                    dp += p.outcome == Outcome::DoublePlay;
                    errors += p.outcome == Outcome::Error;
                    hits += p.outcome == Outcome::Single || p.outcome == Outcome::Double || p.outcome == Outcome::Triple || p.outcome == Outcome::HomeRun;
                    ab += p.outcome != Outcome::Walk;
                }
            }
        }
        const double avg = static_cast<double>(hits) / ab, k_rate = static_cast<double>(k) / pa, bb_rate = static_cast<double>(bb) / pa;
        const double per_game = static_cast<double>(runs) / games_played;
        std::cout << "simulated " << games_played << " games: " << per_game << " runs a game, avg " << avg << ", K " << k_rate
                  << ", BB " << bb_rate << ", HR/game " << static_cast<double>(hr) / games_played << ", DP/game "
                  << static_cast<double>(dp) / games_played << ", errors/game " << static_cast<double>(errors) / games_played
                  << ", extra innings " << extra << '\n';
        check(avg > .2 && avg < .34, "a batting average like ball games have");
        check(k_rate > .1 && k_rate < .3 && bb_rate > .04 && bb_rate < .16, "strikeouts and walks");
        check(per_game > 5 && per_game < 16, "runs");
        check(hr > 0 && dp > 0 && errors > 0, "home runs, double plays, errors all happen");
        check(wins[0] > wins[3], "the strongest club (the Leonoj) wins more than the weakest (the Ardeoj)");
        check(wins[3] > 0, "...but the weakest wins some");
    }
    const auto first = league.linescore(0), again = league.linescore(0);
    check(first.runs == again.runs, "the same game every time it is asked for");

    // Read off the clock.
    const double pitch = league.first_pitch_at(0);
    check(pitch == (2 + games[0].day) * League::day_minutes + League::first_pitch, "first pitch at 14:00 on its day");
    check(league.live_fixture(0, pitch - 200).phase == Phase::Quiet, "nothing in the morning");
    check(league.live_fixture(0, pitch - 30).phase == Phase::Pregame, "the gates open");
    const auto mid = league.live_fixture(0, pitch + first.length / 2);
    check(mid.phase == Phase::Playing && mid.inning >= 4 && mid.outs >= 0 && mid.outs <= 3, "under way");
    const auto over = league.live_fixture(0, pitch + first.length + 5);
    check(over.phase == Phase::Over && over.home_runs == first.total(true) && over.away_runs == first.total(false), "the final score");
    check(league.live(games[0].home, pitch + 10).fixture == 0 && league.live(games[0].away, pitch + 10).phase == Phase::Quiet,
          "the game is at the home team's park");

    // The events, as the clock runs through the first day.
    int on = 0, runs = 0, overs = 0;
    league.update(pitch - 60);
    for (double t = pitch - 60; t < pitch + 600; t += 5)
        for (const auto& e : league.update(t)) {
            on += e.kind == EventKind::GameOn;
            runs += e.kind == EventKind::Run;
            overs += e.kind == EventKind::GameOver;
        }
    check(on == 2 && overs == 2, "two games on, two over");
    check(runs == first.total(true) + first.total(false) + league.linescore(1).total(true) + league.linescore(1).total(false), "every run reported");

    // The table and the final.
    const double end = std::max(league.first_pitch_at(18) + league.linescore(18).length, league.first_pitch_at(19) + league.linescore(19).length) + 1;
    const auto table = league.standings(end);
    int wins = 0, losses = 0;
    for (const auto& s : table) { wins += s.wins; losses += s.losses; check(s.wins + s.losses == 8, "eight games each"); }
    check(wins == 20 && losses == 20, "every game counted");
    check(league.finalists(end - 2).first == -1, "no finalists till the last league game is over");
    const auto [top, second] = league.finalists(end);
    check(top == table[0].team && second == table[1].team && games.back().home == top, "the top two meet, the leader at home");
    const int fin = static_cast<int>(games.size()) - 1;
    const double final_end = league.first_pitch_at(fin) + league.linescore(fin).length;
    check(league.champion(final_end - 1) == -1 && (league.champion(final_end + 1) == top || league.champion(final_end + 1) == second), "a champion");
    check(league.fight_at(fin) >= 0, "the final always boils over in the stands");
    check(league.rioting(top, final_end + 10) && !league.rioting(top, final_end + League::riot_minutes + 1) && !league.rioting(second, final_end + 10),
          "a riot at the final's park, for a while");
    // The championship's events: champion and riot.
    League live = league;
    live.update(final_end - 30);
    bool champion = false, riot = false;
    for (double t = final_end - 30; t < final_end + 30; t += 5)
        for (const auto& e : live.update(t)) { champion |= e.kind == EventKind::Champion; riot |= e.kind == EventKind::Riot; }
    check(champion && riot, "the championship, and the riot");
    check(live.rivalry(top, second) > league.rivalry(top, second), "the final sharpens the rivalry");

    // Fans: rivals' followers travel more; fights between rivals are likelier.
    int fights = 0;
    for (int i = 0; i + 1 < static_cast<int>(games.size()); ++i) fights += league.fight_at(i) >= 0;
    check(fights >= 2 && fights <= 16, "some fights in the stands: " + std::to_string(fights));
    for (int i = 0; i < static_cast<int>(games.size()); ++i) check(league.travelling(i) > 0 && league.travelling(i) <= 1, "travelling support");

    // A new season after the last day; the save.
    League next = league;
    next.update(end);
    const auto events = next.update((2.0 + League::season_days) * League::day_minutes + 1);
    bool fresh = false;
    for (const auto& e : events) fresh |= e.kind == EventKind::NewSeason;
    check(fresh && next.season() == 2 && next.season_day((2.0 + League::season_days) * League::day_minutes + 1) == 0, "season two");
    League loaded;
    check(loaded.parse(live.serialise()), "parse");
    check(loaded.season() == live.season() && loaded.rivalry(top, second) == live.rivalry(top, second) &&
          loaded.linescore(3).runs == live.linescore(3).runs, "the save brings back the same league");
    check(!loaded.parse("league 9 1 2 3") && !loaded.parse("nonsense"), "bad saves refused");
    // The ballparks on the map: one per team, where the terrain was levelled for it.
    outland::world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    outland::world::VerdaRegion region(true);
    check(outland::creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    namespace bp = outland::world::ballpark;
    const auto parks = bp::find(region);
    for (int t = 0; t < team_count; ++t) {
        const auto& site = parks[static_cast<std::size_t>(t)];
        const auto& park = bp::parks[static_cast<std::size_t>(t)];
        check(site.team == t, std::string("a ballpark for the ") + teams[static_cast<std::size_t>(t)].nick);
        check(std::hypot(site.origin.x - park.centre.x, site.origin.z - park.centre.z) < .5F, "found at its design origin, not its bounds' centre");
        const auto* mesh = outland::world::physics::MeshCollisionLibrary::get(bp::model(t));
        check(mesh && std::abs(mesh->source_min.z - bp::model_min.z) < .01F && std::abs(mesh->source_max.z - bp::model_max.z) < .01F &&
              std::abs(mesh->source_min.y - bp::model_min.y) < .01F && std::abs(mesh->source_max.x - bp::model_max.x) < .01F, "the model's bounds as written");
    }
    std::cout << "league: champion " << teams[static_cast<std::size_t>(league.champion(final_end + 1))].name << ", " << fights << " fights\n";
    return 0;
}
