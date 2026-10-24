// How Verda ends when the gangs win: the Green Hand's siege of the parliament, the Russian
// landing, the garrisons to clear, and the save.
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/mafia/Endgame.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace outland;
using game::mafia::EndPhase;
using game::mafia::Endgame;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
const std::vector<Vector3> posts{{0, 0, 0}, {600, 0, 0}, {0, 0, 600}};
const std::vector<std::string> towns{"Verda", "Espera", "Roka"};
void kill_all(const std::vector<int>& ids, game::ai::BattleRoyaleBots& bots) {
    for (const int id : ids) bots.damage_bot(id, 1000, {0, 0, 0}, 1);
}
}

int main() {
    game::ai::BattleRoyaleBots bots;
    Endgame end;
    const Vector3 steps{100, 0, 100}, hall{100, 0, 120};
    end.start(bots, steps, hall, 0, posts, towns, 7);
    check(end.ready() && end.garrisons().size() == 3, "three garrisons");
    check(bots.bots().size() == static_cast<std::size_t>(Endgame::guard_count + 3 * Endgame::squad), "the guard and the troops added");
    for (const auto& b : bots.bots()) check(bots.hidden(b.id()), "nobody shows before their part");
    check(end.phase() == EndPhase::None && !end.fighting(), "peace to begin with");
    const int soldier = end.garrisons()[0].soldiers[0];
    check(end.soldier(soldier) && !end.guard(soldier), "a soldier is a soldier");
    check(!end.hostile(soldier, game::ai::BattleRoyaleBots::player_id), "the troops fight nobody before they land");

    // Another gang wins: no siege, the troops land after the delay.
    {
        game::ai::BattleRoyaleBots other_bots;
        Endgame other;
        other.start(other_bots, steps, hall, 0, posts, towns, 9);
        other.gang_won(2, 10, other_bots);
        check(other.phase() == EndPhase::Landing, "any other gang's win: the landing");
        check(other.update({0, 0, 0}, true, 10 + Endgame::landing_delay - 1, other_bots).empty(), "not yet");
        const auto said = other.update({0, 0, 0}, true, 10 + Endgame::landing_delay + 1, other_bots);
        check(other.phase() == EndPhase::Occupied && said.find("RUSSIAN TROOPS") != std::string::npos, "they come ashore: " + said);
    }

    // The Green Hand win: the siege.
    end.gang_won(0, 100, bots);
    check(end.phase() == EndPhase::Siege && end.fighting(), "the Green Hand march on the parliament");
    check(end.guards_standing(bots) == Endgame::guard_count, "the guard on the steps");
    const int guard = [&] { for (const auto& b : bots.bots()) if (end.guard(b.id())) return b.id(); return -1; }();
    check(end.hostile(guard, game::ai::BattleRoyaleBots::player_id) && end.hostile(guard, 999), "the guard fights the player and the gang");
    check(!end.hostile(guard, guard + 1 < 1000 && end.guard(guard + 1) ? guard + 1 : guard), "the guard doesn't fight itself");
    check(!end.hostile(soldier, game::ai::BattleRoyaleBots::player_id), "the troops aren't here yet");
    check(end.objective({0, 0, 0}).z == hall.z, "the waypoint is the hall");
    // Into the hall with the guard still up: not yet.
    check(end.update(hall, true, 110, bots).empty() && end.phase() == EndPhase::Siege, "the guard still stands");
    std::vector<int> guards;
    for (const auto& b : bots.bots()) if (end.guard(b.id())) guards.push_back(b.id());
    kill_all(guards, bots);
    check(end.update({0, 0, 0}, true, 120, bots).empty(), "the guard down, but the hall not walked into");
    const auto fell = end.update(hall, true, 130, bots);
    check(fell.find("REGISTARO DE VERDA HAS FALLEN") != std::string::npos && end.phase() == EndPhase::Landing, "the parliament falls: " + fell);

    // The landing.
    end.update(hall, true, 130 + Endgame::landing_delay + 1, bots);
    check(end.phase() == EndPhase::Occupied && end.garrisons_left() == 3, "three towns occupied");
    check(!bots.hidden(soldier), "the troops show");
    check(end.hostile(soldier, game::ai::BattleRoyaleBots::player_id) && end.hostile(soldier, guard) && end.hostile(soldier, 999),
          "the troops fight anyone not in their uniform");
    check(!end.hostile(soldier, end.garrisons()[1].soldiers[0]), "...but not each other");
    check(end.objective_town({590, 0, 0}) == "Espera", "the waypoint: the nearest garrison");
    // Far from the player they sleep.
    end.update({0, 0, 0}, true, 200, bots);
    check(bots.asleep(end.garrisons()[1].soldiers[0]) && !bots.asleep(soldier), "the far garrisons sleep");
    kill_all(end.garrisons()[1].soldiers, bots);
    const auto freed = end.update({0, 0, 0}, true, 210, bots);
    check(freed.find("Espera IS FREE") != std::string::npos && end.garrisons_left() == 2, "a town freed: " + freed);

    // The save: phase and freed towns; the rest back at full strength.
    const auto saved = end.serialise();
    game::ai::BattleRoyaleBots fresh_bots;
    Endgame fresh;
    fresh.start(fresh_bots, steps, hall, 0, posts, towns, 7);
    check(fresh.parse(saved, fresh_bots), "parse: " + saved);
    check(fresh.phase() == EndPhase::Occupied && fresh.garrisons_left() == 2 && fresh.garrisons()[1].freed, "occupied, Espera free");
    check(!fresh_bots.hidden(fresh.garrisons()[0].soldiers[0]) && fresh_bots.hidden(fresh.garrisons()[1].soldiers[0]), "troops back where they hold");
    check(!fresh.parse("endgame 9 1 0", fresh_bots) && !fresh.parse("nonsense", fresh_bots), "bad saves refused");

    // The rest of them: Verda is free, and pays.
    kill_all(end.garrisons()[0].soldiers, bots);
    kill_all(end.garrisons()[2].soldiers, bots);
    const auto free = end.update({0, 0, 0}, true, 300, bots);
    check(end.phase() == EndPhase::Free && free.find("VERDA IS FREE") != std::string::npos, "liberation: " + free);
    check(end.take_pay() == Endgame::liberation_pay && end.take_pay() == 0, "the pay, once");
    check(!end.fighting() && !end.hostile(soldier, game::ai::BattleRoyaleBots::player_id), "peace");
    std::cout << "endgame: siege, landing, " << end.garrisons().size() << " garrisons freed, save ok\n";
    return 0;
}
