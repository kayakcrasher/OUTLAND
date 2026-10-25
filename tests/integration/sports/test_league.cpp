// La Verda Basballigo: the schedule (a double round robin, two games a day, then the final),
// games settled by the seed and read off the clock, the table, travelling fans, fights, the
// championship and its riot, a new season, and the save. And the parks on the published map,
// where the game finds them (world::ballpark::find: their design origins, not the bounds' centres).
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/sports/League.hpp"
#include "outland/world/Ballparks.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <cmath>
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
        check(line.away.size() >= 9 && line.total(true) != line.total(false), "a real ball game");
        check(line.length > 200 && line.length < 420, "two to six hours of island time");
    }
    const auto first = league.linescore(0), again = league.linescore(0);
    check(first.runs == again.runs, "the same game every time it is asked for");

    // Read off the clock.
    const double pitch = league.first_pitch_at(0);
    check(pitch == (2 + games[0].day) * League::day_minutes + League::first_pitch, "first pitch at 14:00 on its day");
    check(league.live_fixture(0, pitch - 200).phase == Phase::Quiet, "nothing in the morning");
    check(league.live_fixture(0, pitch - 30).phase == Phase::Pregame, "the gates open");
    const auto mid = league.live_fixture(0, pitch + first.length / 2);
    check(mid.phase == Phase::Playing && mid.inning >= 4, "under way");
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
    const double end = league.first_pitch_at(19) + league.linescore(19).length + 1;
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
