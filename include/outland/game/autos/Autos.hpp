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
    int plating{0}, gun{0};     // the depot's work on a big rig: armour plating, a roof gun
    int tank{0};                // and its fuel tanks (game::freight::Diesel::range_km, times tanks[])
    // The workshop's dress-up and bolt-ons: exhausts, spoilers, window tint, rims, underglow.
    int exhaust{0}, spoiler{0}, tint{0}, rims{0}, glow{0};
    friend bool operator==(const Mods&, const Mods&) = default;
};

struct Paint { const char* name; Color color; int price; };
struct Part { const char* name; const char* detail; int price; };
// An engine: what it does (power and top speed multipliers, degrees more slope it can pull up) and
// what it goes in - petrol for the workshops' cars, diesel for the depot's big rigs.
struct Engine { const char* name; const char* detail; int price; float power, top, climb; bool diesel; };

inline constexpr int paint_price = 1200;
// The street colours, then the five ball clubs' (game::sports::teams: their buses wear them).
inline constexpr std::size_t street_paints = 10, team_paints = street_paints;
inline constexpr std::array<Paint, 15> paints{{
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
    {"Verda City green", {18, 118, 58, 255}, paint_price},
    {"Espera brown", {104, 66, 38, 255}, paint_price},
    {"Porto Luma navy", {26, 46, 108, 255}, paint_price},
    {"Suda Haveno teal", {0, 124, 126, 255}, paint_price},
    {"Roka charcoal", {40, 40, 44, 255}, paint_price},
}};
// (The first three keep their places: saves store the index.)
inline constexpr std::array<Engine, 9> engines{{
    {"Factory engine", "as it left the works", 0, 1, 1, 0, false},
    {"Tuned engine", "+30% power", 2500, 1.3F, 1.05F, 0, false},
    {"V8 big block", "+65% power, +15% top speed", 7000, 1.65F, 1.15F, 1, false},
    {"Turbocharged", "+45% power, +8% top speed", 4200, 1.45F, 1.08F, 0, false},
    {"Supercharged V8", "+95% power, +22% top speed", 13000, 1.95F, 1.22F, 2, false},
    {"Racing engine", "+130% power, +30% top speed", 26000, 2.3F, 1.3F, 2, false},
    {"Big-bore diesel", "+40% pull, +8% top speed, climbs 2 deg steeper", 5000, 1.4F, 1.08F, 2, true},
    {"Twin-turbo diesel", "+80% pull, +15% top speed, climbs 4 deg steeper", 11000, 1.8F, 1.15F, 4, true},
    {"Road-train diesel", "+130% pull, +22% top speed, climbs 6 deg steeper", 22000, 2.3F, 1.22F, 6, true},
}};
// The engines a vehicle can take, cheapest first (the factory one first): diesels for a big rig.
[[nodiscard]] std::vector<int> engine_choices(bool rig);
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
// Verda Transporto's shed fits a big rig out for the road: armour against rounds (and crashes),
// and a gun on the roof the driver works from the cab (all round, wherever they look).
inline constexpr std::array<Part, 3> platings{{
    {"No plating", "as it left the works", 0},
    {"Steel plating", "half the damage from rounds and crashes", 6000},
    {"Fortress plating", "a quarter of the damage, bullet-proof glass", 15000},
}};
// More range for a big rig: its tanks (a full tank's distance times `range`).
inline constexpr std::array<Part, 3> tanks{{
    {"Factory tanks", "about 100 km on a fill", 0},
    {"Long-range tanks", "about 170 km on a fill", 3500},
    {"Road-train tanks", "about 260 km on a fill", 8000},
}};
inline constexpr std::array<float, 3> tank_range{1, 1.7F, 2.6F};
// Exhausts: louder, a little more power, and the straight pipes crackle when you lift off.
inline constexpr std::array<Part, 3> exhausts{{
    {"Factory exhaust", "quiet, as it left the works", 0},
    {"Sport exhaust", "a deeper note, +4% power", 1100},
    {"Straight pipes", "loud, pops and bangs, +8% power", 2400},
}};
inline constexpr std::array<Part, 3> spoilers{{
    {"No spoiler", "as it left the works", 0},
    {"Ducktail", "a lip on the boot, +5% grip", 700},
    {"Race wing", "a big wing, +12% grip, -3% top speed", 1800},
}};
inline constexpr std::array<Part, 3> tints{{
    {"Clear glass", "as it left the works", 0},
    {"Smoked windows", "a light tint", 400},
    {"Limo tint", "nobody sees in", 900},
}};
struct Rim { const char* name; const char* detail; int price; Color color; };
inline constexpr std::array<Rim, 4> rims{{
    {"Factory rims", "as they left the works", 0, {200, 200, 204, 255}},
    {"Chrome rims", "mirror bright", 1200, {240, 244, 250, 255}},
    {"Black rims", "satin black", 900, {34, 34, 38, 255}},
    {"Gold rims", "for the boss", 2200, {222, 176, 56, 255}},
}};
struct Glow { const char* name; const char* detail; int price; Color color; };
inline constexpr std::array<Glow, 5> glows{{
    {"No underglow", "as it left the works", 0, {0, 0, 0, 0}},
    {"Green underglow", "Verda green, under the sills", 1500, {40, 255, 120, 255}},
    {"Blue underglow", "cool blue, under the sills", 1500, {60, 140, 255, 255}},
    {"Pink underglow", "hot pink, under the sills", 1500, {255, 80, 200, 255}},
    {"Amber underglow", "warm amber, under the sills", 1500, {255, 170, 40, 255}},
}};
inline constexpr std::array<Part, 2> guns{{
    {"No roof gun", "as it left the works", 0},
    {"Roof gun", "a machine gun on the cab roof - fire it from the wheel", 12000},
}};

// What the mods do to the car (game::vehicles reads these from its placement state).
struct Tuning {
    float power{1}, top{1};     // acceleration and top speed multipliers
    float wheel{1};             // wheel diameter multiplier (the body rides up or down with it)
    float slope{0};             // degrees more slope it can climb
    float armour{1};            // crash damage multiplier
    float grip{1};              // cornering grip multiplier
    float plating{1};           // damage from rounds (and crashes) multiplier
    bool gun{false};            // a roof gun
    float range{1};             // fuel range multiplier (a rig's tanks)
};
[[nodiscard]] Tuning tuning(const Mods& mods);
[[nodiscard]] bool valid(const Mods& mods);
// Fit the mods to a car: its placement state carries the tuning and the paint from now on.
void apply(const Mods& mods, world::VehiclePlacementState& car);
[[nodiscard]] Mods mods_of(const world::VehiclePlacementState& car);
[[nodiscard]] std::string serialise(const Mods& mods);       // "paint engine tires bumper plating gun tank exhaust spoiler tint rims glow"
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
inline constexpr std::array<ForSale, 9> dealer{{
    {"hatchback", "Classic hatchback", "small, cheap to run", 6000},
    {"sxs", "Vakero side-by-side", "goes anywhere off the road", 8500},
    {"wagon", "Verda Familio", "the estate: room for the dog", 9500},
    {"sedan", "Verda Sedano", "the family car", 11000},
    {"van", "Verda Furgono", "a box van for the trade", 12500},
    {"truck", "Verda pickup", "a working truck with a bed", 14000},
    {"flatbed", "Verda Platformo", "a flatbed for the heavy stuff", 16500},
    {"muscle", "Verda Tempesto", "a big V8 and not much sense", 24000},
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
