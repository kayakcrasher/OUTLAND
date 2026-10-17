#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/economy/Homes.hpp"
#include "outland/game/interiors/Furnishing.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/game/vehicles/VehicleRegistry.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>

using namespace outland;

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {std::cerr << "[FAIL] " << message << '\n'; ++failures;}
}
}

int main() {
    world::VerdaRegion region(true);
    if (!creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map")) {std::cerr << "no map\n"; return 1;}
    // Fourteen homes for sale: detached houses and Main Street flats in the other towns.
    const auto homes = game::economy::plan_homes(region);
    check(homes.size() == static_cast<std::size_t>(game::economy::home_listings), "fourteen homes for sale");
    std::set<std::string> ids, towns;
    int houses = 0;
    bool priced = true, furnished = true;
    for (const auto& home : homes) {
        ids.insert(home.id);
        towns.insert(std::to_string(home.settlement));
        houses += home.house;
        priced = priced && home.price >= 1500 && home.price <= 7500;
        const auto pieces = game::interiors::furnish_home(game::economy::place_of(home), home.centre.y);
        bool bed = false;
        for (const auto& piece : pieces) bed = bed || piece.model.ends_with("bed_double.glb");
        furnished = furnished && !pieces.empty() && bed;
        // The board stands out front, outside the building.
        check(!game::economy::inside(home, {home.board.x, home.centre.y, home.board.z}, 0), "the VENDATA board is outside");
    }
    check(ids.size() == homes.size(), "each listed once");
    check(houses >= 4 && houses <= 10 && towns.size() >= 4, "houses in Okcidenta Verda and flats round the island");
    check(priced, "priced 1,500 to 7,500 kr");
    check(furnished, "each comes furnished, with a bed to sleep in, once bought");
    check(game::economy::plan_homes(region).front().id == homes.front().id, "the same listings every time");
    // Listed homes stand empty: nobody moves in; restoring gives the buildings back.
    auto vacated = game::economy::vacate(region, homes);
    check(vacated.size() == homes.size(), "every listed building made vacant");
    const auto island = game::life::build_island(region, nullptr);
    bool empty = true;
    for (const auto& place : island.places)
        for (const auto& home : homes) empty = empty && place.building_id != home.building;
    check(empty, "island life leaves the homes for sale empty");
    game::economy::restore_purposes(region, vacated);
    bool back = true;
    for (const auto& town : region.settlements()) {
        for (const auto& b : town.buildings) for (const auto& v : vacated) if (b.id == v.id) back = back && b.purpose == v.purpose;
        for (const auto& a : town.assets) for (const auto& v : vacated) if (a.id == v.id) back = back && a.purpose == v.purpose;
    }
    check(back, "restored afterwards");
    // Buying: all or nothing, once.
    game::economy::EconomyState wallet;
    const auto& home = homes.front();
    wallet.money = home.price - 1;
    check(game::economy::buy(wallet, home).find("costs") != std::string::npos && !game::economy::owns(wallet, home) && wallet.money == home.price - 1,
          "too dear: nothing changes");
    wallet.money = home.price + 10;
    check(game::economy::buy(wallet, home).starts_with("Bought") && game::economy::owns(wallet, home) && wallet.money == 10, "bought");
    check(game::economy::buy(wallet, home).find("already") != std::string::npos && wallet.money == 10, "never twice");
    check(game::economy::valid_state(wallet), "the wallet saves");
    // The Verda Sedano, the second everyday car.
    game::vehicles::VehicleRegistry vehicles;
    std::string error;
    check(vehicles.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), "vehicle manifest loads");
    const auto* sedan = vehicles.find("sedan");
    check(sedan && std::filesystem::exists(std::string(OUTLAND_SOURCE_DIR) + "/" + sedan->body), "the Sedano is a vehicle");
    if (failures) return 1;
    std::cout << "[PASS] fourteen homes for sale, furnished when bought; the Verda Sedano\n";
    return 0;
}
