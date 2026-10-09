#pragma once
#include "outland/game/inventory/ItemRegistry.hpp"
#include <vector>
namespace outland::game::inventory {
struct ItemStack {
    std::string item;
    int quantity{0};
    bool operator==(const ItemStack &) const = default;
};
struct InventoryState {
    std::vector<ItemStack> stacks;
    std::string container;
    bool light{false};
    float food_consumed{0}, water_consumed{0};
};
class Inventory {
  public:
    static constexpr int base_capacity = 12;
    int capacity(const ItemRegistry &) const;
    int count(const std::string &) const;
    bool owns_weapon(const ItemRegistry &, combat::WeaponId) const;
    int add(const ItemRegistry &, const std::string &, int quantity);
    int remove(const std::string &, int quantity);
    bool equip_container(const ItemRegistry &, const std::string &);
    bool can_remove(const ItemRegistry &, std::size_t index, int quantity) const;
    bool restore(const ItemRegistry &, InventoryState state);
    const InventoryState &state() const {
        return state_;
    }
    InventoryState &effects() {
        return state_;
    }
    void clear() {
        state_ = {};
    }

  private:
    InventoryState state_;
};
} // namespace outland::game::inventory
