#pragma once
#include "outland/game/combat/CombatWorld.hpp"
#include <raylib.h>
#include <array>
#include <cstdint>
#include <string>

// The Policio's training camp: a fenced range outside Verda City where a recruit qualifies
// before the Chief gives them a badge and a car. Three drills, each started from the firing
// line with USE: silhouette targets pop up downrange one at a time and drop when hit (or when
// their time is up). Hit enough and the drill is qualified; qualify all three to graduate.
//
// The camp is placed at runtime on open, level ground (the game searches for it); its targets
// are hit by real bullets through CombatWorld (bind_drill).
namespace outland::game::career {

struct Drill {
    const char* name;
    const char* brief;
    float distance;    // metres downrange
    int count;         // targets shown
    float window;      // seconds each is up
    int pass;          // hits to qualify
    bool moving;       // the targets slide across the lanes
};
inline constexpr std::array<Drill, 3> drills{{
    {"PISTOL - 10 m", "Ten pop-ups at 10 metres, one at a time. Hit seven.", 10, 10, 2.6F, 7, false},
    {"RIFLE - 25 m", "Ten pop-ups at 25 metres, a little quicker. Hit seven.", 25, 10, 2.2F, 7, false},
    {"MOVERS - 15 m", "Eight targets sliding across the lanes at 15 metres. Hit five.", 15, 8, 3.4F, 5, true},
}};

struct CampTarget {
    Vector3 base{};          // world, on the ground
    float raise{0};          // 0 flat on the ground .. 1 standing
    bool up{false}, hit{false}, headshot{false};
};

enum class CampPhase : std::uint8_t { Idle, Countdown, Running, Result };

class TrainingCamp {
public:
    static constexpr float half_width = 20, depth = 38;   // the fenced range (local x, z)
    static constexpr float behind = 10;                   // fence behind the firing line
    static constexpr float line_radius = 5;               // the player must stay this close to the stand
    static constexpr int slots = 5;                       // target positions across the range
    static constexpr float countdown = 3, gap = .7F, rise_seconds = .25F, slide_span = 8, slide_speed = 3.2F;
    static constexpr float target_width = .6F, target_height = 1.7F, head_from = 1.42F;

    // `line` is the middle of the firing line (on the ground); `yaw` points downrange (degrees,
    // 0 = +z). `ground` gives the terrain height under a point.
    template <class Ground> void place(Vector3 line, float yaw, Ground&& ground) {
        line_ = line; yaw_ = yaw; placed_ = true;
        for (int i = 0; i < slots; ++i)
            for (std::size_t d = 0; d < drills.size(); ++d) {
                Vector3 p = local(slot_x(i), drills[d].distance);
                p.y = ground(p.x, p.z);
                slot_ground_[d][static_cast<std::size_t>(i)] = p.y;
            }
        ground_line_ = line.y;
    }
    [[nodiscard]] bool placed() const { return placed_; }
    [[nodiscard]] Vector3 line() const { return line_; }
    [[nodiscard]] float yaw() const { return yaw_; }
    // Local (x across, z downrange) to world, at the firing line's height.
    [[nodiscard]] Vector3 local(float x, float z) const;
    [[nodiscard]] bool at_line(Vector3 feet) const;

    // Start a drill (from the firing line). False if one is running.
    bool start(int drill);
    void abort(const char* why);
    // The first drill not yet qualified (the one USE starts), or 0 when all are.
    [[nodiscard]] int next_drill() const;
    void update(float dt, Vector3 player_feet, int shots);
    [[nodiscard]] combat::BulletHit trace(Vector3 a, Vector3 b) const;
    void hit(int target, bool headshot);

    [[nodiscard]] CampPhase phase() const { return phase_; }
    [[nodiscard]] int drill() const { return drill_; }
    [[nodiscard]] int hits() const { return hits_; }
    [[nodiscard]] int shown() const { return shown_; }
    [[nodiscard]] int shots() const { return shots_; }
    [[nodiscard]] float clock() const { return clock_; }
    [[nodiscard]] bool qualified(int drill) const { return passed_[static_cast<std::size_t>(drill)]; }
    [[nodiscard]] int best(int drill) const { return best_[static_cast<std::size_t>(drill)]; }
    [[nodiscard]] bool graduated() const { return passed_[0] && passed_[1] && passed_[2]; }
    // True once, the frame every drill becomes qualified.
    bool take_graduation() { const bool g = graduated_now_; graduated_now_ = false; return g; }
    void restore(bool all_passed) { passed_.fill(all_passed); }
    [[nodiscard]] const CampTarget& target() const { return target_; }
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }

private:
    static float slot_x(int i) { return (static_cast<float>(i) - (slots - 1) * .5F) * 3.5F; }
    void pop();
    void finish();
    void say(std::string text, float seconds = 5) { message_ = std::move(text); message_time_ = seconds; }
    float random();

    Vector3 line_{};
    float yaw_{0}, ground_line_{0};
    bool placed_{false};
    std::array<std::array<float, slots>, drills.size()> slot_ground_{};
    CampPhase phase_{CampPhase::Idle};
    int drill_{0}, hits_{0}, shown_{0}, shots_{0}, slot_{0};
    float clock_{0}, slide_{0}, slide_dir_{1};
    CampTarget target_;
    std::array<bool, drills.size()> passed_{};
    std::array<int, drills.size()> best_{};
    bool graduated_now_{false};
    std::string message_;
    float message_time_{0};
    std::uint64_t rng_{0x853c49e6748fea9bULL};
};

} // namespace outland::game::career
