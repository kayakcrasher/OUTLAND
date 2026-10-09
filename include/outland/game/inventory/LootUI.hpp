#pragma once
#include "outland/game/inventory/LootSession.hpp"
#include "outland/assets/ModelCache.hpp"
#include <unordered_set>
namespace outland::game::inventory {
struct InventoryActions {
    bool close{false}, use{false}, drop_one{false}, drop_stack{false};
    std::size_t selected{0};
};
class LootUI {
  public:
    explicit LootUI(std::string asset_root) : asset_root_(std::move(asset_root)) {}
    void opened() {
        warm_ = true;
    }
    InventoryActions update(const Inventory &, int width, int height, bool focused);
    void draw_inventory(const ItemRegistry &, const LootSession &, const combat::WeaponSystem &,
                        int width, int height) const;
    void draw_world(const ItemRegistry &, const LootWorld &, Vector3 player, bool light);
    static Rectangle row_rect(std::size_t row, int width, int height);
    static Rectangle action_rect(int action, int width, int height);
    std::size_t selected() const {
        return selected_;
    }

  private:
    std::string asset_root_;
    assets::ModelCache models_{32};
    std::unordered_set<int> previous_;
    std::size_t selected_{0}, page_{0};
    bool warm_{true}, had_touch_{false};
};
} // namespace outland::game::inventory
