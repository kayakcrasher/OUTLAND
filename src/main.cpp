#include "outland/engine/core/Game.hpp"

int main() {
    outland::engine::Game game;

    game.initialize();

    game.update(1.0 / 60.0);

    game.shutdown();

    return 0;
}
