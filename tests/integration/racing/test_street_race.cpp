// Street racing on the ring road: the track from the meet a quarter of the way round, the bet, the
// grid and countdown, the field driving its lanes flat out, the flares, the result and the pot.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/racing/StreetRace.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/MotorTrade.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <cmath>
#include <iostream>
using namespace outland;
using game::racing::Result;
using game::racing::Stage;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}
int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    std::string error;
    game::vehicles::VehicleRegistry types;
    check(types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem cars(types);

    // The ring, and the meet at the Marnafto station on it.
    std::vector<Vector3> ring;
    for (const auto& line : world::highways::lines()) if (line.loop) ring = line.points;
    Vector3 meet{};
    float best = 1e9F;
    for (const auto& site : world::motor::sites(region))
        if (site.kind == world::motor::Kind::Gas) {
            const float off = std::abs(std::hypot(site.origin.x, site.origin.z) - 1500);
            if (off < best) { best = off; meet = site.origin; }
        }
    check(best < 200, "a station on the ring");
    game::racing::StreetRace race;
    check(race.setup(ring, meet) && race.ready(), "the track");
    check(race.length() > 2000 && race.length() < 2700 && flat(race.start(), meet) < 120, "a quarter of the ring from the meet: " + std::to_string(race.length()));
    check(race.checkpoints().size() >= 5, "flares along the way");

    // Three cars at the meet.
    std::vector<game::racing::Racer> field;
    const char* kinds[]{"muscle", "sedan", "hatchback"};
    for (int i = 0; i < 3; ++i) {
        const Vector3 at{meet.x + 8.0F * static_cast<float>(i), 0, meet.z + 6};
        check(cars.spawn(region, at, 0, kinds[i]), std::string("spawn ") + kinds[i]);
        game::racing::Racer r;
        r.vehicle = cars.last_spawned();
        r.name = kinds[i];
        r.pace = 38.0F + 3.0F * static_cast<float>(i);
        r.parked = at;
        field.push_back(r);
    }
    race.set_racers(field);

    game::racing::Frame f;
    f.dt = .05F;
    f.hour = 12;
    f.car = meet;
    race.update(f, cars, region);
    check(race.stage() == Stage::Closed, "closed by day");
    bool paid = false;
    check(race.talk(5000, paid).find("after nine") != std::string::npos && !paid, "come back at night");
    f.hour = 22;
    race.update(f, cars, region);
    check(race.stage() == Stage::Open, "open at night");
    check(race.talk(5000, paid).find("thousand") != std::string::npos && !paid && race.stage() == Stage::Offered, "the stakes");
    check(race.talk(200, paid).find("Come back") != std::string::npos && !paid && race.stage() == Stage::Open, "no money, no race");
    race.talk(5000, paid);
    check(race.talk(5000, paid).find("line up") != std::string::npos && paid && race.stage() == Stage::Lining, "the bet's on");

    // Lining up: the field rolls up to the grid; the player stops in the first slot; three, two, one.
    f.driving = true;
    f.car = race.grid(0);
    f.speed = 0;
    for (int i = 0; i < 5; ++i) { race.update(f, cars, region); cars.update(f.dt, {}, region, f.car); }
    check(race.stage() == Stage::Countdown, "countdown");
    for (std::size_t i = 0; i < race.racers().size(); ++i) {
        const int v = cars.find(race.racers()[i].vehicle);
        check(v >= 0 && flat(cars.vehicles()[static_cast<std::size_t>(v)].position, race.grid(static_cast<int>(i) + 1)) < 4, "on the grid");
    }
    for (int i = 0; i < 70; ++i) { race.update(f, cars, region); cars.update(f.dt, {}, region, f.car); }
    check(race.stage() == Stage::Racing, "go");

    // The player sits on the line: the field drives away flat out and finishes; the player loses.
    float fastest = 0;
    for (int i = 0; i < 20 * 140 && race.stage() == Stage::Racing; ++i) {
        race.update(f, cars, region);
        cars.update(f.dt, {}, region, f.car);
        for (const auto& v : cars.vehicles()) fastest = std::max(fastest, v.speed);
    }
    float furthest = 0;
    for (const auto& r : race.racers()) {
        furthest = std::max(furthest, r.progress);
        std::cout << "  " << r.name << ": " << r.progress << " m" << (r.finished ? ", finished" : "") << ", attempts " << r.attempts << '\n';
    }
    std::cout << "fastest " << fastest << " m/s, furthest " << furthest << " m of " << race.length() << ", winner " << race.winner() << '\n';
    check(fastest > 25, "they race");
    check(furthest > race.length(), "the field gets round");
    check(!race.winner().empty() && race.winner() != "you", "one of them wins");
    const auto result = race.take_result();
    check(result == Result::Lost || result == Result::Forfeit, "the player lost the stake");

    // Done: back to the meet when the player's away; a new race.
    f.car = {0, 0, 0};
    for (int i = 0; i < 20 * 25; ++i) race.update(f, cars, region);
    check(race.stage() == Stage::Open, "the boys are back at the meet");
    race.talk(5000, paid);
    race.talk(5000, paid);
    f.car = race.grid(0);
    for (int i = 0; i < 80; ++i) race.update(f, cars, region);
    check(race.stage() == Stage::Racing, "a second race");
    // This time the player flies through every flare.
    for (const float d : race.checkpoints()) {
        f.car = world::roads::route_point(race.track(), d);
        race.update(f, cars, region);
    }
    check(race.take_result() == Result::Won && race.winner() == "you" && race.pot() == 4 * game::racing::StreetRace::bet, "first past the last flare takes the pot");

    if (failures == 0) std::cout << "street race ok\n";
    return failures == 0 ? 0 : 1;
}
