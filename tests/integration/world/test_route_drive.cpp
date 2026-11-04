// The roads, driven: a big rig drives the ring's interchanges (every slip road off and on, straight
// across each roundabout and round it) and La Ringo's lots on the world map, steering by the road
// graph's routes the way the traffic does (OUTLAND_DRIVE_ALL=1: a car too, and the way from the
// capital to every town).
// Wherever one gets stuck, drops off a ledge, runs into something or is pushed off its line, that
// is a place a player gets into trouble too.
// OUTLAND_DRIVE_LOG=1 prints every incident.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/roads/HighwayNetwork.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace outland;
namespace hw = world::highways;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }

struct Incident { std::string kind, route; Vector3 at; };
std::vector<Incident> incidents;
void report(const std::string& kind, const std::string& route, Vector3 at) {
    for (const auto& i : incidents) if (i.kind == kind && flat(i.at, at) < 20) return;   // once a spot
    incidents.push_back({kind, route, at});
    if (std::getenv("OUTLAND_DRIVE_LOG")) std::cout << "  " << kind << " on " << route << " at " << at.x << "," << at.y << "," << at.z << '\n';
}

// Drive `id` (already at the wheel) along `route`; incidents are reported. Returns how far it got.
float drive(game::vehicles::VehicleSystem& cars, world::VerdaRegion& region, const std::string& id, const world::roads::Route& route,
            const std::string& name, float top) {
    if (!route.complete || route.points.size() < 2) { report("no route", name, route.points.empty() ? Vector3{} : route.points.front()); return 0; }
    float yaw = 0;
    // (Set down in the lane a little way in, and done a little before the end: a trip's ends are off
    // the road, joined to it across its centre line.)
    const float begin = std::min(30.0F, route.length * .25F);
    const Vector3 start = world::roads::route_point(route, begin, &yaw);
    // (A car is only set down from outside it: out, placed, back in.)
    const auto set_down = [&](Vector3 at, float heading) {
        Vector3 out;
        if (cars.driver()) cars.exit(region, out, true);
        cars.place(region, id, at, heading);
        Vector3 feet = cars.asset(region, id)->position;
        return cars.enter(region, cars.find(id), feet);
    };
    if (!set_down(start, yaw)) { report("can't set down", name, start); return 0; }
    // A full tank and a sound engine for every trip (the audit is of the roads, not the car).
    if (auto* a = cars.asset(region, id)) { a->vehicle.fuel = 1; a->vehicle.engine = 100; a->vehicle.health = 100; }
    float progress = begin, best = begin, since = 0;
    Vector3 last = cars.asset(region, id)->position;
    const int frames = static_cast<int>((route.length / 4 + 30) * 60);
    for (int f = 0; f < frames; ++f) {
        const auto* car = cars.asset(region, id);
        progress = world::roads::route_progress(route, car->position, progress);
        if (progress > route.length - std::min(25.0F, route.length * .2F)) return route.length;
        // Slower into the bends: how far the road turns over the next 40 m.
        float y1 = 0, y2 = 0;
        world::roads::route_point(route, progress + 8, &y1);
        world::roads::route_point(route, progress + 40, &y2);
        const float bend = std::abs(std::remainder(y2 - y1, 360.0F));
        const float want_speed = std::clamp(top * (1 - bend / 110), 6.0F, top);
        const Vector3 target = world::roads::route_point(route, progress + std::max(10.0F, top * .7F));
        const float want = std::atan2(target.x - car->position.x, target.z - car->position.z) * RAD2DEG;
        const float error = std::remainder(want - car->rotation_y, 360.0F);
        const float speed = cars.driver() ? cars.driver()->speed : 0;
        const float throttle = speed < want_speed ? 1.0F : speed > want_speed + 3 ? -.6F : 0.0F;
        cars.update(1 / 60.0F, {throttle, std::clamp(error / 25.0F, -1.0F, 1.0F)}, region, car->position);
        car = cars.asset(region, id);
        const Vector3 now = car->position;
        // A drop: more than a kerb in one step.
        if (last.y - now.y > .9F) report("drop", name, now);
        if (now.y - last.y > .9F) report("jump", name, now);
        if (cars.events().collision && speed > 4) {
            report("hit", name, now);
            // OUTLAND_DRIVE_WHY=1: what is solid within reach of it.
            if (std::getenv("OUTLAND_DRIVE_WHY")) {
                // The footprint a little ahead: which points are blocked, and by what.
                const auto* def = cars.definition(*car);
                const float yr = car->rotation_y * DEG2RAD;
                const Vector3 fwd{std::sin(yr), 0, std::cos(yr)}, right{std::cos(yr), 0, -std::sin(yr)};
                for (const float u : {-.5F, 0.0F, .5F})
                    for (const float v : {-.5F, 0.0F, .5F}) {
                        Vector3 q{now.x + right.x * u * def->width + fwd.x * (v * def->length + .6F), now.y, now.z + right.z * u * def->width + fwd.z * (v * def->length + .6F)};
                        float deck = 0;
                        const bool on_deck = hw::deck(q.x, q.z, deck) && now.y > deck - 2;
                        q.y = on_deck ? deck : world::terrain::TerrainHeight::sample(q.x, q.z);
                        const bool barrier = hw::network(region).barrier_hit(q, .2F);
                        const bool any = world::physics::WorldCollision::vehicle_blocked(q, region, .2F, id);
                        if (barrier || any) std::cout << "    blocked at " << u << "," << v << " (" << q.x << "," << q.y << "," << q.z << ")" << (barrier ? " highway barrier" : " asset") << '\n';
                    }
                {
                    const Vector3 ahead{now.x + fwd.x * .8F, now.y, now.z + fwd.z * .8F};
                    std::cout << "    blocked() here " << cars.blocked(region, now, car->rotation_y, *def, 1, id) << " ahead " << cars.blocked(region, ahead, car->rotation_y, *def, 1, id)
                              << " footprint " << world::physics::WorldCollision::vehicle_footprint_blocked(ahead, car->rotation_y, def->width * .5F, def->length * .5F, region, id) << '\n';
                }
                // Another vehicle in reach; the ground at the front bumper (a sheer rise is met as a wall).
                for (const auto& other : cars.vehicles())
                    if (other.id != id && flat(other.position, now) < 12)
                        std::cout << "    vehicle " << other.id << " " << flat(other.position, now) << " m away\n";
                for (const float u : {-.5F, 0.0F, .5F}) {
                    const Vector3 bumper{now.x + right.x * u * def->width + fwd.x * def->length * .5F, 0, now.z + right.z * u * def->width + fwd.z * def->length * .5F};
                    const Vector3 beyond{bumper.x + fwd.x * .4F, 0, bumper.z + fwd.z * .4F};
                    const float rise = world::terrain::TerrainHeight::sample(beyond.x, beyond.z) - world::terrain::TerrainHeight::sample(bumper.x, bumper.z);
                    if (rise > .3F) std::cout << "    a rise of " << rise << " m at the bumper (" << beyond.x << "," << beyond.z << ")\n";
                }
                std::cout << "    ground under: " << world::terrain::TerrainHeight::sample(now.x, now.z) << " car y " << now.y << " yaw " << car->rotation_y << '\n';
            }
            if (std::getenv("OUTLAND_DRIVE_WHY"))
                for (const auto& town : region.settlements())
                    for (const auto& a : town.assets)
                        if (a.collision && a.id != id && flat(a.position, now) < std::max(a.size.x, a.size.z) * .5F + 4)
                            std::cout << "    near: " << a.id << " " << a.model_path << " at " << a.position.x << "," << a.position.y << "," << a.position.z
                                      << " size " << a.size.x << "x" << a.size.y << "x" << a.size.z << " " << flat(a.position, now) << " m\n";
        }
        // Off its line (a wall, a kerb or a post pushing it out of the lane).
        const Vector3 line = world::roads::route_point(route, progress, nullptr, false);
        if (flat(line, now) > 14) report("off course", name, now);
        if (std::getenv("OUTLAND_DRIVE_TRACE") && f % 20 == 0) {
            float dk = 0;
            const bool on_deck = hw::deck(now.x, now.z, dk);
            std::cout << "    t " << f / 60.0F << " at " << now.x << "," << now.y << "," << now.z << " terrain " << world::terrain::TerrainHeight::sample(now.x, now.z)
                      << (on_deck ? " deck " + std::to_string(dk) : std::string()) << " r " << std::hypot(now.x, now.z) - 1500 << " speed " << speed << '\n';
        }
        last = now;
        if (progress > best + 1) { best = progress; since = 0; }
        else if ((since += 1 / 60.0F) > 4) {
            report("stuck", name, now);
            // On past it, to audit the rest of the way.
            progress = best + 25;
            if (progress > route.length - std::min(25.0F, route.length * .2F)) return best;
            const Vector3 on = world::roads::route_point(route, progress, &yaw);
            set_down(on, yaw);
            if (auto* a = cars.asset(region, id)) { a->vehicle.fuel = 1; a->vehicle.engine = 100; a->vehicle.health = 100; }
            last = cars.asset(region, id)->position;
            best = progress; since = 0;
        }
    }
    report("too slow", name, cars.asset(region, id)->position);
    return best;
}
} // namespace

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    const world::roads::RoadGraph graph(region);
    game::vehicles::VehicleRegistry types;
    std::string error;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem cars(types);
    cars.set_training_structure(false);

    // The routes: every slip road on and off (to and from the roundabout's far side), straight
    // across each roundabout both ways, La Ringo's lots, and the capital to every town.
    struct Trip { std::string name; Vector3 from, to; };
    std::vector<Trip> trips;
    const auto along = [](const std::vector<Vector3>& pts, std::size_t k) { return pts[std::min(k, pts.size() - 1)]; };
    for (const auto& x : hw::interchanges()) {
        const std::string n = "exit " + std::to_string(x.number);
        const Vector3 far_in{x.at.x - x.outward.x * 140, 0, x.at.z - x.outward.z * 140}, far_out{x.at.x + x.outward.x * 140, 0, x.at.z + x.outward.z * 140};
        // (From the lane, not the centre line: traffic keeps right.)
        const Vector3 right_in{x.outward.z, 0, -x.outward.x};   // to the right heading inward
        const auto lane = [&](Vector3 p, Vector3 right) { return Vector3{p.x + right.x * hw::slow_lane, 0, p.z + right.z * hw::slow_lane}; };
        trips.push_back({n + " across inward", lane(far_out, right_in), lane(far_in, right_in)});
        trips.push_back({n + " across outward", lane(far_in, {-right_in.x, 0, -right_in.z}), lane(far_out, {-right_in.x, 0, -right_in.z})});
        for (const auto& slip : x.slips) {
            const Vector3 ring_end = slip.off ? slip.points.front() : slip.points.back();
            // On the ring, in its slow lane on this slip road's side (inside the ring for forward traffic).
            const auto in_lane = [&](Vector3 p) {
                const float r = std::hypot(p.x, p.z), lateral = slip.inside ? -hw::slow_lane : hw::slow_lane;
                return Vector3{p.x * (r + lateral) / r, 0, p.z * (r + lateral) / r};
            };
            const Vector3 back = in_lane({ring_end.x - x.ring_forward.x * (slip.inside ? 1.0F : -1.0F) * 120, 0, ring_end.z - x.ring_forward.z * (slip.inside ? 1.0F : -1.0F) * 120});
            const Vector3 ahead = in_lane({ring_end.x + x.ring_forward.x * (slip.inside ? 1.0F : -1.0F) * 120, 0, ring_end.z + x.ring_forward.z * (slip.inside ? 1.0F : -1.0F) * 120});
            const std::string s = n + (slip.off ? " off " : " on ") + (slip.inside ? "inside" : "outside");
            if (slip.off) {
                trips.push_back({s + " to the town", back, far_out});
                trips.push_back({s + " to the island", back, far_in});
            } else {
                trips.push_back({s + " from the town", far_out, ahead});
                trips.push_back({s + " from the island", far_in, ahead});
            }
        }
    }
    for (const auto& rest : hw::rest_areas()) {
        const std::string n = std::string("La Ringo ") + std::string(rest.name);
        trips.push_back({n + " in", along(rest.off, 0), rest.lot});
        trips.push_back({n + " out", rest.lot, along(rest.on, rest.on.size())});
        trips.push_back({n + " through", along(rest.off, 0), along(rest.on, rest.on.size())});
    }
    Vector3 capital{};
    for (const auto& town : region.settlements()) if (town.id == "capital_verda") capital = town.center;
    // (The whole sweep - a car as well as the rig, and the way to every town - takes ten minutes:
    // OUTLAND_DRIVE_ALL=1. By default, the rig, the hardest to get through, on the new roads.)
    const bool all = std::getenv("OUTLAND_DRIVE_ALL") != nullptr;
    if (all)
        for (const auto& town : region.settlements())
            if (!town.roads.empty() && town.id != "capital_verda" && flat(town.center, capital) > 300)
                trips.push_back({"the capital to " + town.id, graph.nearest(capital).point, graph.nearest(town.center).point});   // (from road to road)

    const char* only = std::getenv("OUTLAND_DRIVE_ONLY");
    int driven = 0;
    std::vector<const char*> kinds{"semi"};
    if (all) kinds.insert(kinds.begin(), "hatchback");
    for (const char* type : kinds) {
        std::string id;
        for (const auto& trip : trips) {
            if (only && trip.name.find(only) == std::string::npos) continue;
            const auto route = graph.route(trip.from, trip.to);
            if (id.empty()) {
                float yaw = 0;
                const Vector3 p = world::roads::route_point(route, 2, &yaw);
                check(cars.spawn(region, p, yaw, type), std::string("a ") + type);
                id = cars.last_spawned();
                Vector3 feet = cars.asset(region, id)->position;
                check(cars.enter(region, cars.find(id), feet), std::string("at the wheel of a ") + type);
            }
            drive(cars, region, id, route, std::string(type) + ": " + trip.name, std::string(type) == "semi" ? 16.0F : 24.0F);
            ++driven;
        }
        Vector3 out;
        cars.exit(region, out, true);
    }
    std::cout << driven << " trips driven, " << incidents.size() << " incidents\n";
    int serious = 0;
    for (const auto& i : incidents) {
        const bool bad = i.kind == "stuck" || i.kind == "drop" || i.kind == "no route" || i.kind == "too slow";
        serious += bad;
        if (bad || std::getenv("OUTLAND_DRIVE_LOG")) std::cout << "  " << i.kind << ": " << i.route << " at " << i.at.x << "," << i.at.y << "," << i.at.z << '\n';
    }
    check(serious == 0, "the roads drive clean: " + std::to_string(serious) + " places a vehicle gets stuck, drops or can't get through");
    if (failures == 0) std::cout << "route drive passed\n";
    return failures == 0 ? 0 : 1;
}
