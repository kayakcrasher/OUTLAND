#pragma once
#include <raylib.h>
#include <vector>

namespace outland::game::combat {

// What the player feels in a fight, independent of drawing: numbers that float off what they hit,
// a kill marker and streak, a red flash and an arrow toward whoever hurt them, and camera shake
// from hits and blasts. The renderer draws it; tests drive it directly.
struct DamageNumber {
    Vector3 position{};
    float amount{0};
    bool headshot{false}, kill{false};
    float life{0};
};

class CombatFeedback {
public:
    static constexpr float number_life = .9F, kill_life = .6F, streak_window = 4.0F;
    void reset();
    // The player hit something (pellets and bursts on one spot merge into one number).
    void hit(Vector3 position, float amount, bool headshot);
    // The player's hit killed it.
    void kill(Vector3 position);
    // The player took `amount` damage; `from` is where it came from when `directional`.
    void hurt(float amount, Vector3 from, bool directional);
    // A blast nearby: shake only (0..1 strength).
    void blast(float strength);
    void update(float dt);

    [[nodiscard]] const std::vector<DamageNumber>& numbers() const { return numbers_; }
    [[nodiscard]] float kill_marker() const { return kill_marker_; }
    [[nodiscard]] int streak() const { return streak_; }
    [[nodiscard]] float flash() const { return flash_; }        // red screen-edge flash, 0..1
    [[nodiscard]] float indicator() const { return indicator_; } // damage arrow, seconds left
    [[nodiscard]] Vector3 source() const { return source_; }
    // Camera offset in metres for this frame's shake.
    [[nodiscard]] Vector3 shake() const;
    [[nodiscard]] float shake_amount() const { return shake_; }

private:
    std::vector<DamageNumber> numbers_;
    float kill_marker_{0}, since_kill_{100}, flash_{0}, indicator_{0}, shake_{0};
    int streak_{0};
    double time_{0};
    Vector3 source_{};
};

} // namespace outland::game::combat
