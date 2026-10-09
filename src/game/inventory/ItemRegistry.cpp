#include "outland/game/inventory/ItemRegistry.hpp"
#include "outland/game/inventory/Inventory.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
namespace outland::game::inventory {
namespace {
std::vector<std::string> columns(std::string line) {
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    std::vector<std::string> out;
    std::stringstream stream(line);
    std::string value;
    while (std::getline(stream, value, '\t'))
        out.push_back(value);
    if (!line.empty() && line.back() == '\t')
        out.emplace_back();
    return out;
}
bool integer(const std::string &s, int &value) {
    try {
        std::size_t used = 0;
        value = std::stoi(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}
bool number(const std::string &s, float &value) {
    try {
        std::size_t used = 0;
        value = std::stof(s, &used);
        return used == s.size() && std::isfinite(value);
    } catch (...) {
        return false;
    }
}
bool identifier(const std::string &s) {
    return !s.empty() && s.size() < 100 &&
           std::all_of(
               s.begin(), s.end(),
               [](unsigned char c) {
                   return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
               });
}
} // namespace
bool valid_text(const std::string &value) {
    return value.size() <= 1024 && std::none_of(value.begin(), value.end(),
                                                [](unsigned char c) { return c < 32 || c == 127; });
}
const char *category_name(ItemCategory c) {
    switch (c) {
    case ItemCategory::Food:
        return "Food";
    case ItemCategory::Water:
        return "Water";
    case ItemCategory::Medical:
        return "Medical";
    case ItemCategory::Ammo:
        return "Ammo";
    case ItemCategory::Weapon:
        return "Weapon";
    case ItemCategory::Container:
        return "Container";
    default:
        return "Utility";
    }
}
const ItemDefinition *ItemRegistry::find(const std::string &id) const {
    const auto it =
        std::find_if(items_.begin(), items_.end(), [&](const auto &item) { return item.id == id; });
    return it == items_.end() ? nullptr : &*it;
}
const LootTable *ItemRegistry::table(const std::string &id, bool zombie) const {
    const auto it = std::find_if(tables_.begin(), tables_.end(),
                                 [&](const auto &t) { return t.id == id && t.zombie == zombie; });
    return it == tables_.end() ? nullptr : &*it;
}
std::string ItemRegistry::table_for_marker(const std::string &id) const {
    std::string result = "general";
    std::size_t length = 0;
    for (const auto &table : tables_)
        for (const std::string prefix : {"creator_marker_loot_spawn_", "loot_spawn_"}) {
            const auto token = prefix + table.id;
            if (token.size() > length && (id == token || id.starts_with(token + "_"))) {
                result = table.id;
                length = token.size();
            }
        }
    return result;
}
bool ItemRegistry::load(const std::string &item_path, const std::string &table_path,
                        std::string &error) {
    error.clear();
    ItemRegistry parsed;
    std::ifstream input(item_path);
    std::string line;
    auto fail = [&](const std::string &reason) {
        error = reason;
        return false;
    };
    if (!input || !std::getline(input, line) ||
        columns(line) != std::vector<std::string>{"id", "name", "category", "model", "stack",
                                                  "height", "action", "value", "weapon", "capacity",
                                                  "requires", "starting_quantity",
                                                  "starting_loaded"})
        return fail("Invalid item manifest header");
    const std::vector<std::string> categories{"food",   "water",   "medical",  "ammo",
                                              "weapon", "utility", "container"},
        actions{"none", "eat", "drink", "heal", "ammo", "equip", "container", "light"};
    while (std::getline(input, line)) {
        if (line.empty() || line.starts_with('#'))
            continue;
        const auto c = columns(line);
        if (c.size() != 13)
            return fail("Invalid item columns");
        ItemDefinition item;
        item.id = c[0];
        item.name = c[1];
        item.model = c[3];
        item.requires_item = c[10];
        const auto category = std::find(categories.begin(), categories.end(), c[2]),
                   action = std::find(actions.begin(), actions.end(), c[6]);
        if (!identifier(item.id) || parsed.find(item.id) || item.name.empty() ||
            !valid_text(item.name) || category == categories.end() || action == actions.end() ||
            !integer(c[4], item.stack_size) || item.stack_size < 1 || item.stack_size > 999 ||
            !number(c[5], item.height) || item.height < .03F || item.height > 3 ||
            !number(c[7], item.value) || item.value < 0 || item.value > 1000 ||
            !integer(c[9], item.capacity_bonus) || item.capacity_bonus < 0 ||
            item.capacity_bonus > 52)
            return fail("Invalid item definition: " + item.id);
        item.category = static_cast<ItemCategory>(category - categories.begin());
        item.action = static_cast<ItemAction>(action - actions.begin());
        if (!valid_text(item.model) ||
            (!item.model.empty() && (!item.model.starts_with("assets/verda/") ||
                                     std::filesystem::path(item.model).extension() != ".glb" ||
                                     item.model.find('\\') != std::string::npos)))
            return fail("Unsafe item model path");
        for (const auto &part : std::filesystem::path(item.model))
            if (part == ".." || part == ".")
                return fail("Unsafe item model path");
        if (c[8] == "pistol")
            item.weapon = combat::WeaponId::Pistol;
        else if (c[8] == "rifle")
            item.weapon = combat::WeaponId::Rifle;
        else if (!c[8].empty())
            return fail("Unknown weapon compatibility");
        const bool firearm = item.category == ItemCategory::Weapon,
                   ammo = item.category == ItemCategory::Ammo;
        if ((item.model.empty() && !firearm) || !integer(c[11], item.starting_quantity) ||
            !integer(c[12], item.starting_loaded) || item.starting_quantity < 0 ||
            item.starting_quantity > 999 || item.starting_loaded < 0 ||
            (!firearm && item.starting_loaded != 0) ||
            (firearm && (!item.weapon || item.starting_quantity > 1 ||
                         item.starting_loaded > combat::definition(*item.weapon).magazine)))
            return fail("Invalid starter items");
        if ((firearm || ammo) != item.weapon.has_value() ||
            (firearm && (item.stack_size != 1 || item.action != ItemAction::Equip)) ||
            (ammo && item.action != ItemAction::Ammo) ||
            (item.action == ItemAction::Container && item.category != ItemCategory::Container) ||
            (item.action == ItemAction::Heal && item.category != ItemCategory::Medical) ||
            (item.action == ItemAction::Equip && !firearm) ||
            (item.action == ItemAction::Ammo && !ammo))
            return fail("Incompatible item action: " + item.id);
        if ((item.action == ItemAction::Eat && item.category != ItemCategory::Food) ||
            (item.action == ItemAction::Drink && item.category != ItemCategory::Water) ||
            (item.action == ItemAction::Light && item.category != ItemCategory::Utility) ||
            ((item.action == ItemAction::Eat || item.action == ItemAction::Drink ||
              item.action == ItemAction::Heal) &&
             item.value <= 0))
            return fail("Invalid item effect");
        parsed.items_.push_back(std::move(item));
        if (parsed.items_.size() > 1024)
            return fail("Too many items");
    }
    if (input.bad() || parsed.items_.empty())
        return fail("Cannot read items");
    for (const auto &item : parsed.items_)
        if (!item.requires_item.empty() &&
            (!parsed.find(item.requires_item) || item.requires_item == item.id ||
             item.action != ItemAction::Light ||
             parsed.find(item.requires_item)->category != ItemCategory::Utility))
            return fail("Missing use requirement");
    Inventory starter;
    for (const auto &item : parsed.items_)
        if (starter.add(parsed, item.id, item.starting_quantity) != item.starting_quantity)
            return fail("Starter inventory exceeds capacity or duplicates a weapon");
    input.close();
    input.open(table_path);
    if (!input || !std::getline(input, line) ||
        columns(line) !=
            std::vector<std::string>{"table", "mode", "item", "min", "max", "weight", "rolls"})
        return fail("Invalid loot table header");
    while (std::getline(input, line)) {
        if (line.empty() || line.starts_with('#'))
            continue;
        const auto c = columns(line);
        if (c.size() != 7 || !identifier(c[0]) || (c[1] != "normal" && c[1] != "zombie") ||
            !parsed.find(c[2]))
            return fail("Invalid loot table entry");
        LootEntry entry;
        entry.item = c[2];
        int rolls = 0;
        if (!integer(c[3], entry.minimum) || !integer(c[4], entry.maximum) ||
            !integer(c[5], entry.weight) || !integer(c[6], rolls) || entry.minimum < 1 ||
            entry.maximum < entry.minimum || entry.maximum > 999 || entry.weight < 1 ||
            entry.weight > 10000 || rolls < 1 || rolls > 8)
            return fail("Invalid loot quantities");
        if (parsed.find(entry.item)->category == ItemCategory::Weapon && entry.maximum != 1)
            return fail("Firearm pickups must be individual");
        const bool zombie = c[1] == "zombie";
        auto found = std::find_if(parsed.tables_.begin(), parsed.tables_.end(), [&](const auto &t) {
            return t.id == c[0] && t.zombie == zombie;
        });
        if (found == parsed.tables_.end()) {
            parsed.tables_.push_back({c[0], zombie, rolls, {}});
            found = std::prev(parsed.tables_.end());
        }
        if (found->rolls != rolls || found->entries.size() >= 128)
            return fail("Inconsistent loot table");
        found->entries.push_back(std::move(entry));
    }
    if (input.bad() || !parsed.table("general", false) || !parsed.table("general", true))
        return fail("Missing general loot tables");
    for (const auto &t : parsed.tables_)
        if (!parsed.table(t.id, !t.zombie))
            return fail("Missing mode variant: " + t.id);
    *this = std::move(parsed);
    return true;
}
} // namespace outland::game::inventory
