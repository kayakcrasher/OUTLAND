#include "outland/engine/core/Game.hpp"

#include <iostream>

namespace outland::engine {

Game::Game() = default;

void Game::initialize() {
    std::cout << "================================\n";
    std::cout << "          O U T L A N D         \n";
    std::cout << "================================\n";

    world_.load_training_arena();

    std::cout << "Loaded: "
              << world_.map_name()
              << '\n';

    std::cout << "Player health: "
              << player_.health().current()
              << '\n';

    running_ = true;
}

void Game::update(double delta_seconds) {
    if (!running_) {
        return;
    }

    time_.update(delta_seconds);
}

void Game::shutdown() {
    running_ = false;

    std::cout << "OUTLAND shutdown complete.\n";
}

bool Game::running() const {
    return running_;
}

}
