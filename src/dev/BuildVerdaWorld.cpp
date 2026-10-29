// outland_build_world: dress the default Verda towns with the Creator catalog and write a
// published world map (maps/verda_world.map by default). Keep editing it in MAP BUILDER.
#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    const std::string root = argc > 2 ? argv[2] : ".";
    const std::string output = argc > 1 ? argv[1] : root + "/maps/verda_world.map";
    outland::world::VerdaRegion region(true);
    // Same baseline a fresh install starts from: coastal layout plus the default loot markers.
    if (!outland::creator::CreatorMapIO::load(region, root + "/maps/verda_loot_defaults.map")) {
        std::cerr << "cannot load " << root << "/maps/verda_loot_defaults.map\n";
        return 1;
    }
    const outland::creator::CreatorAssetRegistry catalog;
    const auto report = outland::dev::build_verda_towns(region, catalog);
    for (const auto& path : report.missing) std::cerr << "missing from catalog: " << path << '\n';
    if (!report.missing.empty()) return 1;
    if (!outland::creator::CreatorMapIO::save(region, output)) {
        std::cerr << "cannot write " << output << '\n';
        return 1;
    }
    std::cout << "wrote " << output << ": " << report.buildings << " enterable buildings, " << report.assets
              << " pieces placed, " << report.skipped_lots << " lots skipped as occupied; parliament "
              << (report.parliament ? "built" : "NOT built") << ", " << report.town_halls << " town halls, stadium ground "
              << (report.stadium_reserved ? "kept free" : "NOT reserved") << ", Stela Turo "
              << (report.stela_turo ? "built at " + std::to_string(static_cast<int>(report.stela_turo_at.x)) + ", " + std::to_string(static_cast<int>(report.stela_turo_at.z)) : std::string("NOT built")) << '\n';
    std::cout << report.street_lights << " street lights added along dark town streets\n";
    std::cout << report.baptist_churches << " Baptist chapels (west), " << report.orthodox_churches << " Orthodox churches (east)\n";
    std::cout << report.ranches.size() << " Vakero ranches at";
    for (const auto& yard : report.ranches) std::cout << " (" << static_cast<int>(yard.x) << ", " << static_cast<int>(yard.z) << ")";
    std::cout << "; stockyard " << (report.stockyard ? "built" : "NOT built") << '\n';
    std::cout << report.gun_shops.size() << " Armilejo gun shops at";
    for (const auto& door : report.gun_shops) std::cout << " (" << static_cast<int>(door.x) << ", " << static_cast<int>(door.z) << ")";
    std::cout << '\n';
    std::cout << report.gas_stations.size() << " Marnafto stations, " << report.mechanics.size() << " workshops, car lot "
              << (report.car_lot ? "at (" + std::to_string(static_cast<int>(report.car_lot_at.x)) + ", " + std::to_string(static_cast<int>(report.car_lot_at.z)) + ")" : std::string("missing"))
              << ", " << report.oil_rigs.size() << " oil rigs\n";
    for (const auto& r : report.oil_rigs) std::cout << "  rig (" << static_cast<int>(r.x) << ", " << static_cast<int>(r.z) << ")\n";
    for (const auto& g : report.gas_stations) std::cout << "  station (" << static_cast<int>(g.x) << ", " << static_cast<int>(g.z) << ")\n";
    for (const auto& g : report.mechanics) std::cout << "  workshop (" << static_cast<int>(g.x) << ", " << static_cast<int>(g.z) << ")\n";
    std::cout << report.freight_yards.size() << " freight yards at";
    for (const auto& y : report.freight_yards) std::cout << " (" << static_cast<int>(y.x) << ", " << static_cast<int>(y.z) << ")";
    std::cout << '\n';
    std::cout << report.gang_houses.size() << " gang houses at";
    for (const auto& house : report.gang_houses) std::cout << " (" << static_cast<int>(house.x) << ", " << static_cast<int>(house.z) << ")";
    std::cout << '\n';
    std::cout << report.radio_stations.size() << " radio stations at";
    for (const auto& door : report.radio_stations) std::cout << " (" << static_cast<int>(door.x) << ", " << static_cast<int>(door.z) << ")";
    std::cout << '\n';
    return 0;
}
