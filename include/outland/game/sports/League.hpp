#pragma once
#include <raylib.h>
#include <algorithm>
#include <memory>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// La Verda Basballigo (Explore): one high-school-sized ballpark in each of the five counties
// (world::ballpark), and a team for each county in its colours. They play a double round robin
// - two games every island day at 13:00, one team resting - then the top two meet in the
// championship final at the leader's park. Then a new season.
//
// Every game is played out - pitch by pitch, the batted ball flown and fielded, the runners racing
// the throws - between the two clubs' rosters, from the season's seed: so its line score (runs per
// half-inning, at the minute each run scores) follows from the fixture alone, so where a game stands is a function
// of the island clock and only the season and the rivalries need saving. The fans: the home crowd
// turns out, the visitors' followers travel (more for a rivalry), fights break out in the stands
// now and then (more between rivals) - and the championship final ends in a riot.
//
// Times are island minutes (game::life's clock). A half-inning takes as long as its plate
// appearances do; a seven-inning game (the high-school length) is
// five to six hours, first pitch at 13:00.
namespace outland::game::sports {

struct Team {
    const char* name;       // in full
    const char* nick;       // what the fans shout
    const char* town;       // its home town
    const char* site;       // world::layout site id of the town (its ballpark is in that county)
    const char* county;     // world::zones zone name
    Color primary, secondary;
    float strength;         // 0..1
    float travel;           // 0..1: how many of its fans follow it on the road
};

inline constexpr std::array<Team, 5> teams{{
    {"Verdaj Leonoj", "Leonoj", "Verda", "capital_verda", "Verdkerno", {18, 118, 58, 255}, {228, 182, 40, 255}, .56F, .35F},
    {"Altverdaj Ursoj", "Ursoj", "Espera", "village_espera", "Altverdo", {104, 66, 38, 255}, {240, 122, 24, 255}, .50F, .55F},
    {"Lumaj Mevoj", "Mevoj", "Porto Luma", "port_luma", "Lumamaro", {26, 46, 108, 255}, {118, 190, 240, 255}, .52F, .40F},
    {"Marĉaj Ardeoj", "Ardeoj", "Suda Haveno", "south_haven", "Marĉfluo", {0, 124, 126, 255}, {236, 236, 226, 255}, .46F, .45F},
    {"Feraj Ministoj", "Ministoj", "Roka", "west_roka", "Ferarbaro", {40, 40, 44, 255}, {196, 42, 30, 255}, .50F, .65F},
}};
inline constexpr int team_count = static_cast<int>(teams.size());

struct Fixture {
    int home{0}, away{0};
    int day{0};             // days from the season's start
    bool final{false};
};

// The players. Ratings run 0..1 (a club's strength sets its players' average); a game is played
// out between two rosters, pitch by pitch (game::sports::simulate_game).
struct Player {
    std::string name;
    float contact{.5F}, power{.5F}, eye{.5F};     // at the plate
    float speed{.5F}, arm{.5F}, glove{.5F};       // on the bases and in the field
};
struct Pitcher {
    std::string name;
    float velocity{.5F}, control{.5F}, stuff{.5F}, stamina{.5F};
};
struct Roster {
    std::array<Player, 9> lineup;                 // the batting order (the designated hitter bats for the pitcher)
    std::array<int, 9> position{};                // the lineup slot playing each fielding spot 1..8 (0: the pitcher)
    std::array<Pitcher, 3> staff;                 // the starter, then the bullpen
};

// A plate appearance and how it ends.
enum class Outcome : std::uint8_t { Strikeout, Walk, Groundout, Flyout, Single, Double, Triple, HomeRun, DoublePlay, Error };
const char* outcome_name(Outcome outcome);

struct Play {
    double at{0}, length{0};        // minutes after first pitch it starts; how long it takes in all
    double action{0};               // ...of which the play itself (the rest: everyone back to their places)
    bool home{false};               // the home side is batting
    int inning{1};
    int batter{0};                  // the batting order, 0..8
    Outcome outcome{Outcome::Strikeout};
    int outs{0};                    // outs before it
    std::uint8_t bases{0}, after{0};   // runners on (bit 0 first .. bit 2 third), before and after
    int runs{0};                    // runs it brings in
    int outs_made{0};               // outs it makes (2 for a double play)
    // Where the batter (0) and the runners on first..third (1..3) ended up: 1..3 a base, 4 home
    // (scored), -1 out (or nobody there).
    std::array<int, 4> to{{-1, -1, -1, -1}};
    int balls{0}, strikes{0};       // the count when it ended
    int pitches{1};                 // how many pitches it took
    int pitcher{0};                 // who was pitching (Roster::staff)
    float angle{0};                 // where the ball goes: degrees off the line to second, + toward third
    float depth{0};                 // metres from the plate it lands or is fielded
    float apex{0}, hang{0};         // a ball in the air: how high it went (m), how long it was up (s)
    bool grounder{false};           // a ball on the ground
    int fielder{1};                 // who fields it (world::ballpark::fielders index; the catcher on a strikeout or walk)
    [[nodiscard]] bool out() const { return to[0] < 0; }   // the batter is out
    // Through the play (0..1 of `action`): the pitch is let go, it reaches the plate, the runs score.
    static constexpr double release = .30, contact = .37, scored = .86;
    [[nodiscard]] double run_time() const { return at + action * scored; }
};

// One game's runs: per half-inning, and the minute (from first pitch) each run crosses the plate;
// every plate appearance that makes them.
struct Linescore {
    std::vector<int> away, home;            // runs per inning (the home half may be unplayed: -1)
    std::vector<std::pair<double, bool>> runs;   // (minutes after first pitch, home team?)
    std::vector<Play> plays;
    std::vector<double> halves;             // when each half-inning starts (top of the first, bottom, ...)
    double length{0};                       // minutes from first pitch to the last out
    [[nodiscard]] int total(bool home_team) const;
    // The plate appearance under way at `t` minutes after first pitch (-1 between them).
    [[nodiscard]] int play_at(double t) const;
    // The half-inning under way at `t` (0 the top of the first; -1 before first pitch), and when it ends.
    [[nodiscard]] int half_at(double t) const;
    [[nodiscard]] double half_end(int half) const;
};

struct Standing { int team{0}, wins{0}, losses{0}, runs_for{0}, runs_against{0}; };

enum class Phase : std::uint8_t { Quiet, Pregame, Playing, Over };

// A game as it stands at some minute.
struct Live {
    int fixture{-1};
    Phase phase{Phase::Quiet};
    int inning{1};
    bool top{true};
    int home_runs{0}, away_runs{0};
    int outs{0};                // in this half, so far
    std::uint8_t bases{0};      // runners on now
    int play{-1};               // the plate appearance under way or just over (Linescore::plays)
};

enum class EventKind : std::uint8_t { GameOn, Run, GameOver, Fight, Riot, Champion, NewSeason };
struct Event {
    EventKind kind{EventKind::GameOn};
    int fixture{-1};
    int team{-1};           // who scored / won / whose fans started it
};

class League {
public:
    static constexpr double day_minutes = 24 * 60;
    static constexpr double real_seconds_per_minute = 2;   // island life's clock runs 30 times real time
    static constexpr int innings = 7;                   // a high-school game
    static constexpr double first_pitch = 13 * 60;      // every game day
    static constexpr double change_over = 3;            // a half's first minutes: the fielders run out
    static constexpr double wind_down = 2;              // ...and its last: they run in
    static constexpr double plate_appearance = 2.0;     // minutes for a quick one (a strikeout is a little less, a triple far more)
    static constexpr double pregame = 75;               // the gates open, the crowd comes in
    static constexpr double postgame = 40;              // ...and goes home
    static constexpr double riot_minutes = 120;         // a riot burns this long
    static constexpr int regular_days = 10;             // a double round robin, two games a day
    static constexpr int final_day = regular_days + 1;  // then the final, after a day off
    static constexpr int season_days = final_day + 2;   // and the next season starts

