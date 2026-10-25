// The motor trade's rules (game::autos): what a mechanic's parts do to a car, the paint, repairs,
// Marnafto's pumps, the car lot's stock and the bought cars' save lines.
#include "outland/game/autos/Autos.hpp"
#include "outland/world/WorldAsset.hpp"
#include <cmath>
#include <iostream>
#include <string>

using namespace outland;
namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
    namespace autos = game::autos;
    // A factory car: nothing changed.
    world::VehiclePlacementState car;
    const auto factory = autos::mods_of(car);
    check(factory == autos::Mods{} && autos::valid(factory), "a factory car has factory parts");
    const auto stock = autos::tuning(factory);
    check(stock.power == 1 && stock.top == 1 && stock.wheel == 1 && stock.armour == 1 && stock.slope == 0, "factory parts change nothing");

    // The V8, monster tyres, a bull bar and a coat of red.
    autos::Mods big{1, 2, 3, 2};
    check(autos::valid(big), "a full build is valid");
    autos::apply(big, car);
    check(car.power > 1.5F && car.top > 1.1F, "the big block: more power, more top speed");
    check(car.wheel > 1.5F && car.slope > 10, "monster tyres: big wheels that climb");
    check(car.armour < .5F, "a bull bar takes the crashes");
    check(car.painted && car.paint.r == autos::paints[1].color.r && car.paint.g == autos::paints[1].color.g, "painted racing red");
    check(autos::mods_of(car) == big, "the car remembers what was fitted");
    autos::apply(autos::Mods{}, car);
    check(!car.painted && car.power == 1 && car.wheel == 1, "back to factory");
    check(!autos::valid(autos::Mods{static_cast<int>(autos::paints.size()), 0, 1, 0}) && !autos::valid(autos::Mods{-1, static_cast<int>(autos::engines.size()), 1, 0}) && !autos::valid(autos::Mods{-1, 0, 4, 0}),
          "no such parts");
    autos::Mods back;
    check(autos::parse(autos::serialise(big), back) && back == big, "mods survive a save");
    check(!autos::parse("1 2", back) && !autos::parse("1 9 1 0", back), "bad mods are refused");
    // The depot's work on a rig: plating and a roof gun, saved with the rest (older saves have neither).
    check(autos::parse("1 2 3 2", back) && back.plating == 0 && back.gun == 0, "an older save: no plating, no gun");
    autos::Mods armed{-1, 1, 1, 0, 2, 1};
    check(autos::valid(armed) && autos::parse(autos::serialise(armed), back) && back == armed, "plating and the roof gun survive a save");
    check(autos::tuning(armed).plating == .25F && autos::tuning(armed).gun && autos::tuning({-1, 0, 1, 0, 1, 0}).plating == .5F, "plating stops rounds");
    check(!autos::valid(autos::Mods{-1, 0, 1, 0, 3, 0}) && !autos::valid(autos::Mods{-1, 0, 1, 0, 0, 2}), "no such plating or gun");
    world::VehiclePlacementState plated;
    autos::apply(armed, plated);
    check(plated.gun && plated.plating == .25F && autos::mods_of(plated) == armed, "the rig remembers its fit-out");
    // More engines: petrol for the cars, diesels for the rigs, cheapest first, each pulling harder
    // than the one before; the saved indices of the first three stay put.
    check(std::string(autos::engines[2].name) == "V8 big block" && autos::engines[2].power == 1.65F, "the V8 keeps its place in old saves");
    for (const bool rig : {false, true}) {
        const auto choices = autos::engine_choices(rig);
        check(choices.size() >= 4 && choices.front() == 0, std::string(rig ? "diesels" : "petrol engines") + " on offer, the factory one first");
        for (std::size_t i = 1; i < choices.size(); ++i) {
            const auto& e = autos::engines[static_cast<std::size_t>(choices[i])], &prev = autos::engines[static_cast<std::size_t>(choices[i - 1])];
            check(e.diesel == rig && e.price > prev.price && e.power > prev.power, std::string("each dearer engine pulls harder: ") + e.name);
        }
    }
    check(autos::tuning({-1, 8, 1, 0}).power > 2 && autos::tuning({-1, 8, 1, 0}).slope >= 6, "the road-train diesel pulls up steeper ground");
    // Long-range tanks for a rig, saved (older lines have the factory tanks).
    autos::Mods tanked{-1, 7, 1, 0, 1, 0, 2};
    check(autos::valid(tanked) && autos::tuning(tanked).range > 2 && autos::parse(autos::serialise(tanked), back) && back == tanked, "road-train tanks, saved");
    check(autos::parse("-1 1 1 0 2 1", back) && back.tank == 0, "an older save: factory tanks");
    check(!autos::valid(autos::Mods{-1, 0, 1, 0, 0, 0, 3}), "no such tanks");
    // Smaller tyres sit low and grip; bigger ones lift the car.
    check(autos::tuning({-1, 0, 0, 0}).wheel < 1 && autos::tuning({-1, 0, 0, 0}).grip > 1, "low-profile tyres");

    // Repairs: nothing to fix costs nothing; a wreck costs plenty, and comes back new.
    world::VehiclePlacementState wreck;
    check(autos::repair_cost(wreck) == 0, "nothing to fix");
    wreck.health = 20; wreck.engine = 40; wreck.tires = {0, 100, 100, 50}; wreck.destroyed = true;
    check(autos::repair_cost(wreck) > 500, "a wreck costs: " + std::to_string(autos::repair_cost(wreck)));
    autos::repair(wreck);
    check(wreck.health == 100 && wreck.engine == 100 && wreck.tires[0] == 100 && !wreck.destroyed, "good as new");

    // Marnafto: a full tank costs nothing; an empty one 60 litres at 2 kr; short of cash, what it buys.
    check(autos::fill_cost(1) == 0 && autos::fill_cost(0) == static_cast<int>(autos::tank_litres) * autos::litre_price, "the price of a tank");
    std::int64_t money = 1000;
    float fuel = autos::fill(.25F, money);
    check(std::abs(fuel - 1) < 1e-4F && money == 1000 - 90, "filled up: 45 litres, 90 kr (" + std::to_string(money) + ")");
    money = 30;
    fuel = autos::fill(0, money);
    check(money == 0 && std::abs(fuel - .25F) < 1e-3F, "30 kr buys a quarter of a tank");

    // The car lot sells real cars, cheapest first, the GT dearest.
    for (std::size_t i = 1; i < autos::dealer.size(); ++i) check(autos::dealer[i].price > autos::dealer[i - 1].price, "the lot's prices rise");

    // A bought car's save line.
    autos::OwnedCar mine{"truck", "creator_vehicle_7", {3, 1, 2, 1}, {12.5F, 3, -40.25F}, 87, .5F};
    autos::OwnedCar loaded;
    check(autos::parse(autos::serialise(mine), loaded) && loaded.definition == "truck" && loaded.mods == mine.mods &&
          std::abs(loaded.at.z + 40.25F) < 1e-3F && std::abs(loaded.yaw - 87) < 1e-3F && std::abs(loaded.fuel - .5F) < 1e-4F && loaded.vehicle.empty(),
          "a bought car survives a save (its id is the session's)");
    check(autos::parse("semi 0 0 1 0 1 2 3 4 1", loaded) && loaded.definition == "semi" && loaded.mods.plating == 0, "an older car line still loads");
    autos::OwnedCar rig{"semi", "", {-1, 0, 1, 0, 1, 1}, {1, 2, 3}, 0, 1};
    check(autos::parse(autos::serialise(rig), loaded) && loaded.mods == rig.mods, "an armed rig's save line");
    autos::OwnedCar long_haul{"semi", "", {-1, 8, 2, 0, 2, 1, 1}, {1, 2, 3}, 0, 1};
    check(autos::parse(autos::serialise(long_haul), loaded) && loaded.mods == long_haul.mods, "a long-haul rig's save line");
    check(!autos::parse("tr uck 0 0 1 0 1 2 3 4 1", loaded) && !autos::parse("truck 0 0 1 0 1 2 3 4 2", loaded), "bad car lines are refused");

    if (failures == 0) std::cout << "autos ok\n";
    return failures == 0 ? 0 : 1;
}
