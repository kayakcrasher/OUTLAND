#include "outland/engine/core/GameTime.hpp"

namespace outland::engine {

void GameTime::update(double delta_seconds) {
    if (delta_seconds < 0.0) {
        delta_seconds = 0.0;
    }

    delta_seconds_ = delta_seconds;
    total_seconds_ += delta_seconds;
}

double GameTime::delta_seconds() const {
    return delta_seconds_;
}

double GameTime::total_seconds() const {
    return total_seconds_;
}

}
