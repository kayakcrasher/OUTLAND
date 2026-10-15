#pragma once
#include <raylib.h>
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

// The Battle Royale drop: a row of three helicopters flies a straight line across Verda, from one
// of eight directions, and everyone jumps from them. A jumper falls belly-down (steering and
// diving), opens a parachute (by choice, or automatically low down) and glides to the ground.
// Pure logic: no drawing, no world queries but the ground height passed in.
namespace outland::game::aircraft {

// The eight routes: 0 north to south, 1 south to north, 2 east to west, 3 west to east,
// 4 south-west to north-east, 5 north-east to south-west, 6 north-west to south-east,
// 7 south-east to north-west. (+z is south, +x east, as on the map.)
inline constexpr int heading_count = 8;
const char* heading_name(int heading);
Vector2 heading_direction(int heading); // unit (x, z)

struct FlightPath {
    int heading{0};
    Vector3 start{}, end{};
    Vector3 direction{};   // unit, level
    Vector3 side{};        // unit, level, to the right of travel
    float length{0};
    float speed{58};       // metres per second
    float altitude{650};   // world height of the middle helicopter
    float land_from{0}, land_to{0}; // the stretch of the line over the island (distances along it)

    [[nodiscard]] Vector3 point(float distance) const;
    [[nodiscard]] float duration() const { return length / speed; }
    // Distance along the line of the point nearest `p` (clamped to the line), and how far to the
    // side of it `p` lies (+ right).
    [[nodiscard]] float along(Vector3 p) const;
    [[nodiscard]] float offset(Vector3 p) const;
};

// A random route: a heading and a line a little off the island's centre, over the sea at both
// ends. `island_radius` reaches the coast.
FlightPath make_flight(std::uint64_t seed, float island_radius = 2300);

// The row of three (0 left, 1 middle, 2 right), side by side with the middle one a little ahead.
struct Formation {
    static constexpr int count = 3;
    static constexpr float spacing = 42;        // metres between neighbours
    static constexpr float trail = 14;          // the outer two sit this far behind
    static constexpr float linger = 25;         // seconds they fly on past the end of the line
};
// Where helicopter `index` is `seconds` after the start (before the start it waits at it).
Vector3 helicopter_position(const FlightPath& path, int index, float seconds);
// Its rotor phase and a gentle bob/roll, for drawing.
float helicopter_bob(int index, float seconds);
// Where the jumper leaves helicopter `index` (its side door).
Vector3 helicopter_door(const FlightPath& path, int index, float seconds);
// Seconds into the flight when everyone still aboard must jump (the last land under the line).
float last_jump_time(const FlightPath& path);

// ---------------------------------------------------------------------------------------------

enum class DiveStage { Aboard, Freefall, Canopy, Landed };

struct DiveInput {
    float forward{0}, strafe{0};  // -1..1, from the move stick or WASD
    float yaw{0};                 // where the player looks (radians, the game's yaw)
    float pitch{0};               // look pitch (radians, up positive): look down to dive
    bool deploy{false};           // open the parachute
};

// The player's jump, from the door to the ground. `position` is the player's (body centre, 1 m
// above the feet, as the rest of the game keeps it).
class Skydiver {
public:
    // Freefall: falling belly-down at ~45 m/s; pushing forward glides (slower fall, ~32 m/s
    // across), looking down and pushing forward dives (fast fall, steep).
    static constexpr float gravity = 9.8F;
    static constexpr float terminal = 46, glide_fall = 33, dive_fall = 68;
    static constexpr float glide_speed = 31, dive_speed = 22, drift_speed = 9;
    // The parachute opens on request after a moment of falling, and by itself this high above
    // the ground; it takes `open_seconds` to fill.
    static constexpr float auto_open = 150, min_open = 18, open_seconds = 1.7F, deploy_after = .9F;
    static constexpr float canopy_fall = 6.8F, canopy_speed = 11, fast_fall = 9.8F, fast_speed = 17.5F;
    static constexpr float brake_fall = 4.6F, brake_speed = 3.5F, turn_rate = 1.15F;

    void board() { *this = Skydiver{}; }
    void jump(Vector3 door, Vector3 carried_velocity, float yaw);
    // `ground` is the height of whatever is under the jumper (terrain, roofs).
    void update(float dt, const DiveInput& input, float ground);

    [[nodiscard]] DiveStage stage() const { return stage_; }
    [[nodiscard]] bool airborne() const { return stage_ == DiveStage::Freefall || stage_ == DiveStage::Canopy; }
    [[nodiscard]] Vector3 position() const { return position_; }
    [[nodiscard]] Vector3 velocity() const { return velocity_; }
    [[nodiscard]] float heading() const { return heading_; }     // the way the body faces
    [[nodiscard]] float canopy() const { return canopy_; }       // 0 packed .. 1 fully open
    [[nodiscard]] float body_pitch() const { return pitch_; }    // forward tilt (radians): ~1.4 belly-down
    [[nodiscard]] float bank() const { return bank_; }           // roll into turns (radians)
    [[nodiscard]] float swing() const { return swing_; }         // the pendulum under the canopy
    [[nodiscard]] float stage_time() const { return stage_time_; }
    [[nodiscard]] float height_above(float ground) const { return position_.y - 1 - ground; }
    [[nodiscard]] float landing_speed() const { return landing_speed_; } // vertical speed at touchdown
    [[nodiscard]] bool can_deploy() const { return stage_ == DiveStage::Freefall && stage_time_ >= deploy_after; }
    // True for the one update in which the canopy started to open / the jumper touched down.
    [[nodiscard]] bool opened_now() const { return opened_now_; }
    [[nodiscard]] bool landed_now() const { return landed_now_; }

private:
    DiveStage stage_{DiveStage::Aboard};
    Vector3 position_{}, velocity_{};
    float heading_{0}, canopy_{0}, pitch_{0}, bank_{0}, swing_{0}, swing_rate_{0};
    float stage_time_{0}, landing_speed_{0}, forward_speed_{0};
    bool opened_now_{false}, landed_now_{false};
};

// ---------------------------------------------------------------------------------------------

// A bot's jump, planned at the start of the match: it leaves the helicopter whose line passes
// nearest its landing spot and comes down there. Until `land_time` the bot is in the air.
struct BotJump {
    int bot{-1};
    int helicopter{1};
    Vector3 landing{};      // feet
    Vector3 door{};         // where it jumped from
    float jump_time{0}, open_time{0}, land_time{0};
};
struct BotInAir {
    Vector3 position{};     // body centre
    float yaw{0};
    float canopy{0};
    float body_pitch{0};
    DiveStage stage{DiveStage::Aboard};
};
std::vector<BotJump> plan_bot_jumps(const FlightPath& path, const std::vector<std::pair<int, Vector3>>& landings, std::uint64_t seed);
BotInAir bot_in_air(const BotJump& jump, float seconds);

// How far from the line a jumper can still reach the ground on a glide (bots land within it).
inline constexpr float drop_reach = 700;

} // namespace outland::game::aircraft
