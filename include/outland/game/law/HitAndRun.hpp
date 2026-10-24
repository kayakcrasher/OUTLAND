#pragma once
#include <raylib.h>
#include <cmath>

// HIT AND RUN (Explore). A driver who hits a pedestrian has to pull over and wait for the
// ambulance and the police: stay near the scene (stopped, or out of the car) for `wait_seconds`
// and it was an accident. Drive off instead and it's a hit and run - wanted, one star - and if
// the Policio catch you for it, prison time is mandatory (`sentence` on top of the rest).
//
// An officer with the lights on is exempt: a pedestrian who doesn't get out of the way of a
// police car running with its lights is at fault (game::law, the Renderer's dodge).
//
// The rules only; the game says who was hit, where the driver is and whether they've stopped.
namespace outland::game::law {

class HitAndRun {
public:
    static constexpr float stay_radius = 35;     // waiting this close to the scene counts
    static constexpr float run_radius = 90;      // ...this far away and they ran
    static constexpr float wait_seconds = 15;    // the ambulance and the police take this long
    static constexpr float sentence = 90;        // mandatory seconds in prison if busted for it

    enum class Outcome { None, Stayed, Ran };

    // The player's vehicle struck a pedestrian at `where`. `exempt`: a police car with its lights on.
    // Returns whether the player now has to stop and wait.
    bool struck(Vector3 where, bool exempt) {
        if (exempt || pending_) return false;
        pending_ = true;
        scene_ = where;
        waited_ = 0;
        return true;
    }
    // Each frame while a hit is pending: Stayed once they waited it out, Ran once they left.
    Outcome update(Vector3 player, bool stopped, float dt) {
        if (!pending_) return Outcome::None;
        const float d = std::hypot(player.x - scene_.x, player.z - scene_.z);
        if (d > run_radius) { pending_ = false; wanted_ = true; return Outcome::Ran; }
        if (d < stay_radius && stopped) waited_ += dt;
        if (waited_ >= wait_seconds) { pending_ = false; return Outcome::Stayed; }
        return Outcome::None;
    }
    // Busted while wanted for a hit and run: the mandatory time (0 if not wanted for one).
    float busted() { const float extra = wanted_ ? sentence : 0; wanted_ = false; pending_ = false; return extra; }
    // The police gave up (no stars left): the hit and run is off the books.
    void lost_them() { wanted_ = false; }
    void clear() { *this = {}; }

    [[nodiscard]] bool pending() const { return pending_; }
    [[nodiscard]] bool wanted() const { return wanted_; }
    [[nodiscard]] Vector3 scene() const { return scene_; }
    [[nodiscard]] float waited() const { return waited_; }

private:
    bool pending_{false}, wanted_{false};
    Vector3 scene_{};
    float waited_{0};
};

} // namespace outland::game::law
