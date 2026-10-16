#pragma once

namespace outland::game {

// The player's health: 25% more than everyone else's 100.
inline constexpr float player_max_health = 125.0F;

class Health {
public:
    explicit Health(float maximum = 100.0F);

    void damage(float amount);
    void heal(float amount);
    void reset();

    [[nodiscard]]
    float current() const;

    [[nodiscard]]
    float maximum() const;

    [[nodiscard]]
    bool alive() const;

private:
    float maximum_;
    float current_;
};

}
