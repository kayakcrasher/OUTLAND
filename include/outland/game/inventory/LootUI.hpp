#pragma once
#include "outland/game/inventory/LootSession.hpp"
#include "outland/assets/ModelCache.hpp"
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
namespace outland::game::inventory {
// A quest in hand, for the bag's QUESTS section (the story's goal, the gang's or the Policio's job,
// the load for Verda Transporto): the HUD shows only a line; the whole brief is here.
struct QuestNote {
    std::string title;      // "KILL RIVALS  ·  La Verda Mano"
    std::string detail;     // what to do
    std::string status;     // "2 of 3  ·  4:10 left  ·  350 m"
    Color colour{255, 214, 120, 255};
    std::vector<std::pair<std::string, std::string>> table{};   // a record: label and value, two to a row
};
struct InventoryActions {
    bool close{false}, use{false}, drop_one{false}, drop_stack{false};
    std::size_t selected{0};
};
class LootUI {
  public:
    explicit LootUI(std::string asset_root) : asset_root_(std::move(asset_root)) {}
    // White, green, blue, purple.
    static Color rarity_color(Rarity rarity);
    void opened() {
        warm_ = true;
    }
    InventoryActions update(const Inventory &, int width, int height, bool focused);
    void draw_inventory(const ItemRegistry &, const LootSession &, const combat::WeaponSystem &,
                        int width, int height) const;
    // The quests to show (set before drawing). Wide screens show them beside the bag; narrow ones
    // behind the QUESTS button (action 6).
    void set_quests(std::vector<QuestNote> quests) { quests_ = std::move(quests); }
    [[nodiscard]] bool showing_quests() const { return quests_view_; }
    static bool wide(int width, int height);
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
    bool warm_{true}, had_touch_{false}, quests_view_{false};
    std::vector<QuestNote> quests_;
};
} // namespace outland::game::inventory
