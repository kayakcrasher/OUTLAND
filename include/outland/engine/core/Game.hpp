#pragma once

#include "outland/engine/core/GameTime.hpp"
#include "outland/game/player/Player.hpp"
#include "outland/world/World.hpp"

namespace outland::engine {

class Game {
public:
    Game();

    void initialize();
    void update(double delta_seconds);
    void shutdown();

    [[nodiscard]]
    bool running() const;

private:
    bool running_{false};

    GameTime time_;
    game::Player player_;
    world::World world_;
};

}
