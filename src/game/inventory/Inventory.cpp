#include "outland/game/inventory/Inventory.hpp"
#include <algorithm>
#include <cmath>
namespace outland::game::inventory {
int Inventory::capacity(const ItemRegistry &registry) const {
    const auto *item = registry.find(state_.container);
    return base_capacity + (item ? item->capacity_bonus : 0);
}
int Inventory::count(const std::string &id) const {
    int total = 0;
    for (const auto &stack : state_.stacks)
        if (stack.item == id)
            total += stack.quantity;
    return total;
}
bool Inventory::owns_weapon(const ItemRegistry &registry, combat::WeaponId weapon) const {
    for (const auto &stack : state_.stacks) {
        const auto *item = registry.find(stack.item);
        if (item && item->category == ItemCategory::Weapon && item->weapon == weapon)
            return true;
    }
    return false;
}
int Inventory::add(const ItemRegistry &registry, const std::string &id, int quantity) {
    const auto *item = registry.find(id);
    if (!item || quantity <= 0 || quantity > 99999)
        return 0;
    if (item->category == ItemCategory::Weapon) {
        if (owns_weapon(registry, *item->weapon))
            return 0;
        quantity = 1;
    }
    const int offered = quantity;
    for (auto &stack : state_.stacks)
        if (stack.item == id) {
            const int amount = std::min(quantity, item->stack_size - stack.quantity);
            stack.quantity += amount;
            quantity -= amount;
            if (quantity == 0)
                break;
        }
    while (quantity > 0 && static_cast<int>(state_.stacks.size()) < capacity(registry)) {
        const int amount = std::min(quantity, item->stack_size);
        state_.stacks.push_back({id, amount});
        quantity -= amount;
    }
    return offered - quantity;
}
int Inventory::remove(const std::string &id, int quantity) {
    if (quantity <= 0)
        return 0;
    const int offered = quantity;
    for (auto &stack : state_.stacks)
        if (stack.item == id) {
            const int amount = std::min(quantity, stack.quantity);
            stack.quantity -= amount;
            quantity -= amount;
            if (quantity == 0)
                break;
        }
    std::erase_if(state_.stacks, [](const auto &stack) { return stack.quantity == 0; });
    if (count(state_.container) == 0)
        state_.container.clear();
    return offered - quantity;
}
bool Inventory::equip_container(const ItemRegistry &registry, const std::string &id) {
    const auto *item = registry.find(id);
    if (!item || item->category != ItemCategory::Container || count(id) == 0 ||
        static_cast<int>(state_.stacks.size()) > base_capacity + item->capacity_bonus)
        return false;
    state_.container = id;
    return true;
}
bool Inventory::can_remove(const ItemRegistry &registry, std::size_t index, int quantity) const {
    if (index >= state_.stacks.size() || quantity <= 0 || quantity > state_.stacks[index].quantity)
        return false;
    const auto &stack = state_.stacks[index];
    if (stack.item != state_.container || count(stack.item) > quantity)
        return true;
    (void)registry;
    return static_cast<int>(state_.stacks.size()) - (quantity == stack.quantity ? 1 : 0) <=
           base_capacity;
}
bool Inventory::restore(const ItemRegistry &registry, InventoryState state) {
    if (state.stacks.size() > 64 || !std::isfinite(state.food_consumed) ||
        !std::isfinite(state.water_consumed) || state.food_consumed < 0 ||
        state.water_consumed < 0 || state.food_consumed > 1e9F || state.water_consumed > 1e9F)
        return false;
    Inventory checked;
    std::vector<combat::WeaponId> weapons;
    for (const auto &stack : state.stacks) {
        const auto *item = registry.find(stack.item);
        if (!item || stack.quantity < 1 || stack.quantity > item->stack_size)
            return false;
        if (item->category == ItemCategory::Weapon) {
            if (std::find(weapons.begin(), weapons.end(), *item->weapon) != weapons.end())
                return false;
            weapons.push_back(*item->weapon);
        }
    }
    checked.state_ = std::move(state);
    if (!checked.state_.container.empty()) {
        const auto *item = registry.find(checked.state_.container);
        if (!item || item->category != ItemCategory::Container || checked.count(item->id) == 0)
            return false;
    }
    if (static_cast<int>(checked.state_.stacks.size()) > checked.capacity(registry))
        return false;
    if (checked.state_.light && std::none_of(checked.state_.stacks.begin(),
                                             checked.state_.stacks.end(), [&](const auto &stack) {
                                                 return registry.find(stack.item)->action ==
                                                        ItemAction::Light;
                                             }))
        return false;
    *this = std::move(checked);
    return true;
}
} // namespace outland::game::inventory
