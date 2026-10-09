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
              << " pieces placed, " << report.skipped_lots << " lots skipped as occupied\n";
    return 0;
}
