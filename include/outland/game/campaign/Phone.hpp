#pragma once
#include <raylib.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The phone (the campaign, in free play): Uncle Ron and Uncle Dario ring now and then, about
// what's going on - the races on the ring, church on Sunday, a feast tomorrow, what people are
// saying about the player, the badge or the family business - and most calls end with a place to
// go (a waypoint on the map). Each thing is said once; the calls are spaced out and wait while the
// player is busy (a race, a menu, a talk).
namespace outland::game::campaign {

struct PhoneContext {
    double played{0};              // seconds of free play this session
    int hour{12};
    bool saturday{false}, sunday{false};
    bool cop{false}, gang{false}, green_hand{false};
    bool owns_car{false};
    int races{0}, race_wins{0};
    std::string rumour;            // what people are saying about the player ("" nothing)
    int feast_tomorrow{-1};        // game::culture::feasts index, or -1
    bool busy{false};
    // Where the calls send the player (unset: no waypoint for that call).
    std::optional<Vector3> race_meet, orthodox_church, chapel, ron_home, hideout;
};

struct Call {
    std::string caller;            // "Uncle Ron"
    std::string topic;
    std::vector<std::string> lines;
    std::optional<Vector3> target;
    std::string target_name;       // "the race meet"
};

class Phone {
public:
    static constexpr float first_call = 240;      // seconds of free play before anyone rings
    static constexpr float gap = 540;             // between calls (plus a little)
    static constexpr float ring_seconds = 2.5F;
    static constexpr float line_seconds = 6.5F;

    void update(float dt, const PhoneContext& context);
    // The next look for something to say in `seconds` (testing; a fresh session waits first_call).
    void ring_in(float seconds) { wait_ = seconds; }
    // USE during a call: the next line (the last one hangs up). True if it did something.
    bool next();

    [[nodiscard]] bool ringing() const { return call_ && ring_ > 0; }
    [[nodiscard]] bool on_call() const { return call_.has_value(); }
    [[nodiscard]] const std::optional<Call>& call() const { return call_; }
    // The line on screen ("" while it rings or between calls).
    [[nodiscard]] std::string line() const;
    // Once each: the phone started ringing; a call ended (with where it sends the player).
    bool take_ring() { const bool r = rang_; rang_ = false; return r; }
    std::optional<Call> take_ended() { auto e = std::move(ended_); ended_.reset(); return e; }

    // The topics already talked about (saved with the campaign).
    [[nodiscard]] const std::vector<std::string>& done() const { return done_; }
    void set_done(std::vector<std::string> topics) { done_ = std::move(topics); }
    [[nodiscard]] bool said(const std::string& topic) const;
    // What would be said now, if anyone rang (tests and debugging).
    [[nodiscard]] std::optional<Call> choose(const PhoneContext& context) const;

private:
    void hang_up();
    std::optional<Call> call_, ended_;
    std::vector<std::string> done_;
    float ring_{0}, on_line_{0}, wait_{first_call};
    int line_{0};
    bool rang_{false};
    std::uint32_t seed_{0x51ed270bU};
};

} // namespace outland::game::campaign
