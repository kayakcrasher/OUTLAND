#pragma once
#include "outland/game/combat/Health.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <vector>

namespace outland::game::lan {

struct Remote;

// Squad revive (LAN Battle Royale with sides): while a teammate still stands, a hit that would
// kill knocks the player down instead (game::Health's down state). Down, the player crawls, can't
// shoot, and bleeds out over `bleed_seconds`; another hit finishes them. A teammate who presses
// USE beside them kneels and picks them up in `revive_seconds` - the bleeding stops meanwhile -
// and they get up with `get_up_health`. When nobody on the side is left standing, everyone down
// is out.
//
// Each player runs their own: the downed player's game keeps the bar (from the reviver's
// `reviving` slot in their state); the reviver's game only says who it is helping.
class SquadRevive {
public:
    static constexpr float bleed_seconds = 40, revive_seconds = 5, get_up_health = 35;
    static constexpr float reach = 2.2F;        // USE this close to a downed teammate
    static constexpr float keep_reach = 3.2F;   // ...and stay this close (their shown body lags a little)
    static constexpr float crawl = .22F;        // of walking speed, down
    static constexpr std::uint8_t nobody = 255;

    void reset() { *this = SquadRevive{}; }

    // The downed player's side, every frame (before the death checks): whether they can be
    // knocked down (a teammate stands), the bleeding, and the bar while `helped`. True on the
    // frame they get up.
    bool update(float dt, Health& health, bool squad_play, int standing_teammates, bool helped);
    [[nodiscard]] float progress() const { return progress_; }   // 0..1, being picked up
    [[nodiscard]] float bleed() const;                           // 0..1 of the down pool left

    // The reviver's side: USE pressed beside a downed teammate starts helping them (the nearest
    // within reach); it goes on while they stay close, and stops when they get up, are out, or
    // the reviver moves off, shoots, or presses USE again.
    void help(std::uint8_t slot) { helping_ = slot; }
    void stop_helping() { helping_ = nobody; }
    [[nodiscard]] std::uint8_t helping() const { return helping_; }

    // The nearest downed teammate within `within` of `feet` (nobody: none).
    static std::uint8_t downed_near(Vector3 feet, const std::vector<Remote>& remotes,
                                    const std::function<bool(std::uint8_t)>& teammate, float within = reach);
    // Someone standing on the side is helping `me` up, close enough.
    static bool helped_by(std::uint8_t me, Vector3 feet, const std::vector<Remote>& remotes,
                          const std::function<bool(std::uint8_t)>& teammate);

private:
    float progress_{0}, pool_{Health::down_pool};
    std::uint8_t helping_{nobody};
};

}
