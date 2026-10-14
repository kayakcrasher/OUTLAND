#pragma once
#include "outland/game/combat/WeaponDefinition.hpp"
#include <functional>

// What the player feels through the phone, as in PUBG Mobile: a kick for every shot of their own
// gun (light for the SMG, heavy for the shotgun and sniper), a jolt when they are hit, a thud
// for a hard landing, a buzz for a car crash in proportion to the impact, a rumble for a grenade
// going off nearby and a pulse for a kill. One vibration at a time: each frame the strongest
// request goes out, and a weaker one never cuts a stronger one short.
namespace outland::game::feel {

class Haptics {
public:
    using Sink = std::function<void(int milliseconds, float strength)>;
    explicit Haptics(Sink sink = {});

    void set_enabled(bool on) { enabled_ = on; }
    [[nodiscard]] bool enabled() const { return enabled_; }

    void shot(combat::WeaponId weapon);
    void hit_taken(float damage);
    void hit_given(bool kill);         // a kill (headshot or not); plain hits stay silent
    void crash(float impact);          // m/s into whatever the car hit
    void explosion(float distance);    // metres from the player
    void land(float speed);            // m/s falling when the feet touch down
    void update(float dt);             // sends this frame's strongest request

private:
    void request(int milliseconds, float strength);
    Sink sink_;
    bool enabled_{true};
    int pending_ms_{0};
    float pending_strength_{0};
    float busy_{0}, busy_weight_{0}; // what is still buzzing, and how hard
};

} // namespace outland::game::feel
