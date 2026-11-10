// Porto Luma's harbour (world/Harbour.hpp): Stella Maris's weekly timetable - alongside on Monday
// mornings, away at noon, back the next Monday, never jumping, always in water - and the opening
// that holds her alongside and sends her off once the player meets Uncle Ron; then the quay on
// the world map: cranes you can't walk through (but can walk and drive between their legs), the
// walk off the brow, through the arrival hall and over the zebra to the waiting area, and the
// port's gate over the terminal road.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/Harbour.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
namespace hb = outland::world::harbour;
namespace L = outland::world::layout;
using world::physics::WorldCollision;
using world::terrain::TerrainHeight;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
constexpr double day = 24 * 60;
}

int main() {
    TerrainHeight::set_coastal_layout(true);
    // The timetable, a week and a bit, a minute at a time.
    check(hb::at(6 * 60).alongside && hb::at(11.9 * 60).alongside, "alongside on Monday morning");
    check(!hb::at(12.5 * 60).alongside && !hb::at(12.5 * 60).at_sea, "under way out at half past twelve");
    check(hb::at(14 * 60).at_sea && hb::at(3 * day + 10 * 60).at_sea && hb::at(3.9 * 60).at_sea, "away at sea the rest of the week");
    check(!hb::at(4.5 * 60).alongside && !hb::at(4.5 * 60).at_sea, "coming in early on Monday");
    check(hb::at(hb::week + 6 * 60).alongside && hb::at(5 * hb::week + 9 * 60).alongside, "back every Monday");
    Vector2 last{};
    for (double m = 0; m < hb::week + day; m += 1) {
        const auto p = hb::at(m);
        if (p.at_sea) { last = hb::route.back(); continue; }
        if (m > 0) check(std::hypot(p.offset.x - last.x, p.offset.y - last.y) < 120, "she never jumps (" + std::to_string(m) + ")");
        last = p.offset;
    }
    check(std::abs(hb::sailing(0, false).yaw) < .01F && std::abs(hb::sailing(.001F, true).yaw) < .5F, "bow to the north alongside");
    // The water she sails: deep under her middle, her bow and her stern, all the way out.
    const Vector3 berth{L::cargo_quay_x1 + 9.2F, 0, L::cargo_z};
    for (float t = 0; t <= 1; t += .01F)
        for (const bool in : {false, true}) {
            const auto p = hb::sailing(t, in);
            const float a = p.yaw * 3.14159265F / 180;
            for (const float along : {-44.0F, 0.0F, 44.0F}) {
                const float x = berth.x + p.offset.x - std::sin(a) * along, z = berth.z + p.offset.y - std::cos(a) * along;
                check(TerrainHeight::sample(x, z) < L::sea_level - 1, "she sails in water (t " + std::to_string(t) + ")");
            }
        }
    // The opening: held alongside whatever the hour; away once the talk begins, gone within the hour;
    // the next Monday as ever.
    check(hb::at(15 * 60, -1, true).alongside, "held alongside for the arrival");
    const double met = 9.5 * 60;
    check(hb::at(met + 1, met).alongside == false && hb::at(met + 70, met).at_sea, "she sails when Uncle Ron starts talking");
    check(hb::at(hb::week + 6 * 60, met).alongside, "and is back the next Monday");

    // The quay on the world map.
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    const float quay = L::cargo_quay_height;
    int cranes = 0, halls = 0, gates = 0, brows = 0;
    for (const auto& town : region.settlements()) for (const auto& a : town.assets) {
        if (a.model_path.ends_with("civic/harbour_crane.glb")) {
            ++cranes;
            check(a.collision, "a crane is solid");
            // Its legs (14 m apart across the quay, 12 m along it, on the rails at x 2222.5 and 2236.5):
            // walk at one along the quay, and it stops you.
            const float cz = a.position.z;
            for (const float lx : {2222.5F, 2236.5F})
                for (const float lz : {cz - 6, cz + 6}) {
                    Vector3 p{lx, quay + 1, lz - 4};
                    for (int i = 0; i < 200; ++i) p = WorldCollision::resolve_body_movement(p, {p.x, p.y, p.z + .05F}, quay, region, .35F);
                    check(p.z < lz - 1, "a crane's leg stops you (got to " + std::to_string(p.z - lz) + " m from it)");
                }
            check(!WorldCollision::body_blocked({2229.5F, quay + 1, cz}, quay, region, .4F), "the way between its legs is open");
            check(!WorldCollision::body_blocked({2229.5F, quay + 1, cz - 3}, quay, region, .4F), "walk along the quay under it");
        }
        halls += a.model_path.ends_with("civic/arrival_hall.glb");
        gates += a.model_path.ends_with("civic/port_gantry.glb");
        brows += a.model_path.ends_with("civic/ship_brow.glb");
    }
    check(cranes == 2 && halls == 1 && gates == 1 && brows == 1, "two cranes, the arrival hall, the brow, the gate");
    // Trucks on the quay lane pass the cranes' legs.
    for (float z = L::cargo_z - 40; z < L::cargo_z + 15; z += 2)
        for (const float x : {L::quay_lane_x - 5.5F, L::quay_lane_x, L::quay_lane_x + 5.5F})
            check(!WorldCollision::vehicle_blocked({x, quay, z}, region, 1.0F), "the quay lane clear of the cranes at z " + std::to_string(z));
    // Off the brow, through the hall, over the zebra to Uncle Ron's waiting area.
    {
        const auto walk = [&](Vector3 p, Vector3 to) {
            for (int i = 0; i < 2000; ++i) {
                const float dx = to.x - p.x, dz = to.z - p.z, l = std::hypot(dx, dz);
                if (l < .05F) break;
                const float st = std::min(.05F, l);
                p = WorldCollision::resolve_body_movement(p, {p.x + dx / l * st, p.y, p.z + dz / l * st}, p.y, region, .35F);
                p.y = WorldCollision::ground_height(p, p.y + .4F, region);
            }
            return p;
        };
        Vector3 p{hb::brow_foot.x - .8F, quay + .5F, hb::brow_foot.z + 1.2F};
        p.y = WorldCollision::ground_height(p, p.y, region);
        for (const Vector3 to : {Vector3{hb::hall.x, 0, hb::hall.z - 8}, Vector3{hb::hall.x, 0, hb::hall.z}, Vector3{hb::hall.x, 0, hb::hall.z + 8.5F},
                                 Vector3{hb::crossing.x + 7.5F, 0, hb::crossing.z}, Vector3{hb::crossing.x - 7.5F, 0, hb::crossing.z},
                                 Vector3{hb::waiting.x + 1.5F, 0, hb::waiting.z}}) {
            const Vector3 got = walk(p, to);
            check(std::hypot(got.x - to.x, got.z - to.z) < .2F, "the way out from the ship is clear to " + std::to_string(to.x) + "," + std::to_string(to.z) +
                  " (stuck at " + std::to_string(got.x) + "," + std::to_string(got.z) + ")");
            p = got;
        }
        check(std::abs(p.y - quay) < .4F, "on the quay by the waiting area");
    }
    // The gate: rigs drive in under it, its posts and gatehouse are solid.
    const float road_y = TerrainHeight::sample(hb::gate_x, L::cargo_z);
    for (const float dz : {-5.5F, 0.0F, 5.5F})
        check(!WorldCollision::vehicle_blocked({hb::gate_x, road_y, L::cargo_z + dz}, region, 1.2F), "in through the gate");
    {
        Vector3 p{hb::gate_x - 3, road_y + 1, L::cargo_z - 9};
        for (int i = 0; i < 120; ++i) p = WorldCollision::resolve_body_movement(p, {p.x + .05F, p.y, p.z}, road_y, region, .35F);
        check(p.x < hb::gate_x - .5F, "its post stops you");
    }
    std::cout << "Porto Luma's harbour: Stella Maris weekly, alongside Monday " << hb::berthed_from / 60 << ":00-" << hb::sails_at / 60
              << ":00; cranes solid, the way to Uncle Ron clear: GREEN\n";
}
