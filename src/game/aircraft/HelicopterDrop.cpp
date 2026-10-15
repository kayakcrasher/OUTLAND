#include "outland/game/aircraft/HelicopterDrop.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::aircraft {
namespace {
float unit(std::uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return static_cast<float>(state >> 40) / static_cast<float>(1ULL << 24);
}
float approach(float value, float target, float rate, float dt) { return value + (target - value) * (1 - std::exp(-rate * dt)); }
float smooth(float t) { t = std::clamp(t, 0.0F, 1.0F); return t * t * (3 - 2 * t); }
float wrap(float angle) { return std::remainder(angle, 2 * PI); }
Vector3 forward_of(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }
Vector3 right_of(float yaw) { return {-std::cos(yaw), 0, std::sin(yaw)}; }
} // namespace

const char* heading_name(int heading) {
    static constexpr std::array<const char*, heading_count> names{
        "NORTH TO SOUTH", "SOUTH TO NORTH", "EAST TO WEST", "WEST TO EAST",
        "SOUTH-WEST TO NORTH-EAST", "NORTH-EAST TO SOUTH-WEST", "NORTH-WEST TO SOUTH-EAST", "SOUTH-EAST TO NORTH-WEST"};
    return names[static_cast<std::size_t>(std::clamp(heading, 0, heading_count - 1))];
}

Vector2 heading_direction(int heading) {
    constexpr float d = .70710678F;
    static constexpr std::array<Vector2, heading_count> directions{
        Vector2{0, 1}, Vector2{0, -1}, Vector2{-1, 0}, Vector2{1, 0},
        Vector2{d, -d}, Vector2{-d, d}, Vector2{d, d}, Vector2{-d, -d}};
    return directions[static_cast<std::size_t>(std::clamp(heading, 0, heading_count - 1))];
}

Vector3 FlightPath::point(float distance) const {
    return Vector3Add(start, Vector3Scale(direction, distance));
}
float FlightPath::along(Vector3 p) const {
    return std::clamp((p.x - start.x) * direction.x + (p.z - start.z) * direction.z, 0.0F, length);
}
float FlightPath::offset(Vector3 p) const {
    return (p.x - start.x) * side.x + (p.z - start.z) * side.z;
}

FlightPath make_flight(std::uint64_t seed, float island_radius, int heading) {
    std::uint64_t state = seed * 0xD1B54A32D192ED03ULL + 0x8CB92BA72F3D8DD7ULL;
    FlightPath path;
    path.heading = static_cast<int>(unit(state) * heading_count) % heading_count;
    if (heading >= 0) path.heading = heading % heading_count;
    const auto d = heading_direction(path.heading);
    path.direction = {d.x, 0, d.y};
    path.side = {-d.y, 0, d.x};
    // Not always over the capital: up to 550 m to either side of it.
    const float offset = (unit(state) * 2 - 1) * 550;
    const float half = island_radius + 350; // start and finish out over the sea
    const Vector3 middle = Vector3Scale(path.side, offset);
    path.start = Vector3Subtract(middle, Vector3Scale(path.direction, half));
    path.end = Vector3Add(middle, Vector3Scale(path.direction, half));
    path.start.y = path.end.y = path.altitude;
    path.length = 2 * half;
    // The chord of the island under the line (a circle of island_radius about the origin).
    const float chord = std::sqrt(std::max(0.0F, island_radius * island_radius - offset * offset));
    path.land_from = std::clamp(half - chord, 0.0F, path.length);
    path.land_to = std::clamp(half + chord, 0.0F, path.length);
    return path;
}

