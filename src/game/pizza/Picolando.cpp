#include "outland/game/pizza/Picolando.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace outland::game::pizza {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
// What they order (Picolando's menu, in Esperanto as on the boxes).
constexpr std::array<const char*, 10> menu{
    "a large Margerita", "two Kolbasaj picoj (pepperoni)", "a Kvar Fromaĝoj and garlic bread", "a Havaja (ham and pineapple)",
    "a family Picolando Speciala", "three Margeritaj for the kids", "a Legoma (vegetable) and a salad", "a Diabla, extra hot",
    "two Kvar Sezonoj and a bottle of cola", "a Fungoj kaj Ŝinko (mushroom and ham)"};
}

int PizzaWorld::nearest_shop(Vector3 p) const {
    int best = -1;
    float best_d = 1e18F;
    for (std::size_t i = 0; i < shops.size(); ++i) {
        const float d = flat(shops[i].door, p);
        if (d < best_d) { best_d = d; best = static_cast<int>(i); }
    }
    return best;
}

PizzaWorld make_world(const std::vector<world::picolando::Pizzeria>& pizzerias, std::vector<Customer> households) {
    PizzaWorld world;
    for (const auto& p : pizzerias) world.shops.push_back({p.shop, p.name, p.counter_point(), p.pickup()});
    for (auto& c : households) {
        const auto area = world::picolando::serving(c.door.x, c.door.z);
        c.shop = -1;
        for (std::size_t i = 0; i < world.shops.size(); ++i)
            if (world.shops[i].id == area) c.shop = static_cast<int>(i);
        if (c.shop >= 0) world.customers.push_back(std::move(c));
    }
    return world;
}

// ------------------------------------------------------------------ the player's deliveries

float Delivery::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

int Delivery::at_counter(Vector3 player, const PizzaWorld& world) const {
    for (std::size_t i = 0; i < world.shops.size(); ++i)
        if (flat(world.shops[i].door, player) < counter_reach && std::abs(world.shops[i].door.y - player.y) < 2.5F) return static_cast<int>(i);
    return -1;
}

Order Delivery::next_order(int shop, const PizzaWorld& world) {
    Order o;
    if (shop < 0 || shop >= static_cast<int>(world.shops.size())) return o;
    std::vector<int> mine;
    for (std::size_t i = 0; i < world.customers.size(); ++i) {
        const auto& c = world.customers[i];
        if (c.shop != shop || static_cast<int>(i) == last_customer_) continue;
        if (flat(c.door, world.shops[static_cast<std::size_t>(shop)].door) < 60) continue;   // (they'd walk round)
        mine.push_back(static_cast<int>(i));
    }
    if (mine.empty()) return o;
    const int pick = mine[std::min(mine.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(mine.size())))];
    const auto& s = world.shops[static_cast<std::size_t>(shop)];
    const auto& c = world.customers[static_cast<std::size_t>(pick)];
    o.stage = Stage::Pickup;
    o.shop = shop;
    o.customer = pick;
    o.shop_name = s.name;
    o.customer_name = c.name;
    o.pizzas = menu[std::min(menu.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(menu.size())))];
    o.from = s.pickup;
    o.to = c.door;
    const float road_km = flat(s.door, c.door) * 1.3F / 1000;
    o.pay = base_pay + static_cast<int>(std::lround(road_km * pay_per_km));
    o.allowed = grace + flat(s.door, c.door) * 1.4F / allowed_speed;
    return o;
}

