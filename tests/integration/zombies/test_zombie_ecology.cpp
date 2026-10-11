#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/sound/SoundBus.hpp"
#include "outland/game/zombies/ZombieEcology.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/navigation/NavGrid.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
using namespace outland;
using namespace outland::game::zombies;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float to_road(const world::VerdaRegion& region, Vector3 p) {
    float best = 1e9F;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
        const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
        const float t = l2 < 1e-6F ? 0 : std::clamp(((p.x - road.start.x) * dx + (p.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
        best = std::min(best, std::hypot(p.x - road.start.x - dx * t, p.z - road.start.z - dz * t) - road.width * .5F);
    }
    return best;
}
}

int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    characters::CharacterRegistry characters; std::string error;
    check(characters.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/characters/character_manifest.tsv", error), error);
    characters::NpcSystem npcs;
    world::navigation::NavGrid nav(region);
    nav.set_max_new_cells(250); nav.set_max_expansions(8000);
    npcs.set_path_finder([&](Vector3 from, Vector3 to, world::navigation::NavPath& path) {return nav.find_path(from, to, path);});

    // Nests round every town (the capital worst) and by the long roads; everyone has a body model.
    ZombieEcology ecology;
    ecology.build(region, &characters, 7);
    const auto& sources = ecology.sources();
    int roadside = 0;
    for (const auto& source : sources) {
        roadside += source.roadside;
        check(world::terrain::TerrainHeight::sample(source.position.x, source.position.z) > world::layout::sea_level, "nests on land");
    }
    std::cout << sources.size() << " nests (" << roadside << " roadside), " << ecology.zombies().size() << " infected\n";
    check(sources.size() >= 20 && roadside >= 3, "nests everywhere");
    for (std::size_t s = 0; s < region.settlements().size(); ++s)
        check(std::count_if(sources.begin(), sources.end(), [&](const ZombieSource& n) {return n.settlement == static_cast<int>(s);}) >= 2,
              "every town is infested: " + region.settlements()[s].name);
    check(static_cast<int>(ecology.zombies().size()) == ecology.config().population, "population");
    for (const auto& z : ecology.zombies()) check(!z.character_id.empty(), "a creature model each");
    {
        ZombieEcology again; again.build(region, &characters, 7);
        for (std::size_t i = 0; i < sources.size(); ++i) check(flat(again.sources()[i].position, sources[i].position) < .01F, "deterministic nests");
    }

    const Vector3 far_away{0, 0, 9000};
    double now = 0;
    const auto run = [&](float seconds, float daylight, Vector3 player, const std::function<void()>& each = {}) {
        characters::NpcContext context; context.player_position = player;
        for (float t = 0; t < seconds; t += .05F) {
            now += .05;
            npcs.update(.05F, context, region);
            ecology.update(.05F, {player, daylight, now}, npcs);
            if (each) each();
        }
    };
    ecology.reset(npcs);

    // Day: many lie dormant; no hordes. Slow when they move.
    run(90, 1, far_away);
    auto stats = ecology.stats();
    std::cout << "day: " << stats.dormant << " dormant of " << stats.alive << ", " << stats.hordes << " hordes\n";
    check(stats.dormant > stats.alive / 4 && stats.hordes == 0, "dormant by day");
    check(npcs.tuning(characters::CharacterPool::Creature).movement_speed < 2, "slow by day");

    // Night: all awake, faster, and hordes walk the roads between towns.
    float worst_off_road = 0;
    run(150, 0, far_away, [&] {
        // The first and last legs join the road from a nest or to a town square; judge the walk between.
        for (const auto& horde : ecology.hordes()) if (horde.progress > 45 && horde.progress < horde.route.length - 45)
            worst_off_road = std::max(worst_off_road, to_road(region, horde.position));
    });
    stats = ecology.stats();
    std::cout << "night: " << stats.dormant << " dormant, " << stats.hordes << " hordes, " << stats.in_hordes << " in hordes\n";
    check(stats.dormant == 0, "awake at night");
    check(stats.hordes >= 1 && stats.in_hordes >= 4, "hordes at night");
    check(worst_off_road < 2, "hordes keep to the roads: " + std::to_string(worst_off_road));
    check(npcs.tuning(characters::CharacterPool::Creature).movement_speed > 2.5F, "faster at night");
    // Dawn breaks them up.
    run(1, 1, far_away);
    check(ecology.hordes().empty() && ecology.stats().in_hordes == 0, "hordes break up at dawn");

    // A gunshot sends whoever hears it to look; zombies out of earshot carry on.
    {
        game::sound::SoundBus bus;
        const Vector3 shot = Vector3Add(sources.front().position, {20, 0, 0});
        bus.emit(game::sound::SoundKind::Gunshot, shot, game::sound::radius::rifle, 0, now);
        ecology.hear(bus);
        int investigating = 0, wrong = 0;
        for (const auto& z : ecology.zombies()) {
            if (z.state == ZombieState::Dead) continue;
            const bool heard = flat(z.position, shot) < game::sound::radius::rifle * .95F;
            if (z.state == ZombieState::Investigate) {++investigating; wrong += flat(z.target, shot) > 8;}
            else if (heard) ++wrong;
        }
        check(investigating >= 3 && wrong == 0, "gunfire draws the infected in earshot: " + std::to_string(investigating) + " came, " + std::to_string(wrong) + " wrong");
    }

    // A safe start clears the spawn.
    {
        ecology.reset(npcs);
        const Vector3 spawn = sources.front().position;
        ecology.clear_area(spawn, 70);
        for (const auto& z : ecology.zombies()) check(flat(z.position, spawn) > 70, "nobody waiting at the spawn");
    }

    // Bodies: near the player the infected become creatures that close in and bite.
    {
        ecology.reset(npcs);
        const auto& nest = sources.front();
        const Vector3 player = Vector3Add(nest.position, {6, 0, 0});
        float bitten = 0;
        int most = 0;
        characters::NpcContext context; context.player_position = player;
        for (float t = 0; t < 30 && bitten <= 0; t += .05F) {
            now += .05;
            npcs.update(.05F, context, region);
            bitten += npcs.events().player_damage;
            ecology.update(.05F, {player, 0, now}, npcs);
            most = std::max(most, ecology.stats().physical);
        }
        std::cout << most << " bodies, bitten for " << bitten << "\n";
        check(most > 0 && most <= ecology.config().max_bodies, "bodies within the budget");
        check(bitten > 0, "they bite");
        // Kill one: it stays down while seen, is cleared away out of sight, and its nest refills.
        int victim = -1;
        for (const auto& z : ecology.zombies()) if (z.physical && z.state != ZombieState::Dead) {victim = z.id; break;}
        check(victim >= 0, "a body to kill");
        const int index = npcs.find_resident(ZombieEcology::body_id_base + victim);
        npcs.damage(static_cast<std::size_t>(index), 1000, player);
        run(1, 0, player);
        const auto& dead = ecology.zombies()[static_cast<std::size_t>(victim)];
        check(dead.state == ZombieState::Dead && dead.physical && ecology.stats().killed == 1, "a kill counts once, the corpse stays");
        run(1, 0, far_away);
        check(!dead.physical, "corpses go when the player does");
        run(ecology.config().respawn_seconds + 2, 0, far_away);
        check(dead.state != ZombieState::Dead && ecology.stats().killed == 1, "the nest sends another");
    }

    ecology.clear(npcs);
    check(npcs.actors().empty() || std::none_of(npcs.actors().begin(), npcs.actors().end(), [](const auto& a) {return ZombieEcology::is_zombie(a.resident);}), "clear removes every body");
    std::cout << "zombie ecology tests passed\n";
    return 0;
}
