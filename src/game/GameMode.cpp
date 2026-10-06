#include "outland/game/GameMode.hpp"

namespace outland::game {

const char* game_mode_name(
    const GameMode mode
) {
    switch (mode) {
    case GameMode::Home:
        return "HOME";

    case GameMode::BattleRoyale:
        return "BATTLE ROYALE";

    case GameMode::ZombieSurvival:
        return "ZOMBIE SURVIVAL";

    case GameMode::Explore:
        return "EXPLORE";

    case GameMode::DevLab:
        return "DEV LAB";
    }

    return "UNKNOWN";
}

} // namespace outland::game
