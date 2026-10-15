#pragma once
#include "outland/game/life/LifeTypes.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::vehicles { class VehicleSystem; }
namespace outland::game::ai { class BattleRoyaleBots; class BotDrivers; }
namespace outland::characters { class NpcSystem; }

// Paul Acosta, "the walking man" (Explore). He lives in a trailer at Ruldomaro, drives his
// hatchback in every morning and leaves it on the shoulder just outside Verda City's limits (the
// Green Core's edge), then walks into town: the casino's poker table until mid-afternoon, then the
// bar till late. Some nights he walks back to the car and drives home; some nights he has had too
// many and stumbles home on foot, weaving down the road to the trailer park (the car stays out
// there till morning, so he walks out to it).
//
// He carries a pistol and never starts trouble. He shoots back in self defence: hurt him, shoot
// near him, or keep a gun aimed at him close up after he has warned you, and he draws and fights
// until it is over. Killed, he is back home the next morning.
//
// His body is a CombatBot in the shared ai::BattleRoyaleBots (so he is drawn, hit and fights like
// the Krimuloj). While calm he is asleep there and walked by this schedule; the Mafio's hostility
// rule asks hostile_to() about him.
namespace outland::game::life {

enum class PaulDoing : std::uint8_t {
    Home, DrivingIn, WalkingIn, Gambling, ToBar, Drinking, ToCar, DrivingHome, WalkingHome, Stumbling, Defending, Dead
};
const char* paul_doing_name(PaulDoing doing);

struct WalkingManFrame {
    float dt{0};
    double now{0};             // match seconds (the hostility clock)
    WorldClock clock{};        // island time (the schedule)
    Vector3 player_feet{};
    bool player_alive{true}, player_in_vehicle{false};
    bool player_fired{false};  // a round left the player's gun this frame
    bool player_aiming{false}; // aiming down the sights
    Vector3 aim_origin{}, aim_direction{0, 0, 1};
    std::function<bool(Vector3, Vector3)> line_of_sight; // optional
};

class WalkingMan {
public:
    static constexpr const char* name = "Paul Acosta";
    static constexpr const char* nickname = "the walking man";
    static constexpr const char* character = "character_27_hm";
    // His car: the hatchback by his trailer at Ruldomaro (nobody else drives it).
    static constexpr std::string_view car_marker = "vehicle_spawn_hatchback_ruldomaro_1";
    static constexpr float walk_speed = 1.45F, stumble_speed = 1.1F;
    // Walks run at real speed within `watch_radius` of the player; further away `unseen_pace`
    // times faster, so a walk across town fits the island's quick clock (an hour every 2 minutes).
    static constexpr float watch_radius = 250, unseen_pace = 6;
    static constexpr float warn_seconds = .8F, aim_seconds = 2.6F, aim_range = 16;
    static constexpr float angry_seconds = 40;
    static constexpr float drive_watch = 230; // the player this close: he really drives (else the car is moved)

    struct Plan {
        Vector3 home_door{}, home{};          // his trailer
        Vector3 car_home{};  float car_home_yaw{0};
        Vector3 parking{};   float parking_yaw{0}; // outside the city limits
        Vector3 casino_door{}, table{};  float table_yaw{0};
        Vector3 bar_door{}, bar{};       float bar_yaw{0};
        std::string bar_name{"the bar"};
        std::string car;                      // runtime vehicle id ("" if the car is missing)
    };

    // Finds his trailer, car, the casino and a bar, and puts him where the clock says. Needs the
    // Mafio's casino (Verda City's poker table); returns false (and stays away) without one.
    bool start(const std::vector<Place>& places, const world::VerdaRegion& region, Vector3 casino_door, Vector3 casino_table,
               ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, std::uint64_t seed, WorldClock clock);
    void clear() { bot_ = -1; active_ = false; }
    [[nodiscard]] bool active() const { return active_; }

    void update(const WalkingManFrame& frame, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    // After BattleRoyaleBots::sync_bodies: his body holsters the pistol while he is calm.
    void dress(characters::NpcSystem& npcs) const;

    // The player hurt him (or shot through his windscreen).
    void provoke(int bot, double now);
    [[nodiscard]] bool angry(double now) const { return angry_until_ > now; }
    // Mafio's hostility rule for anyone outside the gangs: he fights the player only when angry.
    [[nodiscard]] bool hostile_to(int a, int b, double now) const;

    [[nodiscard]] int bot() const { return bot_; }
    [[nodiscard]] PaulDoing doing() const { return doing_; }
    [[nodiscard]] bool drunk() const { return drunk_; }
    [[nodiscard]] const Plan& plan() const { return plan_; }
    [[nodiscard]] Vector3 position(const ai::BattleRoyaleBots& bots) const;
    // "gambling at the casino", "stumbling home drunk", ...
    [[nodiscard]] std::string activity() const;
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }
    // Tonight's choice (seeded per day): about every other night he drinks too much.
    [[nodiscard]] bool drunk_on(int day) const;

    // The schedule's hours (also used by the tests).
    static constexpr float leave_hour = 8.5F, bar_hour = 15, closing_hour = 22;

private:
    void say(std::string text, float seconds = 5) { message_ = std::move(text); message_time_ = seconds; }
    void walk(std::vector<Vector3> legs, PaulDoing doing, const world::VerdaRegion& region);
    void walk_route(Vector3 from, Vector3 to, std::vector<Vector3> extra, PaulDoing doing, const world::VerdaRegion& region);
    bool car_usable(const vehicles::VehicleSystem& cars, const world::VerdaRegion& region) const;
    Vector3 car_position(const vehicles::VehicleSystem& cars) const;
    void settle(PaulDoing doing, Vector3 at, float yaw, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region);
    void resync(const WalkingManFrame& frame, ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    void next_stage(const WalkingManFrame& frame, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
                    vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    void drive(Vector3 goal, float goal_yaw, PaulDoing doing, const WalkingManFrame& frame, ai::BattleRoyaleBots& bots,
               ai::BotDrivers& drivers, vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    bool threatened(const WalkingManFrame& frame, Vector3 feet);

    bool active_{false};
    int bot_{-1};
    std::uint64_t seed_{1};
    Plan plan_;
    world::roads::RoadGraph roads_;
    PaulDoing doing_{PaulDoing::Home};
    // A walk: the path and how far along it he is.
    std::vector<Vector3> path_;
    std::vector<float> lengths_;
    float progress_{0}, pause_{0}, sway_{0};
    bool drunk_{false};
    int day_{-1};             // the day the current outing started
    float wake_hour_{leave_hour};
    double last_minutes_{-1};
    bool riding_{false};
    double angry_until_{-1e9}, last_threat_{-1e9};
    float aimed_{0};
    bool warned_{false};
    double died_at_{-1};
    std::string message_;
    float message_time_{0};
};

} // namespace outland::game::life
