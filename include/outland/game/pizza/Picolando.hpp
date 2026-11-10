#pragma once
#include "outland/world/Picolando.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::characters { class NpcSystem; class CharacterRegistry; }
namespace outland::game::vehicles { class VehicleSystem; }

// Delivering for Picolando (Explore): the pizza chain with a pizzeria in Verda City, Roka, Espera
// and Suda Haveno (world::picolando, which says whose area is whose). Anyone but an officer on duty
// can sign on at any pizzeria's counter (USE at the door). Then the orders come one at a time,
// each from the pizzeria nearest to you: pull up at its lay-by (or walk in) and stop, the boxes
// come out; take them to the customer's door - on foot to the door, or pull up at the kerb right
// outside and stop - and you are paid on the doorstep: the delivery by the distance, and a tip if
// the pizza is still hot (on time). Picolando's own three drivers are on the road too (Fleet).
//
// The rules and the bookkeeping only: the game says where the player is and what they drive.
namespace outland::game::pizza {

struct Shop {
    world::picolando::Shop id{world::picolando::Shop::Verda};
    std::string name;
    Vector3 door{};       // at its counter (USE there to talk)
    Vector3 pickup{};     // the kerb of its lay-by
};
struct Customer {
    std::string name;     // "the Novak household"
    Vector3 door{};       // outside their front door
    int shop{-1};         // the pizzeria whose area they live in (an index into PizzaWorld::shops)
};
struct PizzaWorld {
    std::vector<Shop> shops;
    std::vector<Customer> customers;
    // Index of the pizzeria nearest `p` (-1 none).
    [[nodiscard]] int nearest_shop(Vector3 p) const;
};
// The pizzerias and the households on the map, each household given the pizzeria whose area it is in.
PizzaWorld make_world(const std::vector<world::picolando::Pizzeria>& pizzerias, std::vector<Customer> households);

enum class Stage : std::uint8_t { None, Pickup, Deliver };
struct Order {
    Stage stage{Stage::None};
    int shop{-1}, customer{-1};
    std::string shop_name, customer_name, pizzas;
    Vector3 from{}, to{};
    int pay{0};           // for the delivery (before the tip)
    float allowed{0};     // seconds from pick-up for it to arrive hot
    double picked{-1};    // when the boxes came out (-1 not yet)
    float work{0};        // seconds of handing over done
};
struct Frame {
    float dt{0};
    double now{0};
    Vector3 player{};     // feet
    bool driving{false};  // at the wheel of something
    float speed{0};       // its speed (m/s)
};
enum class Event : std::uint8_t { None, PickedUp, Delivered };

class Delivery {
public:
    static constexpr float counter_reach = 2.5F;     // at a pizzeria's counter, to talk
    static constexpr float pickup_reach = 10;        // pulled up in the lay-by (or on foot at the door)
    static constexpr float door_reach = 4.0F;        // on foot at the customer's door
    static constexpr float kerb_reach = 13;          // ...or stopped at the kerb right outside
    static constexpr float still = 1.2F;             // stopped (m/s)
    static constexpr float hand_over = 2.0F;         // seconds, at either end
    static constexpr int base_pay = 70;
    static constexpr float pay_per_km = 110;         // by the road (about 1.3 times as the crow flies)
    static constexpr int tip_low = 25, tip_high = 70;
    static constexpr float late_share = .75F;        // a cold pizza is paid this share, no tip
    static constexpr float grace = 45, allowed_speed = 8.5F;   // seconds to box it and set off; m/s on the way
    static constexpr double confirm_seconds = 5;

    // The pizzeria whose door `player` is at (-1 none).
    [[nodiscard]] int at_counter(Vector3 player, const PizzaWorld& world) const;
    // USE at a pizzeria's counter: signing on (the first order is boxed and handed over at once),
    // the order in hand, or handing in the cap (asked twice). `cop`: an officer on duty.
    std::string talk(int shop, bool cop, double now, const PizzaWorld& world);
    Event update(const Frame& frame, const PizzaWorld& world);