std::string Delivery::talk(int shop, bool cop, double now, const PizzaWorld& world) {
    if (shop < 0 || shop >= static_cast<int>(world.shops.size())) return {};
    const auto& here = world.shops[static_cast<std::size_t>(shop)];
    if (!hired_) {
        if (cop) return "\"Not in uniform, officer - we can't have the Policio delivering pizza. Come back off duty.\"";
        Order o = next_order(shop, world);
        if (o.stage == Stage::None) return "\"Quiet night - no orders. Try another Picolando.\"";
        hired_ = true;
        offer_until_ = -1;
        order_ = o;
        // The first one's boxed and on the counter: it goes out with you now.
        order_.stage = Stage::Deliver;
        order_.picked = now;
        return "\"Welcome to " + here.name + "! Cap on - first order's hot: " + order_.pizzas + " for " + order_.customer_name +
               ". The Kurieroj out front are yours to take. Hot pizza, happy customer, fat tip.\"";
    }
    if (offer_until_ >= 0 && now <= offer_until_) {
        offer_until_ = -1;
        const int made = deliveries_;
        quit();
        return "\"Hand in the cap? Fair enough - " + std::to_string(made) + " deliveries, good work. Come back any time.\"";
    }
    offer_until_ = now + confirm_seconds;
    if (order_.stage == Stage::Pickup && order_.shop == shop) {
        order_.stage = Stage::Deliver;
        order_.picked = now;
        order_.work = 0;
        return "\"Here you go - " + order_.pizzas + " for " + order_.customer_name + ". Go go go!\" (USE again to hand in your cap)";
    }
    return "\"" + (order_.stage == Stage::Deliver ? "Those boxes are getting cold - " + order_.customer_name + " is waiting!"
                                                : "Your next order's at " + order_.shop_name + ".") + "\" (USE again to hand in your cap)";
}

Vector3 Delivery::target() const {
    return order_.stage == Stage::Pickup ? order_.from : order_.to;
}

std::string Delivery::title() const {
    if (!hired_) return {};
    return order_.stage == Stage::Pickup ? "PICOLANDO - PICK UP" : order_.stage == Stage::Deliver ? "PICOLANDO - DELIVER" : "PICOLANDO";
}

std::string Delivery::detail() const {
    switch (order_.stage) {
        case Stage::Pickup: return "Pull up at " + order_.shop_name + " and stop: " + order_.pizzas + " for " + order_.customer_name;
        case Stage::Deliver: return order_.pizzas + " to " + order_.customer_name + " - to the door, or stop at the kerb outside";
        default: return "Waiting for an order";
    }
}

float Delivery::hot_left(double now) const {
    if (order_.stage != Stage::Deliver || order_.picked < 0) return 0;
    return order_.allowed - static_cast<float>(now - order_.picked);
}

Event Delivery::update(const Frame& f, const PizzaWorld& world) {
    message_time_ = std::max(0.0F, message_time_ - f.dt);
    if (!hired_) return Event::None;
    if (order_.stage == Stage::None) {
        // Waiting for the next order (none came last time): the pizzeria nearest asks again.
        Order o = next_order(world.nearest_shop(f.player), world);
        if (o.stage != Stage::None) { order_ = o; say("ORDER UP at " + order_.shop_name + ": " + order_.pizzas + " for " + order_.customer_name); }
        return Event::None;
    }
    const bool stopped = std::abs(f.speed) < still;
    if (order_.stage == Stage::Pickup) {
        const float d = flat(f.player, order_.from);
        const bool foot_at_door = !f.driving && order_.shop >= 0 && order_.shop < static_cast<int>(world.shops.size()) &&
                                  flat(f.player, world.shops[static_cast<std::size_t>(order_.shop)].door) < counter_reach + 1;
        if ((d < pickup_reach && stopped) || foot_at_door) {
            order_.work += f.dt;
            if (order_.work >= hand_over) {
                order_.stage = Stage::Deliver;
                order_.picked = f.now;
                order_.work = 0;
                say("Boxes in - " + order_.pizzas + " for " + order_.customer_name + ". Hot for " + std::to_string(static_cast<int>(order_.allowed)) + " s!");
                return Event::PickedUp;
            }
        } else order_.work = 0;
        return Event::None;
    }
    // Delivering: to the door on foot, or stopped at the kerb right outside.
    const float d = flat(f.player, order_.to);
    if ((!f.driving && d < door_reach) || (f.driving && stopped && d < kerb_reach)) {
        order_.work += f.dt;
        if (order_.work < hand_over) return Event::None;
        const bool hot = static_cast<float>(f.now - order_.picked) <= order_.allowed;
        int paid = order_.pay;
        int tip = 0;
        if (hot) tip = tip_low + static_cast<int>(random() * static_cast<float>(tip_high - tip_low + 1));
        else paid = static_cast<int>(std::lround(static_cast<float>(paid) * late_share));
        pay_ += paid + tip;
        earned_ += paid + tip;
        ++deliveries_;
        last_customer_ = order_.customer;
        say(hot ? "Delivered hot! " + std::to_string(paid) + " kr and a " + std::to_string(tip) + " kr tip from " + order_.customer_name
                : "Delivered - but it's gone cold. " + std::to_string(paid) + " kr, no tip.", 5);
        // The next one: from the pizzeria nearest to here.
        Order o = next_order(world.nearest_shop(f.player), world);
        order_ = o;
        if (o.stage != Stage::None) message_ += "   ORDER UP at " + o.shop_name;
        return Event::Delivered;
    }
    order_.work = 0;
    return Event::None;
}