    // A new league whose first season starts on island day `day` (seed: the session's).
    void start(int day, std::uint64_t seed);
    // Keeps the season in step with the clock (a new season after the last), and returns what
    // happened since the last call.
    std::vector<Event> update(double now);

    [[nodiscard]] int season() const { return season_; }
    [[nodiscard]] int season_day(double now) const;      // days into this season
    [[nodiscard]] const std::vector<Fixture>& fixtures() const { return fixtures_; }
    [[nodiscard]] double first_pitch_at(int fixture) const;
    // A club's players this season.
    [[nodiscard]] const Roster& roster(int team) const { return rosters_[static_cast<std::size_t>(std::clamp(team, 0, team_count - 1))]; }
    [[nodiscard]] Linescore linescore(int fixture) const;
    // The game at team `park`'s ballpark around `now` (pregame to postgame), if any.
    [[nodiscard]] Live live(int park, double now) const;
    [[nodiscard]] Live live_fixture(int fixture, double now) const;
    // The table: wins, then run difference. Games over by `now` count.
    [[nodiscard]] std::vector<Standing> standings(double now) const;
    // The final's two teams (the top two once the regular season is over; -1 before).
    [[nodiscard]] std::pair<int, int> finalists(double now) const;
    [[nodiscard]] int champion(double now) const;          // -1 until the final is over
    [[nodiscard]] int next_fixture(int team, double now) const;   // its next game not yet over, -1 none
    // Fans: how many of the visitors' followers make the trip (0..1), and the minute a fight breaks
    // out in a game (-1 none: rivals fight more).
    [[nodiscard]] float travelling(int fixture) const;
    [[nodiscard]] double fight_at(int fixture) const;
    [[nodiscard]] float rivalry(int a, int b) const;
    // A riot after the final, at the final's park, until riot_minutes after the last out.
    [[nodiscard]] bool rioting(int park, double now) const;

    [[nodiscard]] std::string serialise() const;
    bool parse(const std::string& text);

private:
    void schedule();
    [[nodiscard]] std::uint64_t mix(std::uint64_t a, std::uint64_t b) const;
    int season_{1};
    int start_day_{0};
    std::uint64_t seed_{1};
    std::vector<Fixture> fixtures_;
    std::array<Roster, 5> rosters_{};
    mutable std::vector<std::shared_ptr<const Linescore>> games_;   // each fixture's game, played out once
    std::array<std::array<float, 5>, 5> rivalry_{};
    double processed_{-1};
};

} // namespace outland::game::sports
