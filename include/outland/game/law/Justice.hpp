#pragma once
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace outland::characters { class NpcSystem; }
namespace outland::game::life { class LifeSimulation; }

// Crime, witnesses and the police on Verda (Explore). Nobody knows what they did not perceive:
// a crime is only reported if a resident saw it (or heard shots) and lived long enough to call.
// Reports raise the wanted level and dispatch the nearest officers; officers who see the player
// keep the trail warm, arrest at one star and shoot from two; staying unseen long enough ends it.
namespace outland::game::law {

enum class CrimeKind : std::uint8_t { ShotsFired, Assault, Murder, AttackOnPolice };
const char* crime_name(CrimeKind kind);
// Stars for a crime someone saw: shooting 1, assault 2, murder 3, attacking police 4.
int crime_severity(CrimeKind kind);

struct JusticeConfig {
    float sight_day{55}, sight_night{22};  // a witness sees the perpetrator this far (lit vs dark)
    float hearing{150};                    // gunshots heard this far: "shots fired" with no description
    float call_min{4}, call_max{14};       // seconds before a witness gets through to the police
    float evade_base{25}, evade_per_star{12}; // unseen this long and the police give up
    float arrest_radius{2.5F}, arrest_seconds{2}, calm_seconds{3};
    float investigate_seconds{60};         // responders to an anonymous report search this long
};

struct Witness {
    int resident{-1};
    CrimeKind kind{CrimeKind::ShotsFired};
    Vector3 where{};
    bool saw{false};     // saw the perpetrator (description); false = only heard shots
    double call_at{0};
};

struct JusticeFrame {
    float dt{0};
    double now{0};
    Vector3 player{};          // feet
    bool player_alive{true};
    bool player_fired{false};
    float daylight{1};         // 0 night .. 1 day (world::sky); darkness shortens sight
    std::function<bool(Vector3, Vector3)> line_of_sight; // eye to eye, true if clear
};

class Justice {
public:
    void configure(JusticeConfig config) { config_ = config; }
    void reset(game::life::LifeSimulation* life, characters::NpcSystem* npcs);

    // The player committed `kind` at `where`. `victim` is the resident hurt (or -1).
    void crime(CrimeKind kind, Vector3 where, int victim, const JusticeFrame& frame,
               const game::life::LifeSimulation& life);
    void update(const JusticeFrame& frame, game::life::LifeSimulation& life, characters::NpcSystem& npcs);

    [[nodiscard]] int wanted() const { return wanted_; }
    [[nodiscard]] Vector3 last_known() const { return last_known_; }
    [[nodiscard]] const std::vector<Witness>& witnesses() const { return pending_; }
    [[nodiscard]] const std::vector<int>& responders() const { return responders_; }
    [[nodiscard]] int reports() const { return reports_; }
    [[nodiscard]] bool arrested() const { return arrested_; } // this frame
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }
    [[nodiscard]] double unseen_for(double now) const { return now - last_seen_; }

private:
    void report(const Witness& witness, const JusticeFrame& frame, game::life::LifeSimulation& life, characters::NpcSystem& npcs);
    void dispatch(int count, Vector3 where, game::life::LifeSimulation& life);
    void stand_down(game::life::LifeSimulation& life, characters::NpcSystem& npcs, const char* why);
    void say(std::string text) { message_ = std::move(text); message_time_ = 4; }
    float sight(float daylight) const;
    float random();

    JusticeConfig config_{};
    std::vector<Witness> pending_;
    std::vector<int> responders_;
    int wanted_{0}, reports_{0};
    Vector3 last_known_{}, dispatched_to_{};
    double last_seen_{-1e9}, last_fired_{-1e9}, investigate_until_{-1e9}, redirect_at_{-1e9};
    float arrest_clock_{0};
    bool arrested_{false};
    std::string message_;
    float message_time_{0};
    std::uint64_t rng_{0x9e3779b97f4a7c15ULL};
};
}
