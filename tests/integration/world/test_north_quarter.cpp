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
#include <vector>
using namespace outland;
namespace nq = outland::world::north;
using world::physics::WorldCollision;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    // The line: one path from Okcidenta Parko to Orienta Verda, its pieces end to end.
    const float total = nq::length();
    check(total > 1000, "La Levita runs across the city: " + std::to_string(total) + " m");
    for (float d = 0; d < total; d += 1) {
        const Vector2 a = nq::at(d), b = nq::at(d + 1);
        check(std::hypot(b.x - a.x, b.y - a.y) < 1.01F, "the line is one piece");
        check(nq::distance(a.x, a.y) < .05F, "on its own line");
    }
    // The timetable: a round of the line, both trains, a tenth of a second at a time.
    const double round = nq::round_time();
    check(round > 200 && round < 600, "a round of the line in a few minutes: " + std::to_string(round));
    int stood[2][nq::stations.size()]{};
    for (int k = 0; k < 2; ++k) {
        float last = nq::train(k, 0).s;
        for (double t = .1; t < round * 2; t += .1) {
            const auto train = nq::train(k, t);
            check(train.s - nq::train_half >= -.01F && train.s + nq::train_half <= total + .01F, "the trains stay on the line");
            check(std::abs(train.speed) <= nq::cruise + .3F, "never faster than the line's speed");
            check(std::abs(train.s - last) <= nq::cruise * .1F + .05F, "never a jump");
            last = train.s;
            for (int c = 0; c < 2; ++c) {
                const auto car = nq::car(train, c);
                check(std::abs(nq::distance(car.at.x, car.at.z) - 1.8F) < .15F, "each car on its own track");
            }
            if (train.standing)
                for (std::size_t i = 0; i < nq::stations.size(); ++i) stood[k][i] += std::abs(train.s - nq::station_s(i)) < 1;
        }
        for (const auto& n : stood[k]) check(n > 0, "each train stands at every station");
    }
    const auto a = nq::train(0, 10), b = nq::train(1, 10);
    check(std::abs(a.s - b.s) > 1 || a.standing != b.standing, "the second train runs half a round behind the first");

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
    std::vector<Vector3> decks;
    for (const auto& s : region.settlements()) for (const auto& asset : s.assets) {
        const auto& m = asset.model_path;
        if (m.find("/city/north/") == std::string::npos) continue;
        flats += m.find("three_flat") != std::string::npos;
        walkups += m.find("walkup") != std::string::npos;
        taverns += m.find("corner_tavern") != std::string::npos;
        murals += m.find("_mural_") != std::string::npos;
        stations += m.ends_with("l_station.glb");
        if (m.find("l_deck") != std::string::npos) decks.push_back(asset.position);
    }
    check(flats >= 30 && walkups >= 12 && taverns >= 3 && murals >= 2, "three-flats, walk-ups, taverns and murals: " + std::to_string(flats) + "/" +
          std::to_string(walkups) + "/" + std::to_string(taverns) + "/" + std::to_string(murals));
    check(stations == static_cast<int>(nq::stations.size()), "five stations on La Levita");
    for (float d = 1; d < total - 1; d += 2) {
        const Vector2 p = nq::at(d);
        float best = 1e9F;
        for (const auto& q : decks) best = std::min(best, std::hypot(q.x - p.x, q.z - p.y));
        check(best < nq::l_piece * .5F + .5F, "the deck the length of the line at " + std::to_string(d));
    }
    // Under the deck: the avenues, the highway and the alleys open to cars and to people.
    const float street = world::terrain::TerrainHeight::sample(0, 0);
    std::vector<Vector2> under{{-87, nq::l_z}, {-5, nq::l_z}, {5, nq::l_z}, {87, nq::l_z}, {-45, nq::l_z}, {45, nq::l_z}, {-129, nq::l_z}, {129, nq::l_z}};
    for (const float side : {-1.0F, 1.0F})   // down the suburb streets, and over their cross streets
        for (const float z : {-150.0F, -126.0F, -100.0F, -84.0F})
            if (side > 0 || z != -84) under.push_back({side * nq::suburb_street, z});
    for (const auto& u : under) {
        const float x = u.x;
        const Vector3 p{x, WorldCollision::ground_height({x, street + 1, u.y}, street, region), u.y};
        check(std::abs(p.y - street) < .3F, "the street under the deck at " + std::to_string(x) + " (" + std::to_string(p.y) + ")");
        check(!WorldCollision::vehicle_blocked(p, region, 1.0F), "a car drives on under the deck at " + std::to_string(x));
        check(!WorldCollision::body_blocked(p, p.y, region, .4F), "people walk on under it at " + std::to_string(x));
    }
    // Up the stairs to a platform: the treads climb to it.
    const auto& centro = nq::stations[2].at;
    float feet = street;
    const float stair_z = nq::l_z - 7.7F;   // the north platform's stair, beside it
    for (float x = centro.x + 2 + 40 * .3F + .5F; x > centro.x + 1.5F; x -= .1F)
        feet = WorldCollision::ground_height({x, feet + 1, stair_z}, feet, region);
    check(std::abs(feet - (street + nq::l_platform)) < .35F, "the stairs climb to the platform: " + std::to_string(feet - street));
    // Every station, both platforms: in off the street at the foot of its stair, up it to the landing
    // and across onto the platform (the platform's railing open there, nowhere else), along its outer lane past the benches.
    {
        const auto walk = [&](Vector3 p, Vector3 to) {
            for (int i = 0; i < 4000; ++i) {
                const float dx = to.x - p.x, dz = to.z - p.z, l = std::hypot(dx, dz);
                if (l < .05F) break;
                const float st = std::min(.05F, l);
                p = WorldCollision::resolve_body_movement(p, {p.x + dx / l * st, p.y, p.z + dz / l * st}, p.y, region, .3F);
                p.y = WorldCollision::ground_height(p, p.y + .4F, region);
            }
            return p;
        };
        for (std::size_t i = 0; i < nq::stations.size(); ++i) {
            const float s = nq::station_s(i);
            Vector2 h{};
            const Vector2 c = nq::at(s, &h);
            // Station-local (x across, z along the line) to the world: the model's +Z along the line.
            const auto world = [&](float x, float z) { return Vector3{c.x + x * h.y + z * h.x, 0, c.y - x * h.x + z * h.y}; };
            for (const float side : {-1.0F, 1.0F}) {
                const std::string where = std::string(nq::stations[i].name) + (side < 0 ? " (left)" : " (right)");
                Vector3 p = world(side * 7.6F, 16.5F);
                const float ground = WorldCollision::ground_height({p.x, street + 1, p.z}, street, region);
                p.y = ground;
                for (const auto& [x, z] : {std::pair{7.6F, 13.0F}, {7.6F, 1.2F}, {6.3F, 1.2F}, {6.3F, -8.0F}}) {
                    const Vector3 to = world(side * x, z), got = walk(p, to);
                    check(std::hypot(got.x - to.x, got.z - to.z) < .2F, "up to the platform at " + where + ": stuck short of " + std::to_string(x) + "," +
                          std::to_string(z) + " (" + std::to_string(got.y - ground) + " m up)");
                    p = got;
                }
                check(std::abs(p.y - ground - nq::l_platform) < .3F, "on the platform at " + where + " (" + std::to_string(p.y - ground) + ")");
                // And the platform's side over the stair is railed: walking out sideways halfway along it stops you.
                Vector3 q = walk(p, world(side * 6.3F, 8.0F));
                q = walk(q, world(side * 9.0F, 8.0F));
                check(std::abs(q.y - ground - nq::l_platform) < .3F, "no stepping off the platform onto the stair's side at " + where);
            }
        }
    }
    std::cout << "Norda Kvartalo: " << flats << " three-flats, " << walkups << " walk-ups, " << taverns << " taverns, " << murals << " murals; La Levita "
              << total << " m, " << stations << " stations, a round in " << round << " s: GREEN\n";
}
