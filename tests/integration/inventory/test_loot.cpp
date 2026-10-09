#include "outland/game/inventory/LootSession.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <raymath.h>
using namespace outland::game;
using namespace outland::game::inventory;
namespace fs = std::filesystem;
namespace {
std::size_t slot(const Inventory &bag, const std::string &item) {
    const auto &stacks = bag.state().stacks;
    for (std::size_t i = 0; i < stacks.size(); ++i)
        if (stacks[i].item == item)
            return i;
    assert(false);
    return 0;
}
Vector3 ground(float x = 0, float z = 8) {
    return {x, outland::world::terrain::TerrainHeight::sample(x, z), z};
}
} // namespace
int main() {
    const fs::path root = OUTLAND_SOURCE_DIR;
    const auto directory =
        fs::temp_directory_path() /
        ("outland-loot-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);
    const auto items = root / "assets/verda/survival/gameplay/items.tsv",
               tables = root / "assets/verda/survival/gameplay/loot_tables.tsv";
    ItemRegistry registry;
    std::string error;
    assert(registry.load(items.string(), tables.string(), error));
    assert(registry.items().size() == 22);
    for (const auto &item : registry.items())
        if (!item.model.empty())
            assert(fs::exists(root / item.model));
    for (const auto &id : {"general", "medical", "camp", "weapons"}) {
        assert(registry.table(id, false) && registry.table(id, true));
    }
    assert(registry.table_for_marker("creator_marker_loot_spawn_medical_7") == "medical");
    assert(registry.table_for_marker("loot_spawn_weapons_xyz") == "weapons");
    assert(registry.table_for_marker("loot_spawn_medicalish") == "general");
    for (const auto &normal : registry.table("weapons", false)->entries)
        if (normal.item.ends_with("_ammo")) {
            const auto &zombie = registry.table("weapons", true)->entries;
            const auto match = std::find_if(zombie.begin(), zombie.end(),
                                            [&](const auto &e) { return e.item == normal.item; });
            assert(match != zombie.end() && match->maximum < normal.maximum &&
                   match->weight < normal.weight);
        }
    const auto invalid = directory / "invalid.tsv";
    {
        std::ofstream file(invalid);
        file << "bad header\n";
    }
    assert(!registry.load(invalid.string(), tables.string(), error) &&
           registry.items().size() == 22);

    Inventory bag;
    assert(bag.add(registry, "water", 8) == 8);
    assert(bag.state().stacks.size() == 2 && bag.state().stacks[0].quantity == 6 &&
           bag.state().stacks[1].quantity == 2);
    assert(bag.add(registry, "water", 4) == 4 && bag.state().stacks.size() == 2);
    assert(bag.add(registry, "water", 999) == 60 && bag.state().stacks.size() == 12);
    assert(bag.add(registry, "water", 1) == 0);
    assert(bag.remove("water", 6) == 6);
    assert(bag.add(registry, "backpack", 1) == 1);
    assert(bag.equip_container(registry, "backpack") && bag.capacity(registry) == 24);
    assert(bag.add(registry, "water", 999) == 72 && bag.state().stacks.size() == 24);
    assert(!bag.can_remove(registry, slot(bag, "backpack"), 1));
    auto invalid_state = bag.state();
    invalid_state.stacks[0].quantity = 999;
    assert(!bag.restore(registry, invalid_state) && bag.state().stacks.size() == 24);
    bag.clear();
    assert(bag.add(registry, "pistol", 1) == 1 && bag.add(registry, "pistol", 1) == 0);
    assert(bag.add(registry, "unknown", 1) == 0 && bag.add(registry, "water", -1) == 0);

    outland::world::VerdaRegion region;
    const auto map = directory / "markers.map";
    {
        std::ofstream file(map);
        file << "OUTLAND_CREATOR_MAP 3\nMARKER \"creator_marker_loot_spawn_medical_test\" 0 0 0 8 "
                ".5 .5 .5 0 1\n";
    }
    assert(outland::creator::CreatorMapIO::load(region, map.string()));
    LootWorld a, b;
    assert(!a.reconcile(region, registry, false, {10000, 0, 10000}));
    assert(a.state().pickups.empty());
    assert(a.reconcile(region, registry, false, ground()));
    assert(b.reconcile(region, registry, false, ground()));
    assert(a.state().pickups.size() == 2 && a.state().sources.size() == 1);
    for (std::size_t i = 0; i < a.state().pickups.size(); ++i) {
        assert(a.state().pickups[i].item == b.state().pickups[i].item &&
               a.state().pickups[i].quantity == b.state().pickups[i].quantity);
    }
    assert(!a.reconcile(region, registry, false, ground()));
    a.edit().pickups.clear();
    assert(!a.reconcile(region, registry, false, ground()) && a.state().pickups.empty());
    assert(!a.drop(registry, "water", 1, {std::numeric_limits<float>::quiet_NaN(), 0, 0}));
    auto bad_world = b.state();
    bad_world.pickups.push_back(bad_world.pickups.front());
    assert(!a.restore(registry, bad_world) && a.state().pickups.empty());
    {
        std::ofstream file(map);
        file << "OUTLAND_CREATOR_MAP 3\nMARKER \"creator_marker_loot_spawn_medical_test\" 0 0 0 8 "
                ".5 .5 .5 0 0\n";
    }
    assert(outland::creator::CreatorMapIO::load(region, map.string()));
    assert(b.reconcile(region, registry, false, ground()) && b.state().pickups.empty() &&
           b.state().sources.empty());
    assert(outland::creator::CreatorMapIO::load(region,
                                                (root / "maps/verda_loot_defaults.map").string()));
    LootWorld defaults;
    assert(defaults.reconcile(region, registry, false, ground()) &&
           defaults.state().sources.size() == 5);

    combat::WeaponSystem weapons;
    LootSession session(registry);
    const auto profile_map = (directory / "maps/verda_creator.map").string();
    session.start(GameMode::Explore, profile_map, weapons);
    assert(session.inventory().owns_weapon(registry, combat::WeaponId::Rifle) &&
           session.inventory().count("rifle_ammo") == 120);
    assert(weapons.ammo(combat::WeaponId::Rifle).loaded == 30 && weapons.ammo().reserve == 120);
    Health health;
    float food = 0, water = 0;
    session.consumed = [&](float f, float w) {
        food += f;
        water += w;
    };
    const auto food_before = session.inventory().count("canned_food");
    assert(session.use(slot(session.inventory(), "canned_food"), weapons, health)
               .starts_with("Consumed"));
    assert(session.inventory().count("canned_food") == food_before - 1 && food == 25);
    session.use(slot(session.inventory(), "water"), weapons, health);
    assert(water == 30);
    assert(session.use(slot(session.inventory(), "bandage"), weapons, health) ==
           "Medical item not needed");
    health.damage(40);
    session.use(slot(session.inventory(), "bandage"), weapons, health);
    assert(health.current() == 70 && session.inventory().count("bandage") == 0);
    assert(session.use(slot(session.inventory(), "pistol_ammo"), weapons, health) ==
           "Equip a compatible weapon first");
    assert(
        session.use(slot(session.inventory(), "pistol"), weapons, health).starts_with("Equipped") &&
        weapons.selected() == combat::WeaponId::Pistol);
    weapons.set_loaded(combat::WeaponId::Pistol, 2);
    const auto ammo_before = session.inventory().count("pistol_ammo");
    session.use(slot(session.inventory(), "pistol_ammo"), weapons, health);
    assert(session.inventory().count("pistol_ammo") == ammo_before);
    combat::CombatWorld combat(region);
    for (int i = 0; i < 20; ++i)
        weapons.update(.1F, {}, {{0, 2, 0}, {0, 0, -1}}, combat);
    assert(weapons.ammo().loaded == 12 &&
           session.inventory().count("pistol_ammo") == ammo_before - 10 &&
           session.inventory().count("rifle_ammo") == 120);
    // Dropped firearm carries its actual magazine. No phantom ammo or duplicate ownership.
    session.drop(slot(session.inventory(), "pistol"), false, ground(2, 8), weapons);
    assert(!session.inventory().owns_weapon(registry, combat::WeaponId::Pistol) &&
           weapons.ammo(combat::WeaponId::Pistol).loaded == 0 && !weapons.available());
    assert(session.world().state().pickups.back().loaded_rounds == 12);
    assert(session.interact(ground(2, 8), weapons, [](Vector3) { return false; }) ==
           "Nothing to pick up nearby");
    assert(session.interact(ground(2, 8), weapons).starts_with("Picked up") &&
           weapons.ammo(combat::WeaponId::Pistol).loaded == 12);
    assert(session.edit_inventory().add(registry, "lantern", 1) == 1);
    assert(session.use(slot(session.inventory(), "lantern"), weapons, health) ==
           "A battery is required");
    assert(session.edit_inventory().add(registry, "battery", 2) == 2);
    session.use(slot(session.inventory(), "lantern"), weapons, health);
    assert(session.inventory().state().light && session.inventory().count("battery") == 1);
    session.use(slot(session.inventory(), "lantern"), weapons, health);
    assert(!session.inventory().state().light);
    session.drop(slot(session.inventory(), "water"), true, ground(2, 8), weapons);
    assert(session.inventory().count("water") == 0 && session.world().state().pickups.size() == 1);
    assert(session.save(weapons));
    const auto saved = session.snapshot(weapons);
    combat::WeaponSystem resumed_weapons;
    LootSession resumed(registry);
    resumed.start(GameMode::Explore, profile_map, resumed_weapons);
    assert(resumed.inventory().state().stacks == saved.inventory.stacks &&
           resumed.world().state().pickups.size() == saved.world.pickups.size() &&
           resumed_weapons.ammo(combat::WeaponId::Pistol).loaded == 12 &&
           resumed_weapons.selected() == combat::WeaponId::Pistol);
    LootSnapshot wrong;
    wrong.mode = GameMode::ZombieSurvival;
    assert(!LootSave::load(registry, wrong, session.path(), error));
    LootSnapshot loaded;
    loaded.mode = GameMode::Explore;
    assert(LootSave::load(registry, loaded, session.path(), error));
    const auto corrupt = directory / "corrupt.loot";
    {
        std::ofstream f(corrupt);
        f << "OUTLAND_LOOT 1 3\nBAG \"\" 0 0 0\nWEAPONS 1 0 0\nNEXT 1\nITEM \"unknown\" 1\nEND\n";
    }
    const auto previous = loaded.inventory.stacks;
    assert(!LootSave::load(registry, loaded, corrupt.string(), error) &&
           loaded.inventory.stacks == previous);
    // A file at the directory path forces save failure; it remains intact.
    const auto blocker = directory / "blocker";
    {
        std::ofstream f(blocker);
        f << "keep";
    }
    assert(!LootSave::save(registry, saved, (blocker / "child.loot").string(), error));
    {
        std::ifstream f(blocker);
        std::string value;
        f >> value;
        assert(value == "keep");
    }
    // Malformed profile is never replaced by a fresh starter inventory.
    fs::copy_file(corrupt, resumed.path(), fs::copy_options::overwrite_existing);
    LootSession broken(registry);
    combat::WeaponSystem broken_weapons;
    broken.start(GameMode::Explore, profile_map, broken_weapons);
    assert(!broken.save(broken_weapons));
    assert(fs::file_size(resumed.path()) == fs::file_size(corrupt));
    assert(LootSave::profile_path(profile_map, GameMode::Explore) !=
           LootSave::profile_path(profile_map, GameMode::ZombieSurvival));
    assert(!broken.can_persist());
    // Partially full stacks pick up only what fits; the remainder survives on the ground.
    combat::WeaponSystem partial_weapons;
    LootSession partial(registry);
    partial.start(GameMode::ZombieSurvival, (directory / "partial/maps/verda_creator.map").string(),
                  partial_weapons);
    partial.edit_inventory().clear();
    partial_weapons.set_loaded(combat::WeaponId::Pistol, 0);
    partial_weapons.set_loaded(combat::WeaponId::Rifle, 0);
    assert(partial.edit_inventory().add(registry, "water", 71) == 71);
    assert(partial.edit_world().drop(registry, "water", 5, ground()));
    assert(partial.interact(ground(), partial_weapons).starts_with("Picked up 1"));
    assert(partial.inventory().count("water") == 72 &&
           partial.world().state().pickups.front().quantity == 4);
    assert(partial.interact(ground(), partial_weapons).starts_with("Bag full"));
    const auto before_drop = partial.inventory().state().stacks;
    assert(partial.drop(0, false, ground(), partial_weapons, [](Vector3) { return false; }) ==
           "Cannot drop through an obstacle");
    assert(partial.inventory().state().stacks == before_drop);
    // World pool exhaustion cannot remove inventory items.
    while (partial.world().state().pickups.size() < LootWorld::max_pickups)
        assert(partial.edit_world().drop(registry, "water", 1, ground(3, 8)));
    assert(partial.drop(0, false, ground(), partial_weapons).starts_with("Cannot drop here"));
    assert(partial.inventory().state().stacks == before_drop);
    partial.changed();
    partial.update(1.1F, region, ground(), partial_weapons);
    assert(!partial.dirty() && fs::exists(partial.path()));
    auto orphan = partial.inventory().state();
    orphan.light = true;
    assert(!partial.edit_inventory().restore(registry, orphan));
    // The last-round magazine is conserved through a save before reload completes.
    combat::WeaponSystem saved_reload_weapons;
    LootSession saved_reload(registry);
    const auto reload_map = (directory / "reload/maps/verda_creator.map").string();
    saved_reload.start(GameMode::Explore, reload_map, saved_reload_weapons);
    saved_reload_weapons.set_loaded(combat::WeaponId::Rifle, 1);
    saved_reload_weapons.request_reload();
    assert(saved_reload.save(saved_reload_weapons));
    combat::WeaponSystem fresh_reload_weapons;
    LootSession fresh_reload(registry);
    fresh_reload.start(GameMode::Explore, reload_map, fresh_reload_weapons);
    assert(fresh_reload_weapons.ammo().loaded == 1 &&
           fresh_reload.inventory().count("rifle_ammo") == 120 &&
           fresh_reload_weapons.reload_remaining() == 0);
    fresh_reload.drop(slot(fresh_reload.inventory(),"pistol"),false,ground(),fresh_reload_weapons);
    fresh_reload_weapons.request_reload();
    combat::WeaponInput cycle;cycle.next_weapon=true;
    fresh_reload_weapons.update(.1F,cycle,{{0,2,0},{0,0,-1}},combat);
    assert(fresh_reload_weapons.selected()==combat::WeaponId::Rifle && fresh_reload_weapons.reload_remaining()>0);
    assert(fresh_reload_weapons.collect_ammo()==0); // Legacy marker ammo cannot bypass the bag.
    // Malformed/truncated/duplicate source records leave the loaded snapshot untouched.
    const auto truncated = directory / "truncated.loot";
    {
        std::ofstream file(truncated);
        file << "OUTLAND_LOOT 1 3\nBAG \"\" 0 0 0\nWEAPONS 1 0 0\nNEXT 1\n";
    }
    assert(!LootSave::load(registry, loaded, truncated.string(), error) &&
           loaded.inventory.stacks == previous);
    // Existing stable marker IDs are idempotent; moving a marker intentionally regenerates its
    // table.
    LootWorld moved;
    assert(moved.reconcile(region, registry, false, ground()));
    const auto sources_before = moved.state().sources.size();
    {
        // V3 loads replace the Creator marker set, so retain the other four markers.
        std::ifstream input(root / "maps/verda_loot_defaults.map");
        std::string contents((std::istreambuf_iterator<char>(input)),{});
        const std::string before="general_training\" 0 4 0 8",after="general_training\" 0 2 0 8";
        const auto at=contents.find(before);
        assert(at!=std::string::npos);
        contents.replace(at,before.size(),after);
        std::ofstream file(map);
        file << contents;
    }
    assert(outland::creator::CreatorMapIO::load(region, map.string()));
    assert(moved.reconcile(region, registry, false, ground()) &&
           moved.state().sources.size() == sources_before);
    // A fully scavenged marker remains empty across a restart, rather than respawning ammo.
    const auto spent_map = (directory / "spent/maps/verda_creator.map").string();
    combat::WeaponSystem spent_weapons;
    LootSession spent(registry);
    spent.start(GameMode::Explore, spent_map, spent_weapons);
    spent.update(.1F, region, ground(), spent_weapons);
    assert(!spent.world().state().sources.empty());
    spent.edit_world().edit().pickups.clear();
    spent.changed();
    assert(spent.save(spent_weapons));
    combat::WeaponSystem spent_after_weapons;
    LootSession spent_after(registry);
    spent_after.start(GameMode::Explore, spent_map, spent_after_weapons);
    spent_after.update(.1F, region, ground(), spent_after_weapons);
    assert(spent_after.world().state().pickups.empty());
    auto duplicate_sources = spent_after.snapshot(spent_after_weapons);
    duplicate_sources.world.sources.push_back(duplicate_sources.world.sources.front());
    assert(!LootSave::save(registry, duplicate_sources, spent_after.path(), error));
    // Picking up an extra gun never overwrites an owned gun's loaded rounds.
    const auto rifle_loaded = spent_after_weapons.ammo(combat::WeaponId::Rifle).loaded;
    assert(spent_after.edit_world().drop(registry, "hunting_rifle", 1, ground(), 0));
    assert(spent_after.interact(ground(), spent_after_weapons) ==
           "You already carry this weapon type");
    assert(spent_after_weapons.ammo(combat::WeaponId::Rifle).loaded == rifle_loaded &&
           spent_after.world().state().pickups.size() == 1);
    assert(!fs::exists(profile_map)); // Inventory never writes the authored world map.
    fs::remove_all(directory);
    std::cout << "[PASS] Registry, deterministic loot, capacity, consume/equip/reload/drop, mode "
                 "profiles and transactional saves\n";
}