std::vector<FlightPath> make_fleet(std::uint64_t seed, int count, float island_radius) {
    std::uint64_t state = seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;
    // The four lines (north-south, east-west and the two diagonals) in a random order, each
    // flown one way or the other; a fifth and later helicopter flies a line already used the
    // other way.
    std::array<int, 4> axes{0, 1, 2, 3};
    for (int i = 3; i > 0; --i) std::swap(axes[static_cast<std::size_t>(i)], axes[static_cast<std::size_t>(unit(state) * static_cast<float>(i + 1)) % static_cast<std::size_t>(i + 1)]);
    std::array<int, 4> sign{};
    for (auto& s : sign) s = unit(state) < .5F ? 0 : 1;
    std::vector<FlightPath> fleet;
    count = std::clamp(count, 1, heading_count);
    for (int i = 0; i < count; ++i) {
        const int axis = axes[static_cast<std::size_t>(i % 4)];
        const int way = (sign[static_cast<std::size_t>(i % 4)] + i / 4) % 2;
        auto path = make_flight(seed * 31 + static_cast<std::uint64_t>(i) * 7919 + 1, island_radius, axis * 2 + way);
        path.index = i;
        path.altitude += static_cast<float>(i) * 22; // never at the same height where lines cross
        path.start.y = path.end.y = path.altitude;
        fleet.push_back(path);
    }
    return fleet;
}

Vector3 helicopter_position(const FlightPath& path, float seconds) {
    const float t = std::max(0.0F, seconds);
    Vector3 at = path.point(path.speed * t);
    at.y = path.altitude + helicopter_bob(path.index, seconds);
    return at;
}

float helicopter_bob(int index, float seconds) {
    return .9F * std::sin(seconds * .9F + static_cast<float>(index) * 2.1F) + .4F * std::sin(seconds * 2.3F + static_cast<float>(index));
}

Vector3 helicopter_door(const FlightPath& path, float seconds) {
    return Vector3Add(Vector3Add(helicopter_position(path, seconds), Vector3Scale(path.side, 1.8F)), {0, -.3F, 0});
}

float last_jump_time(const FlightPath& path) {
    return std::max(5.0F, (path.land_to - 40) / path.speed);
}

float last_jump_time(const std::vector<FlightPath>& fleet) {
    float last = 0;
    for (const auto& path : fleet) last = std::max(last, last_jump_time(path));
    return last;
}

