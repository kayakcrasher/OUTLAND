#include "outland/game/combat/Health.hpp"

#include <algorithm>

namespace outland::game {

Health::Health(float maximum)
    : maximum_(std::max(1.0F, maximum)),
      current_(maximum_) {
}

void Health::damage(float amount) {
    if (amount <= 0.0F) {
        return;
    }

    current_ = std::max(0.0F, current_ - amount);
}

void Health::heal(float amount) {
    if (amount <= 0.0F) {
        return;
    }

    current_ = std::min(maximum_, current_ + amount);
}

void Health::reset() {
    current_ = maximum_;
}

float Health::current() const {
    return current_;
}

float Health::maximum() const {
    return maximum_;
}

bool Health::alive() const {
    return current_ > 0.0F;
}

}
