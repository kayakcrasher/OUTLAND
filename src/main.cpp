#include "outland/engine/core/Game.hpp"
#include "outland/engine/render/Renderer.hpp"

#include <iostream>

int main() {
    outland::engine::Game game;
    outland::engine::Renderer renderer;

    game.initialize();

    if (!renderer.initialize(
            1280,
            720
        )) {

        std::cerr
            << "OUTLAND renderer failed to initialize.\n";

        game.shutdown();

        return 1;
    }

    renderer.run();

    renderer.shutdown();

    game.shutdown();

    return 0;
}