    [[nodiscard]] bool hired() const { return hired_; }
    [[nodiscard]] const Order& order() const { return order_; }
    [[nodiscard]] Vector3 target() const;
    [[nodiscard]] std::string title() const;       // "PICOLANDO - PICK UP" / "... - DELIVER"
    [[nodiscard]] std::string detail() const;
    // Seconds left for the pizza to arrive hot (negative: late; 0 before the pick-up).
    [[nodiscard]] float hot_left(double now) const;
    [[nodiscard]] int deliveries() const { return deliveries_; }
    [[nodiscard]] std::int64_t earned() const { return earned_; }
    int take_pay() { const int p = pay_; pay_ = 0; return p; }
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] float message_time() const { return message_time_; }
    void quit() { hired_ = false; order_ = {}; }

    void seed(std::uint64_t value) { rng_ = (value ^ 0x9e3779b97f4a7c15ULL) | 1; }
    [[nodiscard]] std::string serialise() const;
    bool parse(const std::string& text);

    // The order the shop `shop` would give next (a customer in its area, not the last one).
    Order next_order(int shop, const PizzaWorld& world);

private:
    float random();
    void say(std::string text, float seconds = 4) { message_ = std::move(text); message_time_ = seconds; }

    bool hired_{false};
    int deliveries_{0};
    std::int64_t earned_{0};
    Order order_;
    int pay_{0};
    int last_customer_{-1};
    double offer_until_{-1};
    std::uint64_t rng_{0x2545f4914f6cdd1dULL};
    std::string message_;
    float message_time_{0};
};

// Picolando's own drivers: three Kurieroj (two of Verda City's, one of Roka's) out on deliveries
// all day: from the pizzeria to a household in its area, a moment at the kerb while the driver
// runs the boxes to the door, back to the lay-by, and out again. Like the rigs they drive
// kinematically (VehicleSystem::glide) along the road, holding back for anything in the lane. A
// car that is taken or wrecked leaves the run.
struct Driver {
    std::string name, vehicle, character;
    int shop{-1};
    world::roads::Route route;
    float progress{0}, speed{0}, dwell{0}, waited{0}, passing{0};
    bool out{false};                 // on the way to a customer (else back to the pizzeria)
    bool running{false}, blocked{false};
    int customer{-1}, deliveries{0};
    Vector3 position{}, bay{};
    float yaw{0}, bay_yaw{0};
};
struct DriverSeat { std::string vehicle, character; };

class Fleet {
public:
    static constexpr float cruise = 13, corner_speed = 5, accel = 2.2F, brake = 4.5F;
    static constexpr float door_dwell = 12, shop_dwell = 18;
    // The three: (town id, car number) of the cars they take.
    static constexpr int size = 3;
    bool build(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars, const PizzaWorld& world,
               const characters::CharacterRegistry* characters, std::uint64_t seed);
    void start(world::VerdaRegion& region, vehicles::VehicleSystem& cars);
    void stop(world::VerdaRegion& region, vehicles::VehicleSystem& cars);
    void update(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& cars, Vector3 player, const characters::NpcSystem* npcs,
                const PizzaWorld& world);
    [[nodiscard]] std::vector<DriverSeat> seats() const;
    [[nodiscard]] const std::vector<Driver>& drivers() const { return drivers_; }
    [[nodiscard]] bool running() const { return running_; }
    // Whether `vehicle` is one of the drivers' (on a run).
    [[nodiscard]] bool driven(const std::string& vehicle) const;

private:
    bool send(Driver& d, const PizzaWorld& world);
    float random();
    world::roads::RoadGraph graph_;
    std::vector<Driver> drivers_;
    bool running_{false};
    std::uint64_t rng_{0x51ed270b2a1f7c3dULL};
};

} // namespace outland::game::pizza
