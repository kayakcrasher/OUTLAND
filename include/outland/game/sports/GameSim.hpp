#pragma once
#include "outland/game/sports/League.hpp"
#include <cstdint>
#include <vector>

// A ball game played out between two rosters - nothing decided beforehand. Every pitch: in the
// zone or not (the pitcher's control, tiring with his pitch count), taken or swung at (the
// batter's eye), missed, fouled off or put in play (contact against stuff). Every ball in play:
// an exit speed, a launch angle and a direction; flown with drag (over the fence, off the wall,
// or down to the grass), or skipped along the ground. Every fielder: how far he has to run and how
// fast, so whether he gets there; his glove (errors) and his arm (the throw). Every runner: his
// time to the bag against the ball's - out or safe, an extra base taken or not. Double plays,
// sacrifice flies, the bullpen when the starter tires, extra innings with a runner on second
// (from the eighth), the walk-off. The same seed plays the same game.
namespace outland::game::sports {

struct SimulatedGame {
    std::vector<std::vector<Play>> halves;   // top of the first, bottom, ... (an unplayed bottom half absent)
};

SimulatedGame simulate_game(const Roster& home, const Roster& away, std::uint64_t seed, int innings);

// A roster for a club of `strength` (0..1), its players named.
Roster make_roster(float strength, std::uint64_t seed);

} // namespace outland::game::sports
