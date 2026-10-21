#pragma once
#include "outland/game/inventory/Inventory.hpp"
#include "outland/game/inventory/LootWorld.hpp"
#include "outland/game/GameMode.hpp"
#include <array>
#include "outland/game/economy/Economy.hpp"
namespace outland::game::inventory {
struct LootSnapshot {
    GameMode mode{GameMode::Explore};
    InventoryState inventory;
    LootWorldState world;
    combat::WeaponId selected{combat::WeaponId::Rifle};
    std::array<int, combat::weapon_count> loaded{};
    economy::EconomyState economy;
};
class LootSave {
  public:
    // The campaign (Explore) has three save slots: each slot's files start from its own profile
    // ("explore.loot" for slot 1, as before slots; "explore_2.loot", "explore_3.loot"). The slot in
    // play is chosen on the home screen.
    static constexpr int campaign_slots = 3;
    static void set_campaign_slot(int slot);
    [[nodiscard]] static int campaign_slot();
    static std::string profile_path(const std::string &map_path, GameMode mode);
    static std::string slot_path(const std::string &map_path, int slot);
    static bool save(const ItemRegistry &, const LootSnapshot &, const std::string &path,
                     std::string &error);
    static bool load(const ItemRegistry &, LootSnapshot &, const std::string &path,
                     std::string &error);
};
} // namespace outland::game::inventory
