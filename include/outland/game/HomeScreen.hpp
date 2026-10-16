#pragma once

#include "outland/game/GameMode.hpp"
#include "outland/game/ai/CombatBot.hpp"
#include <unordered_set>

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

    static Rectangle mode_button(int index,int width,int height);
    static Rectangle difficulty_button(int width,int height);
    static Rectangle lan_button(int width,int height);
    // LAN PLAY was pressed (the LAN screen opens); read once.
    bool take_lan() { const bool lan=lan_; lan_=false; return lan; }

    // Battle Royale opponent skill, cycled from the menu.
    [[nodiscard]] ai::BotLevel bot_level() const { return bot_level_; }
    void set_bot_level(ai::BotLevel level) { bot_level_ = level; }

private:
    std::unordered_set<int> previous_;
    ai::BotLevel bot_level_{ai::BotLevel::Medium};
    bool lan_{false};
    GameMode selected_{
        GameMode::Home
    };
};

} // namespace outland::game