int nearest_flight(const std::vector<FlightPath>& fleet, Vector3 p) {
    int best = -1;
    float best_score = 1e30F;
    for (std::size_t i = 0; i < fleet.size(); ++i) {
        const auto& path = fleet[i];
        // Off the land stretch of a line counts as further (nobody rides past the last land).
        const float along = (p.x - path.start.x) * path.direction.x + (p.z - path.start.z) * path.direction.z;
        const float beyond = std::max({0.0F, path.land_from - along, along - path.land_to});
        const float score = std::hypot(path.offset(p), beyond);
        if (score < best_score) { best_score = score; best = static_cast<int>(i); }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------

void Skydiver::jump(Vector3 door, Vector3 carried, float yaw) {
    stage_ = DiveStage::Freefall;
    position_ = door;
    velocity_ = carried;
    heading_ = yaw;
    canopy_ = 0;
    pitch_ = .2F;
    bank_ = swing_ = swing_rate_ = 0;
    stage_time_ = 0;
    landing_speed_ = 0;
    forward_speed_ = Vector3Length({carried.x, 0, carried.z});
    opened_now_ = landed_now_ = false;
}

void Skydiver::update(float dt, const DiveInput& input, float ground) {
    opened_now_ = landed_now_ = false;
    if (!airborne() || !std::isfinite(dt) || dt <= 0) return;
    dt = std::min(dt, .1F);
    stage_time_ += dt;
    const float forward = std::clamp(input.forward, -1.0F, 1.0F), strafe = std::clamp(input.strafe, -1.0F, 1.0F);
    const float above = height_above(std::isfinite(ground) ? ground : -1e6F);

    if (stage_ == DiveStage::Freefall) {
        // Belly-down, facing where the player looks. Forward glides; forward while looking down
        // dives; back holds position in the air (slowest across, a normal fall).
        const float dive = std::clamp((-input.pitch - .35F) / .6F, 0.0F, 1.0F) * std::max(0.0F, forward);
        const float glide = std::max(0.0F, forward) * (1 - dive);
        float fall = terminal + (glide_fall - terminal) * glide;
        fall += (dive_fall - fall) * dive;
        heading_ += std::clamp(wrap(input.yaw - heading_), -3.0F * dt, 3.0F * dt);
        const float across = glide_speed * glide + dive_speed * dive - 6 * std::max(0.0F, -forward);
        const Vector3 wish = Vector3Add(Vector3Scale(forward_of(heading_), across), Vector3Scale(right_of(heading_), strafe * drift_speed));
        // Air resistance pulls the horizontal speed toward the wish; gravity against drag
        // brings the fall up to (or down to) its terminal speed without a jolt.
        velocity_.x = approach(velocity_.x, wish.x, 1.1F, dt);
        velocity_.z = approach(velocity_.z, wish.z, 1.1F, dt);
        const float down = -velocity_.y;
        if (down < fall) velocity_.y -= gravity * (1 - (down / fall) * (down / fall)) * dt * 1.25F;
        else velocity_.y = -approach(down, fall, 1.4F, dt);
        // From the door's upright tumble to belly-down; head-down in a dive.
        pitch_ = approach(pitch_, 1.35F + .45F * dive - .2F * glide, 2.2F, dt);
        bank_ = approach(bank_, -strafe * .45F, 3, dt);
        const bool asked = input.deploy && stage_time_ >= deploy_after && above > min_open;
        if (asked || above <= auto_open) {
            stage_ = DiveStage::Canopy;
            stage_time_ = 0;
            opened_now_ = true;
            forward_speed_ = Vector3DotProduct({velocity_.x, 0, velocity_.z}, forward_of(heading_));
        }
    } else {
        // Under the canopy: the 'chute fills over open_seconds and snatches the fall down to a
        // glide; it turns toward where the player looks (and with the stick), banking into the
        // turn; forward speeds it up and steepens it, back brakes.
        canopy_ = std::min(1.0F, canopy_ + dt / open_seconds);
        const float open = smooth(canopy_);
        const float fast = std::max(0.0F, forward), brake = std::max(0.0F, -forward);
        const float fall = canopy_fall + (fast_fall - canopy_fall) * fast + (brake_fall - canopy_fall) * brake;
        const float speed = canopy_speed + (fast_speed - canopy_speed) * fast + (brake_speed - canopy_speed) * brake;
        const float before = heading_;
        const float turn = std::clamp(wrap(input.yaw - heading_) * 1.6F, -turn_rate, turn_rate);
        heading_ += (turn + strafe * .55F) * dt * open;
        const float turned = wrap(heading_ - before) / dt;
        const float previous_speed = forward_speed_;
        forward_speed_ = approach(forward_speed_, speed, .9F + 1.8F * open, dt);
        const Vector3 along = Vector3Scale(forward_of(heading_), forward_speed_);
        velocity_.x = approach(velocity_.x, along.x, 1.5F + 2 * open, dt);
        velocity_.z = approach(velocity_.z, along.z, 1.5F + 2 * open, dt);
        const float down = -velocity_.y;
        velocity_.y = -approach(down, fall, .4F + 2.6F * open, dt);
        // Upright under the canopy, legs a little forward; bank into turns; the pendulum swings
        // back when the canopy bites and forward as it speeds up.
        pitch_ = approach(pitch_, -.08F, 2.4F, dt);
        bank_ = approach(bank_, std::clamp(-turned * .45F, -.5F, .5F), 2.5F, dt);
        const float surge = (forward_speed_ - previous_speed) / dt;
        swing_rate_ += (-swing_ * 3.2F - swing_rate_ * 1.1F + std::clamp(surge, -20.0F, 20.0F) * .05F) * dt;
        swing_ = std::clamp(swing_ + swing_rate_ * dt, -.6F, .6F);
    }
    position_ = Vector3Add(position_, Vector3Scale(velocity_, dt));
    if (std::isfinite(ground) && position_.y - 1 <= ground) {
        position_.y = ground + 1;
        landing_speed_ = -velocity_.y;
        velocity_ = {0, 0, 0};
        stage_ = DiveStage::Landed;
        stage_time_ = 0;
        canopy_ = 0;
        pitch_ = bank_ = swing_ = 0;
        landed_now_ = true;
    }
}

// ---------------------------------------------------------------------------------------------

namespace {
// A bot's fall: it leaves the door, speeds up to ~46 m/s, and opens its canopy 120 m up.
constexpr float bot_fall = 46, bot_open = 150, bot_canopy_fall = 7.0F, bot_speed_up = 4.5F;
float fallen(float t) { // metres dropped `t` seconds after the jump
    if (t < bot_speed_up) return .5F * (bot_fall / bot_speed_up) * t * t;
    return .5F * bot_fall * bot_speed_up + bot_fall * (t - bot_speed_up);
}
float time_to_fall(float metres) {
    const float accelerating = .5F * bot_fall * bot_speed_up;
    if (metres <= accelerating) return std::sqrt(2 * metres * bot_speed_up / bot_fall);
    return bot_speed_up + (metres - accelerating) / bot_fall;
}
}

std::vector<BotJump> plan_bot_jumps(const std::vector<FlightPath>& fleet, const std::vector<std::pair<int, Vector3>>& landings, std::uint64_t seed) {
    std::uint64_t state = seed * 0x9E3779B97F4A7C15ULL + 77;
    std::vector<BotJump> jumps;
    if (fleet.empty()) return jumps;
    for (const auto& [bot, landing] : landings) {
        BotJump jump;
        jump.bot = bot;
        jump.landing = landing;
        jump.helicopter = std::max(0, nearest_flight(fleet, landing));
        const auto& path = fleet[static_cast<std::size_t>(jump.helicopter)];
        const float last = last_jump_time(path);
        // Jump a little before passing the spot, as players do, keen ones early.
        const float lead = 60 + unit(state) * 160;
        jump.jump_time = std::clamp((path.along(landing) - lead) / path.speed, 2.5F + unit(state) * 4, last - 1);
        jump.door = helicopter_door(path, jump.jump_time);
        const float drop = std::max(0.0F, jump.door.y - 1 - (landing.y + bot_open));
        jump.open_time = jump.jump_time + time_to_fall(drop);
        jump.land_time = jump.open_time + std::max(4.0F, (std::min(bot_open, jump.door.y - 1 - landing.y)) / bot_canopy_fall);
        jumps.push_back(jump);
    }
    return jumps;
}

BotInAir bot_in_air(const BotJump& jump, float seconds) {
    BotInAir air;
    const Vector3 feet_from{jump.door.x, jump.door.y - 1, jump.door.z};
    if (seconds < jump.jump_time) { air.position = jump.door; air.stage = DiveStage::Aboard; return air; }
    if (seconds >= jump.land_time) {
        air.position = Vector3Add(jump.landing, {0, 1, 0});
        air.stage = DiveStage::Landed;
        return air;
    }
    const Vector3 across{jump.landing.x - feet_from.x, 0, jump.landing.z - feet_from.z};
    air.yaw = std::atan2(across.x, across.z);
    const float open_y = jump.landing.y + std::min(bot_open, feet_from.y - jump.landing.y);
    // Most of the distance is covered gliding in freefall, the rest under the canopy.
    float reach = 0, y = 0;
    if (seconds < jump.open_time) {
        const float f = (seconds - jump.jump_time) / std::max(.01F, jump.open_time - jump.jump_time);
        reach = .78F * f;
        y = std::max(open_y, feet_from.y - fallen(seconds - jump.jump_time));
        air.stage = DiveStage::Freefall;
        air.body_pitch = 1.35F;
    } else {
        const float f = (seconds - jump.open_time) / std::max(.01F, jump.land_time - jump.open_time);
        reach = .78F + .22F * f;
        y = open_y + (jump.landing.y - open_y) * f;
        air.stage = DiveStage::Canopy;
        air.canopy = std::min(1.0F, (seconds - jump.open_time) / Skydiver::open_seconds);
    }
    air.position = {feet_from.x + across.x * reach, y + 1, feet_from.z + across.z * reach};
    return air;
}

} // namespace outland::game::aircraft