std::string Delivery::serialise() const {
    std::ostringstream out;
    out << "PICOLANDO 1\n" << (hired_ ? 1 : 0) << ' ' << deliveries_ << ' ' << earned_ << '\n';
    return out.str();
}

bool Delivery::parse(const std::string& text) {
    std::istringstream in(text);
    std::string token;
    int version = 0, hired = 0, deliveries = 0;
    std::int64_t earned = 0;
    if (!(in >> token >> version) || token != "PICOLANDO" || version != 1) return false;
    if (!(in >> hired >> deliveries >> earned) || deliveries < 0 || earned < 0) return false;
    hired_ = hired != 0;
    deliveries_ = deliveries;
    earned_ = earned;
    order_ = {};   // a fresh order comes from the nearest pizzeria
    return true;
}

// ------------------------------------------------------------------ Picolando's own drivers

float Fleet::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
}

bool Fleet::build(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars, const PizzaWorld& world,
                  const characters::CharacterRegistry* characters, std::uint64_t seed) {
    drivers_.clear();
    running_ = false;
    rng_ = (seed ^ 0x7f4a7c159e3779b9ULL) | 1;
    graph_.build(region);
    if (graph_.empty()) return false;
    struct Seat { const char* town; int car; const char* name; };
    constexpr std::array<Seat, size> seats{{{"capital_verda", 2, "Luko"}, {"capital_verda", 3, "Mira"}, {"west_roka", 2, "Tonio"}}};
    std::vector<std::string> bodies;
    if (characters)
        for (const auto* d : characters->pool(characters::CharacterPool::Civilian))
            if (d->id.find("police") == std::string::npos && d->id.find("doctor") == std::string::npos) bodies.push_back(d->id);
    for (std::size_t k = 0; k < seats.size(); ++k) {
        const std::string marker = std::string("vehicle_spawn_pizza_") + seats[k].town + "_" + std::to_string(seats[k].car);
        std::string vehicle;
        for (const auto& car : cars.vehicles()) if (car.id.ends_with(marker)) vehicle = car.id;
        if (vehicle.empty()) continue;
        Driver d;
        d.name = seats[k].name;
        d.vehicle = vehicle;
        if (!bodies.empty()) d.character = bodies[(seed * 131 + k * 7919 + 17) % bodies.size()];
        // Its pizzeria: the shop of that town.
        for (std::size_t s = 0; s < world.shops.size(); ++s)
            if (world::picolando::town_ids[static_cast<std::size_t>(world.shops[s].id)] == std::string_view(seats[k].town)) d.shop = static_cast<int>(s);
        if (d.shop < 0) continue;
        if (const auto* asset = cars.asset(region, vehicle)) { d.bay = asset->vehicle.home; d.bay_yaw = asset->vehicle.home_yaw; d.position = d.bay; d.yaw = d.bay_yaw; }
        drivers_.push_back(std::move(d));
    }
    return !drivers_.empty();
}

