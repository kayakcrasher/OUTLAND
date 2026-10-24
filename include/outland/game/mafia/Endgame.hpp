#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::game::ai { class BattleRoyaleBots; }

// How Verda ends when the gangs win (Explore). The Ĉefo warned it would come to this: a nation
// that loses its law loses itself.
//
//   The siege. La Verda Mano are Esperantista Loyalists, nationalists: once they rule every town,
//   they march on Registaro de Verda, the island's parliament, to put the nation under the Green
//   Hand. The parliament's guard holds the steps; take them down and walk into the hall.
//   (Any other gang that wins skips this: they take the island, not the nation.)
//
//   The occupation. With the Policio broken and the parliament fallen, Russian troops come ashore
//   "to restore order" and garrison the five county seats. They fight anyone not wearing their
//   uniform - the player, every gang, the police - and the dead stay dead. Clear a garrison and
//   that town is free; clear all five and Verda is free.
//
// The guards and the soldiers are CombatBots in the shared ai::BattleRoyaleBots, added once a
// session and out of the game (hidden) until their part begins. Far from the player they sleep.
namespace outland::game::mafia {

enum class EndPhase : std::uint8_t { None, Siege, Landing, Occupied, Free };

struct Garrison {
    std::string town;
    Vector3 post{};
    std::vector<int> soldiers;      // bot ids
    bool freed{false};
};

class Endgame {
public:
    static constexpr int guard_count = 6;            // the parliament's guard
    static constexpr int squad = 5;                  // soldiers a garrison
    static constexpr float post_radius = 14;         // a garrison holds the square
    static constexpr float wake_radius = 380;        // further than this from the player they sleep
    static constexpr float hall_reach = 7;           // the guard down and the player this close to the hall: taken
    static constexpr double landing_delay = 45;      // seconds from the fall of the parliament (or the gang's win) to the landing
    static constexpr int liberation_pay = 100000;    // kronoj, when Verda is free

    // Adds the guard (at the parliament's steps, `steps`, facing `yaw`) and the garrisons (one at
    // each of `posts`, named `towns`), all out of the game. Call once a session, after the crews.
    void start(ai::BattleRoyaleBots& bots, Vector3 steps, Vector3 hall, float yaw, const std::vector<Vector3>& posts,
               const std::vector<std::string>& towns, std::uint64_t seed);
    void clear() { *this = {}; }
    [[nodiscard]] bool ready() const { return !garrisons_.empty(); }

    // The gang won: the Green Hand go for the parliament (siege = true), any other gang's win
    // brings the troops straight away.
    void gang_won(int gang, double now, ai::BattleRoyaleBots& bots);
    // Each frame. Returns a line for the screen when something happens ("" otherwise).
    std::string update(Vector3 player, bool player_alive, double now, ai::BattleRoyaleBots& bots);

    [[nodiscard]] EndPhase phase() const { return phase_; }
    [[nodiscard]] bool fighting() const { return phase_ == EndPhase::Siege || phase_ == EndPhase::Occupied; }
    [[nodiscard]] bool guard(int bot) const;
    [[nodiscard]] bool soldier(int bot) const;
    // Who these bots fight: the guard everyone but the guard (the player and the Krimuloj at the
    // steps), the soldiers everyone but the soldiers. Asked for any pair with one of them in it.
    [[nodiscard]] bool hostile(int a, int b) const;
    [[nodiscard]] bool ours(int bot) const { return guard(bot) || soldier(bot); }
    [[nodiscard]] const std::vector<Garrison>& garrisons() const { return garrisons_; }
    [[nodiscard]] int garrisons_left() const;
    [[nodiscard]] int guards_standing(ai::BattleRoyaleBots& bots) const;
    // Where the player should go: the hall (siege), the nearest garrison still held (occupied).
    [[nodiscard]] Vector3 objective(Vector3 player) const;
    [[nodiscard]] std::string objective_town(Vector3 player) const;
    [[nodiscard]] Vector3 hall() const { return hall_; }
    // Liberation pay, once (when Verda is free).
    int take_pay() { const int p = pay_; pay_ = 0; return p; }

    // Persistence (beside the territory): the phase and which towns are free.
    [[nodiscard]] std::string serialise() const;
    bool parse(const std::string& text, ai::BattleRoyaleBots& bots);

private:
    void deploy_guard(ai::BattleRoyaleBots& bots);
    void land(ai::BattleRoyaleBots& bots, double now);
    void stand_down(const std::vector<int>& ids, ai::BattleRoyaleBots& bots);

    EndPhase phase_{EndPhase::None};
    std::vector<int> guards_;
    std::vector<Garrison> garrisons_;
    Vector3 steps_{}, hall_{};
    float yaw_{0};
    double land_at_{0};
    int pay_{0};
    std::uint64_t seed_{0};
};

} // namespace outland::game::mafia
