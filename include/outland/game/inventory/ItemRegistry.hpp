#pragma once
#include "outland/game/combat/WeaponDefinition.hpp"
#include <string>
#include <raylib.h>
#include <vector>
#include <optional>
namespace outland::game::inventory {
enum class ItemCategory { Food, Water, Medical, Ammo, Weapon, Utility, Container, Furniture };
enum class ItemAction { None, Eat, Drink, Heal, Ammo, Equip, Container, Light };
struct ItemDefinition {
    std::string id, name, model, requires_item;
    ItemCategory category{};
    ItemAction action{};
    int stack_size{1}, capacity_bonus{0}, starting_quantity{0}, starting_loaded{0};
    // `size`: the item's longest side in metres; its model is scaled uniformly to it on the ground.
    float size{.25F}, value{0};
    std::optional<combat::WeaponId> weapon;
};
// How rare a find is: battle-royale colour tiers (white, green, blue, purple) for the glow on the
// ground, the beam over the best finds, and the pickup prompt.
enum class Rarity : unsigned char { Common, Uncommon, Rare, Epic };
Rarity rarity_of(const std::string& item);
const char* rarity_name(Rarity rarity);
struct LootEntry {
    std::string item;
    int minimum{1}, maximum{1}, weight{1};
};
// Which loot a mode finds: Explore's town life, Zombie Survival's scarcity, or Battle Royale's
// arsenal (guns, armour, optics; a table without a royale variant falls back to normal).
enum class LootMode { Normal, Zombie, Royale };
const char* loot_mode_name(LootMode mode);
struct LootTable {
    std::string id;
    bool zombie{false};
    bool royale{false};
    int rolls{1};
    std::vector<LootEntry> entries;
};
class ItemRegistry {
  public:
    bool load(const std::string &items, const std::string &tables, std::string &error);
    const ItemDefinition *find(const std::string &id) const;
    const LootTable *table(const std::string &id, bool zombie) const;
    const LootTable *table(const std::string &id, LootMode mode) const;
    // The ammunition item a firearm item takes (Battle Royale lays it beside each gun).
    const ItemDefinition *ammo_for(const ItemDefinition &firearm) const;
    const std::vector<ItemDefinition> &items() const {
        return items_;
    }
    std::string table_for_marker(const std::string &id) const;

  private:
    std::vector<ItemDefinition> items_;
    std::vector<LootTable> tables_;
};
// Scale that makes the model's longest side one unit (multiplied by ItemDefinition::size on the
// ground), so a flat vest or a long rifle keeps its real proportions. Zero rejects invalid geometry.
float pickup_unit_scale(BoundingBox bounds);
const char *category_name(ItemCategory category);
bool valid_text(const std::string &value);
} // namespace outland::game::inventory
