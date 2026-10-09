#pragma once
#include "outland/game/inventory/ItemRegistry.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raylib.h>
#include <functional>
#include <vector>
#include <cstdint>
namespace outland::game::inventory {
struct WorldPickup {
    std::string id, source, item;
    Vector3 position{};
    int quantity{1}, loaded_rounds{0};
};
struct LootSource {
    std::string key, table;
    Vector3 position{};
};
struct LootWorldState {
    std::vector<WorldPickup> pickups;
    std::vector<LootSource> sources;
    std::uint64_t next_drop{1};
};
class LootWorld {
  public:
    static constexpr std::size_t max_pickups = 1024;
    bool reconcile(const world::VerdaRegion &, const ItemRegistry &, bool zombie, Vector3 player,
                   float radius = 90);
    int nearest(Vector3 player, float radius = 2.8F,
                const std::function<bool(Vector3)> &reachable = {}) const;
    bool drop(const ItemRegistry &, const std::string &item, int quantity, Vector3 position,
              int loaded_rounds = 0);
    bool restore(const ItemRegistry &, LootWorldState);
    LootWorldState &edit() {
        return state_;
    }
    const LootWorldState &state() const {
        return state_;
    }
    void clear() {
        state_ = {};
    }

  private:
    LootWorldState state_;
};
} // namespace outland::game::inventory
