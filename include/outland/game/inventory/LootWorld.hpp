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
    static constexpr std::size_t max_pickups = 4096;
    bool reconcile(const world::VerdaRegion &, const ItemRegistry &, LootMode mode, Vector3 player,
                   float radius = 90);
    bool reconcile(const world::VerdaRegion &region, const ItemRegistry &registry, bool zombie,
                   Vector3 player, float radius = 90) {
        return reconcile(region, registry, zombie ? LootMode::Zombie : LootMode::Normal, player, radius);
    }
    int nearest(Vector3 player, float radius = 2.8F,
                const std::function<bool(Vector3)> &reachable = {}) const;
    bool drop(const ItemRegistry &, const std::string &item, int quantity, Vector3 position,
              int loaded_rounds = 0, const std::string &id = {});
    // LAN play: someone else took from a pickup (or all of it). One not rolled here yet stays gone
    // when it would be.
    void taken_elsewhere(const std::string &id, int remaining);
    [[nodiscard]] const WorldPickup *find(const std::string &id) const;
    bool restore(const ItemRegistry &, LootWorldState);
    LootWorldState &edit() {
        return state_;
    }
    const LootWorldState &state() const {
        return state_;
    }
    void clear() {
        state_ = {};
        elsewhere_.clear();
    }

  private:
    LootWorldState state_;
    std::vector<std::pair<std::string, int>> elsewhere_;
};
} // namespace outland::game::inventory
