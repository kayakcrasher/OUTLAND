#pragma once

#include "outland/game/combat/Health.hpp"

namespace outland::game {

class Player {
public:
    Player();

    void move(float x, float y, float z);

    [[nodiscard]]
    float x() const;

    [[nodiscard]]
    float y() const;

    [[nodiscard]]
    float z() const;

    Health& health();

    [[nodiscard]]
    const Health& health() const;

private:
    float x_{0.0F};
    float y_{0.0F};
    float z_{0.0F};

    Health health_{100.0F};
};

}
