#pragma once
#include "outland/game/inventory/Inventory.hpp"
#include "outland/game/inventory/LootWorld.hpp"
#include "outland/game/GameMode.hpp"
#include <array>
namespace outland::game::inventory {
struct LootSnapshot {
    GameMode mode{GameMode::Explore};
    InventoryState inventory;
    LootWorldState world;
    combat::WeaponId selected{combat::WeaponId::Rifle};
    std::array<int, 2> loaded{};
};
class LootSave {
  public:
    static std::string profile_path(const std::string &map_path, GameMode mode);
    static bool save(const ItemRegistry &, const LootSnapshot &, const std::string &path,
                     std::string &error);
    static bool load(const ItemRegistry &, LootSnapshot &, const std::string &path,
                     std::string &error);
};
} // namespace outland::game::inventory
