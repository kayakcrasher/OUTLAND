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
    bool remote{false};   // thrown by another player (LAN): it hurts us, but its hits on others are theirs to send
    bool rocket{false};   // a rocket: flies straight, goes off on whatever it hits
    bool friendly{false}; // a teammate's (LAN, playing as a side): it does not hurt us
};
struct Explosion {
    Vector3 position{};
    float age{0};
    bool rocket{false};
};
struct GrenadeWorld {
    // First solid contact on the segment a -> b: true with the point and surface normal.
    std::function<bool(Vector3 a, Vector3 b, Vector3& point, Vector3& normal)> collide;
    // What a rocket hits (people and cars as well as walls); `collide` when unset.
    std::function<bool(Vector3 a, Vector3 b, Vector3& point, Vector3& normal)> rocket_collide;
};

class Grenades {
public:
    static constexpr float fuse_seconds = 2.8F, throw_speed = 17.0F, cooldown_seconds = .9F;
    static constexpr float blast_radius = 8.5F, lethal_radius = 2.5F, max_damage = 160.0F;
    static constexpr float explosion_life = 1.4F;
    // Rockets: a bigger blast, a straight flight that drops only a little, gone after rocket_life.
    static constexpr float rocket_blast_radius = 10.0F, rocket_lethal_radius = 3.0F, rocket_max_damage = 220.0F;
    static constexpr float rocket_life = 5.0F, rocket_drop = 1.5F;
    // Damage at `distance` metres from the blast (before cover).
    static float damage_at(float distance);
    static float rocket_damage_at(float distance);
    static float blast_radius_of(bool rocket) { return rocket ? rocket_blast_radius : blast_radius; }
    static float damage_at(float distance, bool rocket) { return rocket ? rocket_damage_at(distance) : damage_at(distance); }

    void reset(int count) { live_.clear(); explosions_.clear(); exploded_.clear(); exploded_remote_.clear(); exploded_rocket_.clear(); exploded_friendly_.clear(); count_ = count; cooldown_ = 0; }
    void give(int count) { count_ += count; }
    // Grenades carried in the bag: the count follows it (a throw then takes one from the bag).
    void set_count(int count) { count_ = count < 0 ? 0 : count; }
    [[nodiscard]] int count() const { return count_; }
    [[nodiscard]] bool ready() const { return count_ > 0 && cooldown_ <= 0; }
    // Throw from `from` along `direction` (lobbed a little upward), carrying the thrower's velocity.
    bool throw_grenade(Vector3 from, Vector3 direction, Vector3 carried);
    void update(float dt, const GrenadeWorld& world);
    // A rocket from the launcher (or another player's, remote).
    void launch_rocket(Vector3 origin, Vector3 velocity, bool remote = false, bool friendly = false) { live_.push_back({origin, velocity, rocket_life, 0, remote, true, friendly}); }
    // Another player's throw, replayed here (LAN play); a teammate's is `friendly`.
    void spawn_remote(Vector3 origin, Vector3 velocity, bool friendly = false) { live_.push_back({origin, velocity, fuse_seconds, 0, true, false, friendly}); }

    [[nodiscard]] const std::vector<Grenade>& live() const { return live_; }
    [[nodiscard]] const std::vector<Explosion>& explosions() const { return explosions_; }
    // Blasts that went off during the last update.
    [[nodiscard]] const std::vector<Vector3>& exploded() const { return exploded_; }
    // For each of exploded(): another player's grenade.
    [[nodiscard]] const std::vector<bool>& exploded_remote() const { return exploded_remote_; }
    // For each of exploded(): a rocket (the bigger blast).
    [[nodiscard]] const std::vector<bool>& exploded_rocket() const { return exploded_rocket_; }
    // For each of exploded(): a teammate's (it spares us).
    [[nodiscard]] const std::vector<bool>& exploded_friendly() const { return exploded_friendly_; }

private:
    std::vector<Grenade> live_;
    std::vector<Explosion> explosions_;
    std::vector<Vector3> exploded_;
    std::vector<bool> exploded_remote_, exploded_rocket_, exploded_friendly_;
    int count_{0};
    float cooldown_{0};
};

} // namespace outland::game::combat
