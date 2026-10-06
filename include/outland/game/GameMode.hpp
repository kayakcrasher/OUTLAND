#pragma once

namespace outland::game {

enum class GameMode {
    Home,
    BattleRoyale,
    ZombieSurvival,
    Explore,
    DevLab
};

const char* game_mode_name(
    GameMode mode
);

} // namespace outland::game
