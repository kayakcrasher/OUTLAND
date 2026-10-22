#pragma once
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::vehicles { class VehicleSystem; }

// Battle Royale bots on the road. At the start of a match some bots dropped near a parked car
// take it and drive the roads in toward the zone. A driver gets out (and fights on foot) when it
// arrives, sees the player close by, gets stuck, or the car is wrecked; the zone's edge closing
// behind it sends it on. While driving the bot is "mounted": its car carries it and takes the
// bullets (shots through the windows hurt the driver).
namespace outland::game::ai {
class BattleRoyaleBots;

struct BotRide {
    int bot{-1};
    std::string vehicle;
    world::roads::Route route;
    float progress{0}, stuck{0}, reverse{0}, driven{0};
    int attempts{0};
    bool active{false};
    // A dispatched ride (Mafio hit squads): it heads for `goal`, may carry passengers, and only
    // stops for the player if `ambush` is set.
    Vector3 goal{};
    bool targeted{false}, ambush{true};
    std::vector<int> passengers;
};

// Drives `vehicle` along `route` by autopilot (pure pursuit, slower on bends and behind other cars,
// backing off when stuck): the bots' and the emergency services' road driving.
void steer_along(const world::roads::Route& route, float& progress, float& stuck, float& reverse, int& attempts,
                 std::string_view vehicle, vehicles::VehicleSystem& cars, float dt, float straight = 17, float bends = 7);

class BotDrivers {
public:
    // Puts up to `max_drivers` bots that dropped within 140 m of a running car into it. Returns
    // how many drive.
    int start(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
              std::uint64_t seed, float zone_radius, int max_drivers = 7, Vector2 zone_center = {0, 0});
    // Puts `crew` (the first one drives) into the nearest usable car within `reach` of `from` and
    // drives it by road to `goal`; everyone gets out there. Returns whether they set off.
    // `vehicle` set: that car only (even a reserved one).
    bool dispatch(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                  const std::vector<int>& crew, Vector3 from, Vector3 goal, float reach = 160, std::string_view vehicle = {});
    // Ends the ride carrying `bot` and puts everyone out beside the car (a pull-over).
    void halt(int bot, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    // Ends the ride carrying `bot` where it is: everyone aboard is off it (not placed anywhere).
    void cancel(int bot, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars);
    // Cars nobody else takes (Paul Acosta's): ids containing `marker`.
    void reserve(std::string marker) { reserved_.push_back(std::move(marker)); }
    [[nodiscard]] bool reserved(std::string_view vehicle) const;
    // Gets everyone out and hands the cars back (end of a match).
    void clear(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars);
    using Sight = std::function<bool(Vector3 from, Vector3 to)>;
    // `zone_radius` <= 0: no circle (Explore); rides end at their goal.
    void update(float dt, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                Vector3 player, bool player_alive, float zone_radius, const Sight& sight, Vector2 zone_center = {0, 0});
    // The ride carrying `bot` (driver or passenger), or nullptr.
    [[nodiscard]] const BotRide* ride_of(int bot) const;
    // The bot at the wheel of `vehicle`, or -1.
    [[nodiscard]] int driver_of(std::string_view vehicle) const;
    [[nodiscard]] int driving() const;
    [[nodiscard]] const std::vector<BotRide>& rides() const { return rides_; }
    // Where a ride heads for: toward the zone's centre, well inside the circle.
    static Vector3 zone_goal(Vector3 from, float zone_radius, Vector2 center = {0, 0});
private:
    void dismount(BotRide& ride, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    void steer(BotRide& ride, vehicles::VehicleSystem& cars, float dt);
    world::roads::RoadGraph roads_;
    std::vector<BotRide> rides_;
    std::vector<std::string> reserved_;
};
} // namespace outland::game::ai
