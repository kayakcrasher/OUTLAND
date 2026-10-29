// The outbreak (Zombie Survival's story): the scenes in order, the holdouts and the black tower on
// the shipped map, fuel that runs out, the red map's clearings, the save file, and the ecology's
// story infected and safe ground.
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/outbreak/Outbreak.hpp"
#include "outland/game/zombies/ZombieEcology.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace outland;
using namespace outland::game::outbreak;

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

int main() {
    // The scenes.
    Story story;
    story.start(Stage::WakeUp);
    check(story.fade() == 1 && !story.talking(), "the screen starts black");
    story.update(1);
    check(story.fade() > 0 && story.fade() < 1, "eyes opening");
    story.update(2);
    check(story.talking() && story.line() && std::string(story.line()->speaker) == Story::ron, "Ron is talking as the eyes open");
    check(story.objective(0).find("Uncle Ron") != std::string::npos, "listen to him");
    for (int k = 0; k < 20 && story.stage() == Stage::WakeUp; ++k) story.talk();
    check(story.stage() == Stage::ToTruck && !story.talking() && story.take_changed(), "to the pickup");
    check(story.objective(0).find("pickup") != std::string::npos, "get in the pickup");
    story.boarded();
    check(story.stage() == Stage::Drive && story.objective(0).find("Ranĉo Verdaj Kampoj") != std::string::npos, "drive to the barn");
    story.arrived();
    check(story.stage() == Stage::Standoff && story.talking() && std::string(story.line()->speaker) == Story::dario, "Dario at the barn");
    bool fired = false, rose = false;
    for (int k = 0; k < 400 && !rose; ++k) {
        story.update(.1F);
        const auto cue = story.take_cue();
        if (cue == Cue::Fire) { check(!rose, "the shots before the rising"); fired = true; }
        if (cue == Cue::Rise) rose = true;
    }
    check(fired && rose && story.stage() == Stage::Clear, "the standoff plays itself: shots, then they get up");
    check(story.objective(9) == "Clear the farm: 9 infected left", "clear the farm");
    story.cleared();
    check(story.stage() == Stage::Free && story.take_cue() == Cue::Claimed && story.talking(), "the barn is the hideout");
    for (int k = 0; k < 10; ++k) story.talk();
    check(!story.talking() && story.objective(0).empty(), "the island is the player's");
    check(std::string(Story::free_line(0).text) != Story::free_line(1).text, "words in free play, in turn");

    // The save.
    OutbreakState state;
    state.stage = Stage::Free;
    state.has_position = true;
    state.position = {12.5F, 3, -40};
    state.yaw = 90;
    state.truck = "creator_vehicle_7";
    state.found = {"lupoj", "policio"};
    state.talks = 3;
    state.tower_seen = true;
    draw_fuel(state, 2, {10, 0, 20}, .4F);
    OutbreakState back;
    check(parse(serialise(state), back), "parse the save");
    check(back.stage == Stage::Free && back.truck == state.truck && back.found == state.found && back.talks == 3 && back.tower_seen &&
          back.fuel.size() == 1 && std::abs(back.position.x - 12.5F) < 1e-3F, "the save round trips");
    OutbreakState mid;
    mid.stage = Stage::Standoff;
    check(parse(serialise(mid), back) && back.stage == Stage::Drive, "a save from mid-scene plays the scene again");
    check(!parse("nonsense", back), "not a save");

    // Fuel runs out.
    OutbreakState fuel;
    const Vector3 pump{300, 0, -120};
    const float stock = fuel_left(fuel, 4, pump);
    check(stock >= .3F && stock <= 3.5F && stock == initial_fuel(pump), "a station holds a few tankfuls at most");
    float drawn = 0;
    for (int k = 0; k < 20; ++k) drawn += draw_fuel(fuel, 4, pump, .7F);
    check(std::abs(drawn - stock) < 1e-4F && fuel_left(fuel, 4, pump) == 0, "every fill comes out of it, until it is dry");
    check(draw_fuel(fuel, 4, pump, 1) == 0, "a dry station is done");
    check(fuel_left(fuel, 5, {-900, 0, 40}) == initial_fuel({-900, 0, 40}), "the others untouched");

    // The world.
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    const auto sites = find_holdouts(region);
    std::cout << sites.size() << " holdouts\n";
    check(sites.size() == holdouts.size(), "every holdout is on the map");
    Vector3 capital{};
    for (const auto& town : region.settlements()) if (town.id == "capital_verda") capital = town.center;
    for (const auto& h : sites) {
        const auto& spec = holdouts[static_cast<std::size_t>(h.spec)];
        std::cout << "  " << spec.name << " at " << h.centre.x << "," << h.centre.z << '\n';
        if (spec.id != "frateco") check(flat(h.centre, capital) > 600, std::string(spec.name) + " is out of Verda City");
    }
    const auto tower = find_tower(region);
    check(tower.found && flat(tower.centre, capital) < 200 && tower.size.y > 60, "the black tower downtown");
    // The red map: clear round the player, the hideout (once it is) and holdouts found.
    OutbreakState seen;
    seen.found = {"policio"};
    auto clear = clearings(seen, capital, {0, 0, -600}, sites);
    check(clear.size() == 2 && clear.front().radius == sight_radius, "round the player and the one holdout found");
    seen.stage = Stage::Free;
    clear = clearings(seen, capital, {0, 0, -600}, sites);
    check(clear.size() == 3, "and the hideout");

    // Ground zero, the story's infected and safe ground.
    characters::CharacterRegistry characters;
    std::string error;
    check(characters.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/characters/character_manifest.tsv", error), error);
    game::zombies::ZombieEcology plain, outbreak;
    plain.build(region, &characters, 5);
    game::zombies::ZombieConfig config;
    config.capital_nests = 22;
    outbreak.configure(config);
    outbreak.build(region, &characters, 5);
    const auto in_city = [&](const game::zombies::ZombieEcology& e) {
        int n = 0;
        for (const auto& z : e.zombies()) n += flat(z.position, capital) < 420;
        return n;
    };
    std::cout << in_city(plain) << " infected in Verda City normally, " << in_city(outbreak) << " at ground zero\n";
    check(in_city(outbreak) > in_city(plain) * 3 / 2, "Verda City is ground zero");
    characters::NpcSystem npcs;
    outbreak.reset(npcs);
    const Vector3 farm{-60, 6, -610};
    const int risen = outbreak.spawn_at(farm);
    check(!outbreak.dead(risen) && flat(outbreak.zombies()[static_cast<std::size_t>(risen)].position, farm) < .1F, "one of them gets back up");
    outbreak.set_safe({{capital, 120}});
    check(outbreak.safe(capital) && !outbreak.safe(farm), "safe ground");
    int inside = 0;
    for (const auto& z : outbreak.zombies()) inside += !z.physical && flat(z.position, capital) < 120;
    check(inside == 0, "the dead are moved out of safe ground");
    std::cout << "outbreak tests passed\n";
    return 0;
}