bool Fleet::send(Driver& d, const PizzaWorld& world) {
    // Out: to a household in its pizzeria's area, at the kerb nearest its door.
    for (int tries = 0; tries < 12; ++tries) {
        std::vector<int> mine;
        for (std::size_t i = 0; i < world.customers.size(); ++i)
            if (world.customers[i].shop == d.shop && static_cast<int>(i) != d.customer &&
                flat(world.customers[i].door, d.bay) > 80 && flat(world.customers[i].door, d.bay) < 1100) mine.push_back(static_cast<int>(i));
        if (mine.empty()) return false;
        const int pick = mine[std::min(mine.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(mine.size())))];
        const Vector3 kerb = graph_.nearest(world.customers[static_cast<std::size_t>(pick)].door).point;
        auto route = graph_.route(d.position, kerb);
        if (!route.complete || route.length < 30) continue;
        d.route = std::move(route);
        d.customer = pick;
        d.out = true;
        d.progress = 0;
        return true;
    }
    return false;
}

void Fleet::start(world::VerdaRegion& region, vehicles::VehicleSystem& cars) {
    running_ = !drivers_.empty();
    for (auto& d : drivers_) {
        // Each starts in its bay, off on its first run after a stagger.
        d.position = d.bay;
        d.yaw = d.bay_yaw;
        d.speed = 0;
        d.out = false;
        d.route = {};
        d.dwell = 4 + 12 * static_cast<float>(&d - drivers_.data());
        d.running = cars.glide(region, d.vehicle, d.position, d.yaw, 0, 0, 0);
    }
}

void Fleet::stop(world::VerdaRegion& region, vehicles::VehicleSystem& cars) {
    running_ = false;
    for (auto& d : drivers_) {
        cars.release(d.vehicle);
        d.running = false;
        if (const auto* asset = cars.asset(region, d.vehicle))
            if (!asset->vehicle.destroyed) cars.place(region, d.vehicle, asset->vehicle.home, asset->vehicle.home_yaw);
    }
}

bool Fleet::driven(const std::string& vehicle) const {
    for (const auto& d : drivers_) if (d.running && d.vehicle == vehicle) return true;
    return false;
}

std::vector<DriverSeat> Fleet::seats() const {
    std::vector<DriverSeat> out;
    for (const auto& d : drivers_) if (d.running && !d.character.empty()) out.push_back({d.vehicle, d.character});
    return out;
}

