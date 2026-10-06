#pragma once

#include "outland/game/GameMode.hpp"

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

private:
    GameMode selected_{
        GameMode::Home
    };
};

} // namespace outland::game
