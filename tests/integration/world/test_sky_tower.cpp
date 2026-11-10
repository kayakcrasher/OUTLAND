// Kristala Turo (world/SkyTower.hpp): Verda's tallest building, on the block east of the highway at
// the foot of Norda Kvartalo. On the world map: in off the plaza through the lobby's doors to the
// elevator; up top the penthouse floor is level and walkable from the elevator out to the terrace,
// and its balustrade keeps you from the edge; the elevator's call points find themselves; no model
// on the island stands taller. And the penthouse is for sale for 100000 kronoj.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/economy/SkyPenthouse.hpp"
#include "outland/world/SkyTower.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
namespace tw = outland::world::tower;
using world::physics::WorldCollision;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::string at(Vector3 p) { return std::to_string(p.x) + "," + std::to_string(p.y) + "," + std::to_string(p.z); }
}

int main() {
    // The penthouse: 100000 kronoj, bought once.
    {
        game::economy::EconomyState wallet;
        wallet.money = 99999;
        game::economy::SkyPenthouse::buy(wallet);
        check(!game::economy::SkyPenthouse::owned(wallet) && wallet.money == 99999, "too poor for the penthouse");
        wallet.money = 150000;
        game::economy::SkyPenthouse::buy(wallet);
        check(game::economy::SkyPenthouse::owned(wallet) && wallet.money == 50000, "the penthouse costs 100000");
        game::economy::SkyPenthouse::buy(wallet);
        check(wallet.money == 50000 && wallet.properties.size() == 1, "bought only once");
    }

    world::terrain::TerrainHeight::set_coastal_layout(true);
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    tw::Site site;
    check(tw::find(region, site), "Kristala Turo is on the map");
    check(std::abs(site.origin.x - tw::site.x) < .01F && std::abs(site.origin.z - tw::site.z) < .01F && std::abs(site.yaw - tw::site_yaw) < .01F,
          "on its site, its front to the south");
    // Round trip of the site's frame.
    {
        const Vector3 l{3, 4, -5}, back = site.to_local(site.to_world(l));
        check(std::abs(back.x - l.x) + std::abs(back.y - l.y) + std::abs(back.z - l.z) < 1e-3F, "to_local undoes to_world");
        check(site.to_world({0, 0, -20}).z > site.origin.z, "the plaza to the south");
    }
    // The tallest on the island.
    float tower_top = 0, tallest_else = 0;
    for (const auto& town : region.settlements())
        for (const auto& a : town.assets) {
            const float top = a.position.y + a.size.y;
            if (a.model_path == tw::model) tower_top = top;
            else tallest_else = std::max(tallest_else, top - a.position.y);
        }
    check(tower_top - site.origin.y > tallest_else, "no model taller than Kristala Turo (" + std::to_string(tallest_else) + " m)");

    const auto walk = [&](Vector3 p, Vector3 to) {
        for (int i = 0; i < 4000; ++i) {
            const float dx = to.x - p.x, dz = to.z - p.z, l = std::hypot(dx, dz);
            if (l < .05F) break;
            const float st = std::min(.05F, l);
            p = WorldCollision::resolve_body_movement(p, {p.x + dx / l * st, p.y, p.z + dz / l * st}, p.y, region, .35F);
            p.y = WorldCollision::ground_height(p, p.y + .4F, region);
        }
        return p;
    };
    // In off the plaza, through the doors, to the elevator.
    {
        Vector3 p = site.to_world({0, 0, -22});
        p.y = WorldCollision::ground_height(p, site.origin.y + 2, region);
        for (const Vector3 l : {Vector3{0, 0, -tw::half - 3}, Vector3{0, 0, -tw::half + 2}, Vector3{0, 0, -4}, Vector3{tw::lobby_call.x, 0, tw::lobby_call.z}}) {
            const Vector3 to = site.to_world(l), got = walk(p, to);
            check(std::hypot(got.x - to.x, got.z - to.z) < .2F, "the way in to the elevator is clear to local z " + std::to_string(l.z) + " (stuck at " + at(site.to_local(got)) + ")");
            p = got;
        }
        check(site.in_lobby(p), "in the lobby (" + at(site.to_local(p)) + ")");
        bool up = false;
        check(site.at_elevator(p, up) && up, "at the elevator's doors in the lobby");
        // The lobby's walls keep you in: sideways into its side wall.
        Vector3 q = p;
        for (int i = 0; i < 600; ++i) {
            const Vector3 l = site.to_local(q), to = site.to_world({l.x + .05F, l.y, l.z});
            q = WorldCollision::resolve_body_movement(q, to, q.y, region, .35F);
        }
        check(std::abs(site.to_local(q).x) < tw::half, "the lobby's walls stop you (" + at(site.to_local(q)) + ")");
    }
    // Up top: the floor at the elevator, level all over the suite and the terrace.
    {
        const Vector3 call = site.to_world(tw::penthouse_call);
        Vector3 p = call;
        p.y = WorldCollision::ground_height(p, site.origin.y + tw::penthouse_floor + 1, region);
        check(std::abs(p.y - site.origin.y - tw::penthouse_floor) < .1F, "the penthouse floor at the elevator (" + std::to_string(p.y) + ")");
        check(site.in_penthouse(p), "the elevator's top doors open into the penthouse");
        bool up = true;
        check(site.at_elevator(p, up) && !up, "at the elevator's doors up top");
        for (const Vector3 l : {Vector3{0, 0, -2}, Vector3{0, 0, -5.5F}, Vector3{6, 0, -9.5F}, Vector3{-10.5F, 0, -10.5F}, Vector3{-10.5F, 0, 10.5F},
                                Vector3{tw::bed.x + 2.2F, 0, tw::bed.z - 3}, Vector3{10.5F, 0, 10.5F}}) {
            const Vector3 g = site.to_world({l.x, tw::penthouse_floor + 1, l.z});
            const float y = WorldCollision::ground_height(g, g.y, region);
            check(std::abs(y - site.origin.y - tw::penthouse_floor) < .15F, "level floor up top at " + at(l) + " (" + std::to_string(y) + ")");
        }
        // Out onto the terrace through the suite's front (past the sofa, between the pool and the
        // loungers), then on toward the edge: the balustrade stops you.
        for (const Vector3 l : {Vector3{0, 0, -4}, Vector3{1, 0, -6.2F}, Vector3{4, 0, -7.6F}, Vector3{4, 0, -10.5F}}) {
            const Vector3 to = site.to_world(l), got = walk(p, to);
            check(std::hypot(got.x - to.x, got.z - to.z) < .2F, "out to the terrace at " + at(l) + " (stuck at " + at(site.to_local(got)) + ")");
            p = got;
        }
        const Vector3 edge = walk(p, site.to_world({4, 0, -20}));
        check(std::abs(site.to_local(edge).z) < tw::terrace + .2F && site.in_penthouse(edge), "the balustrade keeps you on the terrace (" + at(site.to_local(edge)) + ")");
        // And the bed, to sleep in.
        Vector3 b = site.to_world({tw::bed.x + 2.2F, 0, tw::bed.z - 3});
        b.y = site.origin.y + tw::penthouse_floor;
        check(site.in_penthouse(b), "the bedroom is in the penthouse");
    }
    std::cout << "Kristala Turo: " << tower_top - site.origin.y << " m, the lobby open, the elevator to the penthouse floor at "
              << tw::penthouse_floor << " m, " << game::economy::SkyPenthouse::price << " kronoj: GREEN\n";
}