void Fleet::update(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& cars, Vector3 player, const characters::NpcSystem* npcs,
                   const PizzaWorld& world) {
    if (!running_ || !std::isfinite(dt) || dt <= 0) return;
    dt = std::min(dt, .1F);
    const auto* taken = cars.driver();
    for (auto& d : drivers_) {
        if (!d.running) continue;
        const auto* asset = cars.asset(region, d.vehicle);
        if (!asset || asset->vehicle.destroyed || (taken && taken->id == d.vehicle)) {
            cars.release(d.vehicle);
            d.running = false;
            continue;
        }
        // Standing: at a customer's kerb, or in the bay at the pizzeria.
        if (d.dwell > 0 || d.route.points.size() < 2) {
            d.dwell -= dt;
            d.speed = 0;
            if (d.dwell <= 0) {
                if (d.out) {
                    // Back to the pizzeria's lay-by, into its own bay.
                    auto route = graph_.route(d.position, d.bay);
                    if (route.complete) { d.route = std::move(route); d.out = false; d.progress = 0; }
                    else d.dwell = 5;
                } else if (!send(d, world)) d.dwell = 10;
            }
            cars.glide(region, d.vehicle, d.position, d.yaw, 0, 0, dt);
            continue;
        }
        const float left = d.route.length - d.progress;
        float limit = cruise;
        float near_yaw = 0, far_yaw = 0;
        world::roads::route_point(d.route, std::min(d.route.length, d.progress + 5), &near_yaw);
        world::roads::route_point(d.route, std::min(d.route.length, d.progress + 14 + d.speed * 2), &far_yaw);
        const float turn = std::abs(std::remainder(far_yaw - near_yaw, 360.0F));
        if (turn > 60) limit = std::min(limit, 3.5F);
        else if (turn > 20) limit = std::min(limit, corner_speed);
        limit = std::min(limit, std::sqrt(2 * brake * std::max(0.0F, left - .3F)));
        // Anything in the lane ahead: hold back.
        const Vector3 forward{std::sin(d.yaw * DEG2RAD), 0, std::cos(d.yaw * DEG2RAD)};
        const float reach = 4 + d.speed * d.speed / (2 * brake) + 8;
        float nearest = 1e9F;
        const auto look = [&](Vector3 p, float width) {
            const float x = p.x - d.position.x, z = p.z - d.position.z;
            const float ahead = x * forward.x + z * forward.z, lateral = std::abs(x * forward.z - z * forward.x);
            if (ahead > 2.5F && ahead < reach && lateral < width) nearest = std::min(nearest, ahead);
        };
        look(player, 1.8F);
        if (npcs) for (const auto& actor : npcs->actors())
            if (actor.active && actor.state != characters::NpcState::Dead && !actor.hidden) look(actor.position, 1.6F);
        for (const auto& other : cars.vehicles())
            if (other.id != d.vehicle && (d.passing <= 0 || std::abs(other.speed) > .3F)) look(other.position, std::abs(other.speed) > .3F ? 1.9F : 1.5F);
        d.blocked = nearest < 1e8F;
        d.waited = d.blocked && d.speed < .3F ? d.waited + dt : 0;
        if (d.waited > 12) { d.passing = 8; d.waited = 0; }
        d.passing = std::max(0.0F, d.passing - dt);
        if (d.blocked) limit = std::min(limit, std::sqrt(2 * brake * std::max(0.0F, nearest - 6)));
        if (d.speed < limit) d.speed = std::min(limit, d.speed + accel * dt);
        else d.speed = std::max(limit, d.speed - brake * 1.4F * dt);
        d.speed = std::max(0.0F, d.speed);
        d.progress = std::min(d.route.length, d.progress + d.speed * dt);
        // Pose: the axles on the route, the body between them.
        float front_yaw = 0;
        const Vector3 front = world::roads::route_point(d.route, std::min(d.route.length, d.progress + 1.3F), &front_yaw);
        const Vector3 rear = world::roads::route_point(d.route, std::max(0.0F, d.progress - 1.25F));
        if (flat(front, rear) > .3F) {
            // Eased toward the route (where two roads of different widths meet, the lane's offset
            // steps sideways: the car drifts across, it doesn't jump).
            const Vector3 target = Vector3Lerp(rear, front, 1.25F / 2.55F);
            const float target_yaw = std::atan2(front.x - rear.x, front.z - rear.z) * RAD2DEG;
            const float ease = std::min(1.0F, dt * 5);
            d.position = Vector3Lerp(d.position, target, ease);
            d.yaw += std::remainder(target_yaw - d.yaw, 360.0F) * ease;
        }
        const float steering = std::clamp(std::remainder(front_yaw - d.yaw, 360.0F), -30.0F, 30.0F);
        if (!cars.glide(region, d.vehicle, d.position, d.yaw, d.speed, steering, dt)) {
            cars.release(d.vehicle);
            d.running = false;
            continue;
        }
        if (d.route.length - d.progress < .4F && d.speed < .5F) {
            // Arrived: at the customer's kerb (the driver runs the boxes up), or home in the bay.
            if (d.out) { d.dwell = door_dwell; ++d.deliveries; }
            else d.dwell = shop_dwell;   // (nose in, in its bay: it turns out again next time)
            d.route = {};
            d.speed = 0;
        }
    }
}

} // namespace outland::game::pizza
