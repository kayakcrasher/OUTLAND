#pragma once

#include "outland/game/GameMode.hpp"
#include "outland/game/ai/CombatBot.hpp"

namespace outland::game {

class HomeScreen {
public:
    GameMode update(
        int screen_width,
        int screen_height
    );

    void draw(
        int screen_width,
        int screen_height
    ) const;

    // Battle Royale opponent skill, cycled from the menu.
    [[nodiscard]] ai::BotLevel bot_level() const { return bot_level_; }

private:
    ai::BotLevel bot_level_{ai::BotLevel::Medium};
    GameMode selected_{
        GameMode::Home
    };
};

} // namespace outland::game
