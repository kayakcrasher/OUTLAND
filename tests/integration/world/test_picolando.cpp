// Picolando (world/Picolando.hpp, game/pizza/Picolando.hpp): the pizza chain. On the world map:
// a pizzeria in each of Verda City, Roka, Espera and Suda Haveno, with its delivery cars (the
// Picolando Kuriero) in the lay-by - none of them a resident's. Whose area is whose: inside the
// ring road and at La Ringo only Verda's, Ferarbaro (Sovaĝejo with it) Roka's. A delivery from
// signing on to the doorstep (hot: paid and tipped; cold: paid less, no tip), the next order from
// the nearest pizzeria, picking it up there; an officer on duty turned away; handing in the cap;
// the save. And the chain's three drivers: out to a household, back to the lay-by, never jumping.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/game/pizza/Picolando.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/Picolando.hpp"
#include "outland/world/SovagejoCounty.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
using namespace outland;
namespace pl = outland::world::picolando;
namespace pz = outland::game::pizza;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

int main() {
    world::terrain::TerrainHeight::set_coastal_layout(true);
    // Whose area is whose.
    const auto site = [](const char* id) { return world::layout::find(id)->center; };
    check(pl::serving(0, 0) == pl::Shop::Verda && pl::serving(900, -900) == pl::Shop::Verda && pl::serving(-1400, 300) == pl::Shop::Verda, "inside the ring: Verda's");
    for (const auto& rest : world::highways::rest_areas())
        check(pl::serving(rest.centre.x, rest.centre.z) == pl::Shop::Verda, "La Ringo's bar and grills: Verda's");
    for (const auto& h : world::layout::hamlets) check(pl::serving(h.center.x, h.center.z) == pl::Shop::Verda, std::string(h.name) + " (on the ring): Verda's");
    check(pl::serving(site("west_roka").x, site("west_roka").z) == pl::Shop::Roka, "Roka: Roka's");
    check(pl::serving(world::layout::sovagejo_center.x, world::layout::sovagejo_center.z) == pl::Shop::Roka, "Sovaĝejo: Roka's");
    check(pl::serving(world::county::village().centre.x, world::county::village().centre.z) == pl::Shop::Roka, "Fajrejo: Roka's");
    check(pl::serving(site("village_espera").x, site("village_espera").z) == pl::Shop::Espera, "Espera: Espera's");
    check(pl::serving(site("south_haven").x, site("south_haven").z) == pl::Shop::SudaHaveno, "Suda Haveno: its own");
    const auto luma = pl::serving(site("port_luma").x, site("port_luma").z);
    check(luma == pl::Shop::Espera || luma == pl::Shop::SudaHaveno, "Porto Luma: from Espera or Suda Haveno");
    // Nobody else delivers inside the ring; Verda never outside it (but at La Ringo).
    for (float x = -2400; x <= 2400; x += 60)
        for (float z = -2400; z <= 2400; z += 60) {
            const float r = std::hypot(x, z);
            if (r < 1500) check(pl::serving(x, z) == pl::Shop::Verda, "only Verda inside the ring");
            bool la_ringo = false;
            for (const auto& rest : world::highways::rest_areas()) la_ringo |= std::hypot(rest.centre.x - x, rest.centre.z - z) < 120;
            if (r > 1540 && !la_ringo) check(pl::serving(x, z) != pl::Shop::Verda, "Verda not outside the ring");
            if (r > 1540 && !la_ringo && world::zones::zone_at(x, z) == world::zones::ZoneId::Ferarbaro) check(pl::serving(x, z) == pl::Shop::Roka, "Ferarbaro: Roka's");
        }

    // The world map: four pizzerias, one to a town, and their cars.
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    const auto pizzerias = pl::find(region);
    check(pizzerias.size() == 4, "four pizzerias");
    std::set<pl::Shop> towns;
    for (const auto& p : pizzerias) {
        towns.insert(p.shop);
        check(flat(p.origin, site(pl::town_ids[static_cast<std::size_t>(p.shop)])) < 420, p.name + " is in its town");
        // Walk in at the door; the lay-by kerb is open road.
        Vector3 in = p.at(0, 0, -6.5F);   // (in at the door, over to the counter)
        in.y = world::physics::WorldCollision::ground_height({in.x, in.y + 2, in.z}, in.y + 2, region);
        for (int i = 0; i < 600 && flat(in, p.counter_point()) > .1F; ++i) {
            const Vector3 to = p.counter_point(), d = {to.x - in.x, 0, to.z - in.z};
            const float l = std::hypot(d.x, d.z), st = std::min(.05F, l);
            in = world::physics::WorldCollision::resolve_body_movement(in, {in.x + d.x / l * st, in.y, in.z + d.z / l * st}, in.y, region, .35F);
            in.y = world::physics::WorldCollision::ground_height(in, in.y + .4F, region);
        }
        {
            const float a = -p.yaw * DEG2RAD, dx = in.x - p.origin.x, dz = in.z - p.origin.z;
            check(flat(in, p.counter_point()) < .2F, "in at " + p.name + "'s counter (stuck at local " + std::to_string(dx * std::cos(a) + dz * std::sin(a)) + "," +
                  std::to_string(-dx * std::sin(a) + dz * std::cos(a)) + " y " + std::to_string(in.y - p.origin.y) + ")");
        }
    }
    check(towns.size() == 4, "one in each of the four cities");
    // The counter is clear of the pizzeria's own till by the door (its store's USE reaches 3.5 m
    // round the place's door, 2 m out from the marker): USE at the counter is Picolando's.
    for (const auto& p : pizzerias) check(flat(p.counter_point(), p.at(pl::door.x, 0, pl::door.y - 2)) > 3.6F, "the counter clear of the store");
    game::vehicles::VehicleRegistry types;
    std::string error;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    check(types.find("pizza") != nullptr, "the Picolando Kuriero");
    game::vehicles::VehicleSystem cars(types);
    cars.reconcile(region);
    int kurieroj = 0;
    for (const auto& car : cars.vehicles())
        if (const auto* a = cars.asset(region, car.id); a && car.id.find("vehicle_spawn_pizza_") != std::string::npos) {
            ++kurieroj;
            check(a->vehicle.definition == "pizza", "a Kuriero in the lay-by");
            check(!world::physics::WorldCollision::vehicle_blocked(a->position, region, 1.0F, a->id), "nothing in a Kuriero's way: " + car.id);
        }
    check(kurieroj == 9, "nine Kurieroj (" + std::to_string(kurieroj) + ")");
    const auto island = game::life::build_island(region, nullptr);
    for (const auto& c : island.cars) check(c.vehicle.find("vehicle_spawn_pizza_") == std::string::npos, "no resident drives a Kuriero home");

    // The households, each with its pizzeria; deliveries.
    std::vector<pz::Customer> homes;
    std::vector<std::string> family(island.places.size());
    for (const auto& r : island.residents)
        if (r.home >= 0 && family[static_cast<std::size_t>(r.home)].empty()) family[static_cast<std::size_t>(r.home)] = r.family_name;
    for (std::size_t i = 0; i < island.places.size(); ++i)
        if (island.places[i].kind == game::life::PlaceKind::Home && !family[i].empty()) homes.push_back({"the " + family[i] + " household", island.places[i].door, -1});
    const auto world = pz::make_world(pizzerias, homes);
    check(world.customers.size() > 100, "households to deliver to");
    std::array<int, 4> per{};
    for (const auto& c : world.customers) {
        check(world.shops[static_cast<std::size_t>(c.shop)].id == pl::serving(c.door.x, c.door.z), "each household on its pizzeria's books");
        ++per[static_cast<std::size_t>(world.shops[static_cast<std::size_t>(c.shop)].id)];
    }
    for (const int n : per) check(n > 3, "every pizzeria has customers");

    pz::Delivery job;
    job.seed(7);
    int verda = -1;
    for (std::size_t i = 0; i < world.shops.size(); ++i) if (world.shops[i].id == pl::Shop::Verda) verda = static_cast<int>(i);
    const Vector3 counter = world.shops[static_cast<std::size_t>(verda)].door;
    check(job.at_counter(counter, world) == verda, "at Verda's counter");
    check(!job.talk(verda, true, 0, world).empty() && !job.hired(), "an officer on duty is turned away");
    job.talk(verda, false, 0, world);
    check(job.hired() && job.order().stage == pz::Stage::Deliver, "signed on: the first order goes out with you");
    check(pl::serving(job.order().to.x, job.order().to.z) == pl::Shop::Verda, "Verda's order goes inside the ring");
    // Drive there (teleport), stop at the kerb outside: paid, tipped.
    double now = 10;
    pz::Frame f;
    f.dt = .1F;
    const auto run = [&](Vector3 at, bool driving, float seconds) {
        pz::Event got = pz::Event::None;
        f.player = at; f.driving = driving; f.speed = 0;
        for (float t = 0; t < seconds && got == pz::Event::None; t += f.dt) { now += f.dt; f.now = now; got = job.update(f, world); }
        return got;
    };
    const Vector3 door = job.order().to;
    const int quoted = job.order().pay;
    check(quoted >= pz::Delivery::base_pay, "paid by the distance");
    check(run({door.x + 3, door.y, door.z}, true, 5) == pz::Event::Delivered, "delivered from the kerb outside");
    const int hot_pay = job.take_pay();
    check(hot_pay >= quoted + pz::Delivery::tip_low && hot_pay <= quoted + pz::Delivery::tip_high, "hot: the pay and a tip (" + std::to_string(hot_pay) + ")");
    // The next order: from the pizzeria nearest the doorstep, picked up there.
    check(job.order().stage == pz::Stage::Pickup && job.order().shop == world.nearest_shop(door), "the next from the nearest pizzeria");
    const auto& shop = world.shops[static_cast<std::size_t>(job.order().shop)];
    check(run({shop.pickup.x, shop.pickup.y, shop.pickup.z}, true, 5) == pz::Event::PickedUp, "picked up in the lay-by");
    check(job.order().stage == pz::Stage::Deliver && pl::serving(job.order().to.x, job.order().to.z) == shop.id, "the order goes to its own area");
    // Dawdle: it goes cold - paid three quarters, no tip; on foot at the door this time.
    const int cold_quote = job.order().pay;
    run({0, 0, 50000}, false, job.order().allowed + 5);
    check(run(job.order().to, false, 5) == pz::Event::Delivered, "delivered at the door on foot");
    check(job.take_pay() == static_cast<int>(std::lround(static_cast<float>(cold_quote) * pz::Delivery::late_share)), "cold: three quarters, no tip");
    check(job.deliveries() == 2 && job.earned() > hot_pay, "two deliveries on the books");
    // Not delivered from too far, or while moving.
    {
        pz::Delivery other = job;
        if (other.order().stage == pz::Stage::Pickup) {
            const auto& s = world.shops[static_cast<std::size_t>(other.order().shop)];
            pz::Frame g; g.dt = .1F; g.driving = true; g.speed = 8; g.player = s.pickup;
            for (int i = 0; i < 50; ++i) { g.now = 1000 + i * .1; check(other.update(g, world) == pz::Event::None, "no pick-up at speed"); }
        }
    }
    // The save keeps the cap and the books; handing in the cap takes asking twice.
    pz::Delivery loaded;
    check(loaded.parse(job.serialise()) && loaded.hired() && loaded.deliveries() == 2 && loaded.earned() == job.earned(), "the save");
    job.talk(verda, false, 500, world);
    check(job.hired(), "asked once: still on");
    job.talk(verda, false, 501, world);
    check(!job.hired(), "asked twice: the cap handed in");

    // The chain's three drivers.
    pz::Fleet fleet;
    check(fleet.build(region, cars, world, nullptr, 3) && fleet.drivers().size() == 3, "three drivers");
    fleet.start(region, cars);
    std::vector<Vector3> last;
    for (const auto& d : fleet.drivers()) last.push_back(d.position);
    for (int step = 0; step < 6000; ++step) {   // ten minutes
        fleet.update(.1F, region, cars, {0, -100, 9000}, nullptr, world);
        for (std::size_t k = 0; k < fleet.drivers().size(); ++k) {
            const auto& d = fleet.drivers()[k];
            check(d.running, d.name + " keeps driving");
            check(flat(d.position, last[k]) < 3, d.name + " never jumps (step " + std::to_string(step) + ": " + std::to_string(flat(d.position, last[k])) + " m; progress " + std::to_string(d.progress) + " of " + std::to_string(d.route.length) + (d.out ? " out" : " back") + ", dwell " + std::to_string(d.dwell) + ", at " + std::to_string(d.position.x) + "," + std::to_string(d.position.z) + ")");
            last[k] = d.position;
        }
    }
    for (const auto& d : fleet.drivers())
        check(d.deliveries >= 1, d.name + " made a delivery (" + std::to_string(d.deliveries) + ")");
    fleet.stop(region, cars);
    std::cout << "Picolando: " << pizzerias.size() << " pizzerias, " << kurieroj << " Kurieroj, " << world.customers.size()
              << " households; a delivery " << hot_pay << " kr hot; drivers made " << fleet.drivers()[0].deliveries << "/"
              << fleet.drivers()[1].deliveries << "/" << fleet.drivers()[2].deliveries << " deliveries: GREEN\n";
}
