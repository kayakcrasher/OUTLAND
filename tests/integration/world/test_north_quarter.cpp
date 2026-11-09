// Norda Kvartalo (world/NorthQuarter.hpp): La Levita's timetable - two trains that keep to their
// tracks, never outrun the line's speed, never jump, and stand still at the three stations - then
// the world map's district: its streets, its blocks of three-flats, walk-ups and taverns, the
// elevated deck the whole length of the line on its bents, the stations' platforms you can climb
// to, and the streets and alleys open to cars and people under the deck.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/NorthQuarter.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
namespace nq = outland::world::north;
using world::physics::WorldCollision;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    // The timetable: a round of the line, both trains, a tenth of a second at a time.
    const double round = nq::round_time();
    check(round > 100 && round < 300, "a round of the line in a few minutes: " + std::to_string(round));
    int stood[2][3]{};
    for (int k = 0; k < 2; ++k) {
        Vector3 last = nq::train(k, 0).at;
        for (double t = .1; t < round * 2; t += .1) {
            const auto train = nq::train(k, t);
            check(std::abs(train.at.z - (nq::l_z + nq::l_tracks[static_cast<std::size_t>(k)])) < .01F, "each train keeps to its own track");
            check(train.at.x - nq::train_half >= nq::l_west && train.at.x + nq::train_half <= nq::l_east, "the trains stay on the deck");
            check(std::abs(train.speed) <= nq::cruise + .3F, "never faster than the line's speed");
            check(std::abs(train.at.x - last.x) <= nq::cruise * .1F + .05F, "never a jump");
            last = train.at;
            if (train.standing)
                for (std::size_t s = 0; s < nq::stations.size(); ++s)
                    stood[k][s] += std::abs(train.at.x - nq::stations[s].x) <= nq::station_half - nq::train_half + 3;
        }
        for (const auto& n : stood[k]) check(n > 0, "each train stands at every station");
    }
    const auto a = nq::train(0, 10), b = nq::train(1, 10);
    check(std::abs(a.at.x - b.at.x) > 1 || a.standing != b.standing, "the second train runs half a round behind the first");

    // The district on the world map.
    world::terrain::TerrainHeight::set_coastal_layout(true);
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    const world::Settlement* capital = nullptr;
    for (const auto& s : region.settlements()) if (s.id == "capital_verda") capital = &s;
    check(capital, "Verda City is on the map");
    int streets = 0;
    for (const auto& r : capital->roads)
        streets += (r.start.z == r.end.z && (r.start.z == nq::streets[0] || r.start.z == nq::streets[1]) && r.start.x < -170 && r.end.x > 170) ||
                   (r.start.x == r.end.x && std::abs(r.start.x) > 80 && std::min(r.start.z, r.end.z) < -nq::north_edge + 1);
    check(streets == 6, "the avenues on north and the two new streets across the city: " + std::to_string(streets));
    int flats = 0, walkups = 0, taverns = 0, murals = 0, stations = 0;
    float deck_from = 1e9F, deck_to = -1e9F;
    for (const auto& s : region.settlements()) for (const auto& asset : s.assets) {
        const auto& m = asset.model_path;
        if (m.find("/city/north/") == std::string::npos) continue;
        flats += m.find("three_flat") != std::string::npos;
        walkups += m.find("walkup") != std::string::npos;
        taverns += m.find("corner_tavern") != std::string::npos;
        murals += m.find("_mural_") != std::string::npos;
        stations += m.ends_with("l_station.glb");
        if (m.ends_with("l_deck.glb")) {deck_from = std::min(deck_from, asset.position.x - nq::l_piece * .5F); deck_to = std::max(deck_to, asset.position.x + nq::l_piece * .5F);}
    }
    check(flats >= 30 && walkups >= 12 && taverns >= 3 && murals >= 2, "three-flats, walk-ups, taverns and murals: " + std::to_string(flats) + "/" +
          std::to_string(walkups) + "/" + std::to_string(taverns) + "/" + std::to_string(murals));
    check(stations == 3, "three stations on La Levita");
    check(deck_from <= nq::l_west + .1F && deck_to >= nq::l_east - .1F, "the deck the length of the line");
    // Under the deck: the avenues, the highway and the alleys open to cars and to people.
    const float street = world::terrain::TerrainHeight::sample(0, 0);
    for (const float x : {-87.0F, -5.0F, 5.0F, 87.0F, -45.0F, 45.0F, -129.0F, 129.0F}) {
        const Vector3 p{x, WorldCollision::ground_height({x, street + 1, nq::l_z}, street, region), nq::l_z};
        check(std::abs(p.y - street) < .3F, "the street under the deck at " + std::to_string(x) + " (" + std::to_string(p.y) + ")");
        check(!WorldCollision::vehicle_blocked(p, region, 1.0F), "a car drives on under the deck at " + std::to_string(x));
        check(!WorldCollision::body_blocked(p, p.y, region, .4F), "people walk on under it at " + std::to_string(x));
    }
    // Up the stairs to a platform: the treads climb to it.
    const auto& centro = nq::stations[1];
    float feet = street;
    const float stair_z = nq::l_z - 7.7F;   // the north platform's stair, beside it
    for (float x = centro.x + 2 + 40 * .3F + .5F; x > centro.x + 1.5F; x -= .1F)
        feet = WorldCollision::ground_height({x, feet + 1, stair_z}, feet, region);
    check(std::abs(feet - (street + nq::l_platform)) < .35F, "the stairs climb to the platform: " + std::to_string(feet - street));
    std::cout << "Norda Kvartalo: " << flats << " three-flats, " << walkups << " walk-ups, " << taverns << " taverns, " << murals << " murals; La Levita "
              << deck_to - deck_from << " m, " << stations << " stations, a round in " << round << " s: GREEN\n";
}
