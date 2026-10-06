#include "outland/game/player/Player.hpp"

namespace outland::game {

Player::Player() = default;

void Player::move(float x, float y, float z) {
    x_ += x;
    y_ += y;
    z_ += z;
}

float Player::x() const {
    return x_;
}

float Player::y() const {
    return y_;
}

float Player::z() const {
    return z_;
}

Health& Player::health() {
    return health_;
}

const Health& Player::health() const {
    return health_;
}

}
