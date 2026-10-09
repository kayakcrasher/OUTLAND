#include "outland/game/inventory/LootSave.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
namespace outland::game::inventory {
namespace {
bool valid(const ItemRegistry &registry, const LootSnapshot &state) {
    if (state.mode != GameMode::Explore && state.mode != GameMode::BattleRoyale &&
        state.mode != GameMode::ZombieSurvival)
        return false;
    Inventory bag;
    LootWorld world;
    if (!bag.restore(registry, state.inventory) || !world.restore(registry, state.world) ||
        static_cast<std::size_t>(state.selected) > 1)
        return false;
    for (std::size_t i = 0; i < state.loaded.size(); ++i) {
        const auto id = static_cast<combat::WeaponId>(i);
        if (state.loaded[i] < 0 || state.loaded[i] > combat::definition(id).magazine ||
            (!bag.owns_weapon(registry, id) && state.loaded[i] != 0))
            return false;
    }
    return true;
}
} // namespace
std::string LootSave::profile_path(const std::string &map_path, GameMode mode) {
    const auto name = mode == GameMode::ZombieSurvival ? "zombie"
                      : mode == GameMode::BattleRoyale ? "battle_royale"
                                                       : "explore";
    return (std::filesystem::path(map_path).parent_path().parent_path() / "profiles" /
            (std::string(name) + ".loot"))
        .string();
}
bool LootSave::save(const ItemRegistry &registry, const LootSnapshot &state,
                    const std::string &path, std::string &error) {
    error.clear();
    if (path.empty() || !valid(registry, state)) {
        error = "Invalid inventory snapshot";
        return false;
    }
    const std::filesystem::path destination(path), temporary = path + ".tmp";
    std::error_code ec;
    if (destination.has_parent_path())
        std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        error = "Cannot create inventory save directory";
        return false;
    }
    std::ofstream out(temporary);
    if (!out) {
        error = "Cannot write inventory save";
        return false;
    }
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << "OUTLAND_LOOT 1 "
        << static_cast<int>(state.mode) << '\n';
    out << "BAG " << std::quoted(state.inventory.container) << ' ' << state.inventory.light << ' '
        << state.inventory.food_consumed << ' ' << state.inventory.water_consumed << '\n';
    out << "WEAPONS " << static_cast<int>(state.selected) << ' ' << state.loaded[0] << ' '
        << state.loaded[1] << '\n';
    out << "NEXT " << state.world.next_drop << '\n';
    for (const auto &stack : state.inventory.stacks)
        out << "ITEM " << std::quoted(stack.item) << ' ' << stack.quantity << '\n';
    for (const auto &source : state.world.sources)
        out << "SOURCE " << std::quoted(source.key) << ' ' << std::quoted(source.table) << ' '
            << source.position.x << ' ' << source.position.y << ' ' << source.position.z << '\n';
    for (const auto &pickup : state.world.pickups)
        out << "PICKUP " << std::quoted(pickup.id) << ' ' << std::quoted(pickup.source) << ' '
            << std::quoted(pickup.item) << ' ' << pickup.position.x << ' ' << pickup.position.y
            << ' ' << pickup.position.z << ' ' << pickup.quantity << ' ' << pickup.loaded_rounds
            << '\n';
    out << "END\n";
    out.flush();
    bool good = out.good();
    out.close();
    good = good && !out.fail();
    if (good) {
        std::filesystem::rename(temporary, destination, ec);
        good = !ec;
    }
    if (!good) {
        std::filesystem::remove(temporary, ec);
        error = "Inventory save failed; previous save kept";
    }
    return good;
}
bool LootSave::load(const ItemRegistry &registry, LootSnapshot &destination,
                    const std::string &path, std::string &error) {
    error.clear();
    std::ifstream input(path);
    LootSnapshot parsed;
    std::string token;
    int version = 0, mode = 0;
    auto fail = [&]() {
        error = "Invalid or incompatible inventory save";
        return false;
    };
    if (!(input >> token >> version >> mode) || token != "OUTLAND_LOOT" || version != 1 ||
        mode != static_cast<int>(destination.mode))
        return fail();
    parsed.mode = static_cast<GameMode>(mode);
    bool bag = false, weapons = false, next = false, end = false;
    int count = 0;
    while (input >> token) {
        if (++count > 12000)
            return fail();
        if (token == "BAG") {
            int light = 0;
            if (bag ||
                !(input >> std::quoted(parsed.inventory.container) >> light >>
                  parsed.inventory.food_consumed >> parsed.inventory.water_consumed) ||
                light < 0 || light > 1)
                return fail();
            parsed.inventory.light = light != 0;
            bag = true;
        } else if (token == "WEAPONS") {
            int selected = 0;
            if (weapons || !(input >> selected >> parsed.loaded[0] >> parsed.loaded[1]) ||
                selected < 0 || selected > 1)
                return fail();
            parsed.selected = static_cast<combat::WeaponId>(selected);
            weapons = true;
        } else if (token == "NEXT") {
            if (next || !(input >> parsed.world.next_drop))
                return fail();
            next = true;
        } else if (token == "ITEM") {
            ItemStack stack;
            if (parsed.inventory.stacks.size() >= 64 ||
                !(input >> std::quoted(stack.item) >> stack.quantity))
                return fail();
            parsed.inventory.stacks.push_back(std::move(stack));
        } else if (token == "SOURCE") {
            LootSource source;
            if (parsed.world.sources.size() >= 10000 ||
                !(input >> std::quoted(source.key) >> std::quoted(source.table) >>
                  source.position.x >> source.position.y >> source.position.z))
                return fail();
            parsed.world.sources.push_back(std::move(source));
        } else if (token == "PICKUP") {
            WorldPickup pickup;
            if (parsed.world.pickups.size() >= LootWorld::max_pickups ||
                !(input >> std::quoted(pickup.id) >> std::quoted(pickup.source) >>
                  std::quoted(pickup.item) >> pickup.position.x >> pickup.position.y >>
                  pickup.position.z >> pickup.quantity >> pickup.loaded_rounds))
                return fail();
            parsed.world.pickups.push_back(std::move(pickup));
        } else if (token == "END") {
            end = true;
            break;
        } else
            return fail();
    }
    if (!bag || !weapons || !next || !end || (input >> token) || input.bad() ||
        !valid(registry, parsed))
        return fail();
    destination = std::move(parsed);
    return true;
}
} // namespace outland::game::inventory
