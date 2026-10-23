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

    // Squad play: while downable, a hit that would kill knocks the player down instead - still
    // alive, with `down_pool` to bleed out from; a hit while down kills. Healing does nothing
    // while down: only a teammate's get_up() brings the player back, with `health`.
    static constexpr float down_pool = 100.0F;
    void set_downable(bool downable) { downable_ = downable; }
    void get_up(float health);
    [[nodiscard]]
    bool downed() const { return downed_ && current_ > 0.0F; }

    [[nodiscard]]
    float current() const;

    [[nodiscard]]
    float maximum() const;

    [[nodiscard]]
    bool alive() const;

private:
    float maximum_;
    float current_;
    bool downable_{false}, downed_{false};
};

}
