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
};

class BotDrivers {
public:
    // Puts up to `max_drivers` bots that dropped within 140 m of a running car into it. Returns
    // how many drive.
    int start(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
              std::uint64_t seed, float zone_radius, int max_drivers = 7);
    // Gets everyone out and hands the cars back (end of a match).
    void clear(BattleRoyaleBots& bots, vehicles::VehicleSystem& cars);
    using Sight = std::function<bool(Vector3 from, Vector3 to)>;
    void update(float dt, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region,
                Vector3 player, bool player_alive, float zone_radius, const Sight& sight);
    // The bot at the wheel of `vehicle`, or -1.
    [[nodiscard]] int driver_of(std::string_view vehicle) const;
    [[nodiscard]] int driving() const;
    [[nodiscard]] const std::vector<BotRide>& rides() const { return rides_; }
    // Where a ride heads for: toward the zone's centre, well inside the circle.
    static Vector3 zone_goal(Vector3 from, float zone_radius);
private:
    void dismount(BotRide& ride, BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region);
    void steer(BotRide& ride, vehicles::VehicleSystem& cars, float dt);
    world::roads::RoadGraph roads_;
    std::vector<BotRide> rides_;
};
} // namespace outland::game::ai
