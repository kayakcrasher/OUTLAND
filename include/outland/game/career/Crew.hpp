#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::ai { class BattleRoyaleBots; class BotDrivers; }
namespace outland::game::vehicles { class VehicleSystem; }

// The player's own people (Explore).
//
// - A gang member recruits up to four of the gang's foot soldiers: USE beside one ("Recruit
//   him"). They are the gang's own Krimuloj (game::mafia lends them), in its colours.
// - A serving officer gets a partner at any police station's front desk: one officer who stays
//   with them on and off the beat.
//
// They follow the player on foot and fight whoever the player's side fights. When the player
// drives off, those close by get in too (they ride out of sight) and get out where the player
// does. And the player can ride with them: USE at a car's passenger door and one of them drives
// the player there - to the waypoint set on the map, or else the job in hand.
//
// They go when they fall (a fallen gang member is back at the hideout later, as any Krimulo),
// when the player leaves the gang or hands in the badge, or when the player dies.
namespace outland::game::career {

class Crew {
public:
    static constexpr int max_gang = 4;
    static constexpr float follow_radius = 4.5F;    // they keep about this close
    static constexpr float board_radius = 35;       // close enough to get in when the player drives off
    static constexpr float catch_up = 90;           // left further behind than this: they catch up out of sight

    // The partner (added once a session, after the Mafio's crews): out of the game until assigned.
    void start(ai::BattleRoyaleBots& bots, Vector3 park, std::uint64_t seed);
    void clear() { members_.clear(); partner_ = -1; partner_on_ = false; aboard_.clear(); ride_car_.clear(); ride_driver_ = -1; }

    // A foot soldier of the player's gang joins. False when the crew is full or he is in it.
    bool recruit(int bot);
    // The desk gives the officer a partner: on their feet at `at`. False when there already is one.
    bool assign_partner(Vector3 at, float yaw, ai::BattleRoyaleBots& bots, double now);
    // Everyone goes: the partner off duty, the gang members back to the Mafio (returned).
    std::vector<int> dismiss(ai::BattleRoyaleBots& bots);
    // Just the partner (the badge handed in).
    void dismiss_partner(ai::BattleRoyaleBots& bots);

    struct Frame {
        Vector3 player{};          // feet
        bool alive{true};
        std::string driving;       // the car the player drives ("" on foot)
        Vector3 car{};             // ...where it is
        float car_yaw{0};
        bool riding{false};        // the player rides with the crew (the ride moves them)
    };
    // Every frame: keep up, get in and out with the player. Returns the gang members who fell
    // (to hand back to the Mafio), and sets `said` when something should be on the screen.
    std::vector<int> update(const Frame& frame, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region, std::string& said);

    // The player at a passenger door: the nearest member (within `reach` of the car) drives the
    // car `vehicle` to `goal`, with the player and the others aboard. Returns the driver (-1: none).
    int ride(const std::string& vehicle, Vector3 car, Vector3 goal, ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers,
             vehicles::VehicleSystem& cars, world::VerdaRegion& region, float reach = 40);
    // The ride: the car, its driver, and whether it is still going.
    [[nodiscard]] const std::string& ride_car() const { return ride_car_; }
    [[nodiscard]] int ride_driver() const { return ride_driver_; }
    [[nodiscard]] bool riding(const ai::BotDrivers& drivers) const;
    // Ends the ride where the car is (USE while riding, or the ride is over): everyone out.
    void end_ride(ai::BattleRoyaleBots& bots, ai::BotDrivers& drivers, vehicles::VehicleSystem& cars, const world::VerdaRegion& region);

    [[nodiscard]] const std::vector<int>& members() const { return members_; }
    // Who is in the car with the player at the wheel (they shoot from the windows).
    [[nodiscard]] const std::vector<int>& aboard_now() const { return aboard_; }
    [[nodiscard]] int partner() const { return partner_on_ ? partner_ : -1; }
    [[nodiscard]] int partner_bot() const { return partner_; }
    [[nodiscard]] bool member(int bot) const;
    // Everyone with the player (gang members and the partner), and how many.
    [[nodiscard]] std::vector<int> everyone() const;
    [[nodiscard]] int size() const { return static_cast<int>(everyone().size()); }
    [[nodiscard]] bool aboard(int bot) const;

private:
    void get_out(Vector3 car, float yaw, ai::BattleRoyaleBots& bots, const world::VerdaRegion& region);
    std::vector<int> members_;
    int partner_{-1};
    bool partner_on_{false};
    std::vector<int> aboard_;          // riding along while the player drives
    std::string driving_;              // the car the player drove last frame
    Vector3 last_car_{};
    float last_yaw_{0};
    std::string ride_car_;
    int ride_driver_{-1};
    std::uint64_t seed_{0};
};

} // namespace outland::game::career
