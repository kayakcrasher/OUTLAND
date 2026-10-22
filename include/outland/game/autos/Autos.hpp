#pragma once
#include <raylib.h>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outland::world { struct VehiclePlacementState; }

// Verda's motor trade (Explore): Marnafto's filling stations, the Meĥanikejo workshops and the
// car lot in Verda City.
//
// - Marnafto: the island's own oil company. It works the offshore rigs (Platformoj Marnafto) out
//   in the sea, refines on the coast and sells at its stations: pull up at a pump and fill up.
// - Meĥanikejo: drive into a bay and the mechanic asks what you want: a new colour, a bigger
//   engine, bigger or smaller tyres, a front bumper (or a repair). Leave whenever you like.
// - Aŭtovendejo Verda: the car lot. Buy a car at the sales office; it is yours (it comes back
//   where you left it, with what you fitted, next time you play).
namespace outland::game::autos {

// What a mechanic has fitted to a car. Indices into the tables below; paint -1 is the factory
// colour, tires 1 the factory tyres.
struct Mods {
    int paint{-1}, engine{0}, tires{1}, bumper{0};
    friend bool operator==(const Mods&, const Mods&) = default;
};

struct Paint { const char* name; Color color; int price; };
struct Part { const char* name; const char* detail; int price; };

inline constexpr int paint_price = 1200;
inline constexpr std::array<Paint, 10> paints{{
    {"Verda green", {38, 132, 74, 255}, paint_price},
    {"Racing red", {176, 28, 30, 255}, paint_price},
    {"Sea blue", {28, 82, 160, 255}, paint_price},
    {"Midnight black", {26, 26, 30, 255}, paint_price},
    {"Pearl white", {236, 236, 230, 255}, paint_price},
    {"Sunflower yellow", {232, 188, 36, 255}, paint_price},
    {"Rig orange", {226, 110, 30, 255}, paint_price},
    {"Gunmetal grey", {92, 98, 104, 255}, paint_price},
    {"Plum purple", {104, 48, 120, 255}, paint_price},
    {"Sky teal", {40, 160, 164, 255}, paint_price},
}};
inline constexpr std::array<Part, 3> engines{{
    {"Factory engine", "as it left the works", 0},
    {"Tuned engine", "+30% power", 2500},
    {"V8 big block", "+65% power, +15% top speed", 7000},
}};
inline constexpr std::array<Part, 4> tires{{
    {"Low-profile tyres", "smaller wheels, sits low, quick to turn", 800},
    {"Factory tyres", "as it left the works", 0},
    {"All-terrain tyres", "bigger wheels, a lift, climbs steeper ground", 1500},
    {"Monster tyres", "huge wheels, a big lift, climbs almost anything", 4000},
}};
inline constexpr std::array<Part, 3> bumpers{{
    {"Factory bumper", "as it left the works", 0},
    {"Steel push bar", "crashes hurt the car less", 900},
    {"Bull bar", "crashes hurt the car much less", 1600},
}};
inline constexpr int factory_tires = 1;

// What the mods do to the car (game::vehicles reads these from its placement state).
struct Tuning {
    float power{1}, top{1};     // acceleration and top speed multipliers
    float wheel{1};             // wheel diameter multiplier (the body rides up or down with it)
    float slope{0};             // degrees more slope it can climb
    float armour{1};            // crash damage multiplier
    float grip{1};              // cornering grip multiplier
};
[[nodiscard]] Tuning tuning(const Mods& mods);
[[nodiscard]] bool valid(const Mods& mods);
// Fit the mods to a car: its placement state carries the tuning and the paint from now on.
void apply(const Mods& mods, world::VehiclePlacementState& car);
[[nodiscard]] Mods mods_of(const world::VehiclePlacementState& car);
[[nodiscard]] std::string serialise(const Mods& mods);       // "paint engine tires bumper"
[[nodiscard]] bool parse(std::string_view text, Mods& out);

// A repair: a few kronoj a point of damage (body, engine and tyres).
[[nodiscard]] int repair_cost(const world::VehiclePlacementState& car);
void repair(world::VehiclePlacementState& car);

// Marnafto's pumps.
inline constexpr float tank_litres = 60;
inline constexpr int litre_price = 2;   // kronoj
// What it costs to fill the tank from `fuel` (0..1), and how much of it `money` buys.
[[nodiscard]] int fill_cost(float fuel);
[[nodiscard]] float fill(float fuel, std::int64_t& money);   // returns the new level, takes the money

// Aŭtovendejo Verda's stock.
struct ForSale { const char* definition; const char* name; const char* detail; int price; };
inline constexpr std::array<ForSale, 5> dealer{{
    {"hatchback", "Classic hatchback", "small, cheap to run", 6000},
    {"sxs", "Vakero side-by-side", "goes anywhere off the road", 8500},
    {"sedan", "Verda Sedano", "the family car", 11000},
    {"truck", "Verda pickup", "a working truck with a bed", 14000},
    {"green_hand_gt", "Verda Mano GT", "the island's fastest car", 45000},
}};

// A car the player bought: it is theirs from session to session.
struct OwnedCar {
    std::string definition;
    std::string vehicle;         // its runtime id this session ("" until spawned)
    Mods mods;
    Vector3 at{};                // where it was left
    float yaw{0};
    float fuel{1};
};
[[nodiscard]] std::string serialise(const OwnedCar& car);
[[nodiscard]] bool parse(std::string_view text, OwnedCar& out);
inline constexpr std::size_t max_owned = 12;

} // namespace outland::game::autos
