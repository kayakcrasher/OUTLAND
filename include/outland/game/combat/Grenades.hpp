#pragma once
#include <raylib.h>
#include <functional>
#include <vector>

namespace outland::game::combat {

// Frag grenades: thrown in an arc, bouncing off the ground and walls, exploding after their fuse.
// The blast is lethal close in and falls off to nothing at `blast_radius`; walls between the
// blast and a target stop it (the caller checks line of sight when it applies damage).
struct Grenade {
    Vector3 position{}, velocity{};
    float fuse{0};
    int bounces{0};
};
struct Explosion {
    Vector3 position{};
    float age{0};
};
struct GrenadeWorld {
    // First solid contact on the segment a -> b: true with the point and surface normal.
    std::function<bool(Vector3 a, Vector3 b, Vector3& point, Vector3& normal)> collide;
};

class Grenades {
public:
    static constexpr float fuse_seconds = 2.8F, throw_speed = 17.0F, cooldown_seconds = .9F;
    static constexpr float blast_radius = 8.5F, lethal_radius = 2.5F, max_damage = 160.0F;
    static constexpr float explosion_life = 1.4F;
    // Damage at `distance` metres from the blast (before cover).
    static float damage_at(float distance);

    void reset(int count) { live_.clear(); explosions_.clear(); exploded_.clear(); count_ = count; cooldown_ = 0; }
    void give(int count) { count_ += count; }
    // Grenades carried in the bag: the count follows it (a throw then takes one from the bag).
    void set_count(int count) { count_ = count < 0 ? 0 : count; }
    [[nodiscard]] int count() const { return count_; }
    [[nodiscard]] bool ready() const { return count_ > 0 && cooldown_ <= 0; }
    // Throw from `from` along `direction` (lobbed a little upward), carrying the thrower's velocity.
    bool throw_grenade(Vector3 from, Vector3 direction, Vector3 carried);
    void update(float dt, const GrenadeWorld& world);

    [[nodiscard]] const std::vector<Grenade>& live() const { return live_; }
    [[nodiscard]] const std::vector<Explosion>& explosions() const { return explosions_; }
    // Blasts that went off during the last update.
    [[nodiscard]] const std::vector<Vector3>& exploded() const { return exploded_; }

private:
    std::vector<Grenade> live_;
    std::vector<Explosion> explosions_;
    std::vector<Vector3> exploded_;
    int count_{0};
    float cooldown_{0};
};

} // namespace outland::game::combat
