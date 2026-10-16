#include "outland/game/inventory/LootWorld.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
namespace outland::game::inventory {
namespace {
bool finite(Vector3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x) < 1e6F &&
           std::abs(v.y) < 1e6F && std::abs(v.z) < 1e6F;
}
float distance2(Vector3 a, Vector3 b) {
    const auto x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return x * x + y * y + z * z;
}
bool same(const LootSource &a, const LootSource &b) {
    return a.table == b.table && distance2(a.position, b.position) < .000001F;
}
std::uint32_t random(std::uint32_t &state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
} // namespace
bool LootWorld::reconcile(const world::VerdaRegion &region, const ItemRegistry &registry,
                          LootMode mode, Vector3 player, float radius) {
    if (!finite(player) || !std::isfinite(radius) || radius <= 0)
        return false;
    std::unordered_map<std::string, LootSource> current;
    for (const auto &settlement : region.settlements())
        for (const auto &marker : settlement.gameplay_markers) {
            if (!marker.enabled || marker.type != world::GameplayMarkerType::LootSpawn ||
                !finite(marker.position) || !valid_text(marker.id) || !valid_text(settlement.id))
                continue;
            const auto key = std::to_string(settlement.id.size()) + ":" + settlement.id + marker.id;
            if (!valid_text(key))
                continue;
            current.try_emplace(
                key, LootSource{key, registry.table_for_marker(marker.id), marker.position});
        }
    bool changed = false;
    for (auto it = state_.sources.begin(); it != state_.sources.end();) {
        const auto found = current.find(it->key);
        if (found == current.end() || !same(*it, found->second)) {
            const auto key = it->key;
            std::erase_if(state_.pickups, [&](const auto &p) { return p.source == key; });
            it = state_.sources.erase(it);
            changed = true;
        } else
            ++it;
    }
    std::unordered_set<std::string> generated;
    for (const auto &source : state_.sources)
        generated.insert(source.key);
    // Deterministic order matters when the bounded world pickup pool is full.
    std::vector<LootSource> active;
    for (const auto &[key, source] : current)
        if (!generated.contains(key) && distance2(player, source.position) <= radius * radius)
            active.push_back(source);
    std::sort(active.begin(), active.end(),
              [](const auto &a, const auto &b) { return a.key < b.key; });
    for (const auto &source : active) {
        const auto *table = registry.table(source.table, mode);
        // Battle Royale lays a stack of ammunition beside every gun, so up to two pickups a roll.
        const std::size_t most = static_cast<std::size_t>(table ? table->rolls : 0) * (mode == LootMode::Royale ? 2 : 1);
        if (!table || state_.pickups.size() + most > max_pickups ||
            state_.sources.size() >= 10000)
            continue;
        std::uint32_t seed = static_cast<std::uint32_t>(
            characters::character_seed(source.key + loot_mode_name(mode)));
        if (seed == 0)
            seed = 1;
        int total = 0;
        for (const auto &entry : table->entries)
            total += entry.weight;
        for (int roll = 0; roll < table->rolls; ++roll) {
            int choice = static_cast<int>(random(seed) % static_cast<unsigned>(total));
            const LootEntry *selected = &table->entries.back();
            for (const auto &entry : table->entries) {
                choice -= entry.weight;
                if (choice < 0) {
                    selected = &entry;
                    break;
                }
            }
            const int quantity =
                selected->minimum +
                static_cast<int>(random(seed) %
                                 static_cast<unsigned>(selected->maximum - selected->minimum + 1));
            const float angle =
                static_cast<float>(roll) * 6.283185F / static_cast<float>(table->rolls);
            Vector3 position{source.position.x + std::cos(angle) * .65F, source.position.y,
                             source.position.z + std::sin(angle) * .65F};
            position.y = std::max(position.y,
                                  world::terrain::TerrainHeight::sample(position.x, position.z)) +
                         .06F;
            state_.pickups.push_back({source.key + "/" + std::to_string(roll), source.key,
                                      selected->item, position, quantity, 0});
            const auto *item = registry.find(selected->item);
            if (mode == LootMode::Royale && item)
                if (const auto *ammo = registry.ammo_for(*item)) {
                    // Beside the gun, a little further round the circle: two magazines' worth.
                    const float beside = angle + .45F;
                    Vector3 at{source.position.x + std::cos(beside) * .95F, source.position.y,
                               source.position.z + std::sin(beside) * .95F};
                    at.y = std::max(at.y, world::terrain::TerrainHeight::sample(at.x, at.z)) + .06F;
                    const int rounds = std::clamp(combat::definition(*item->weapon).magazine * 2, 5, ammo->stack_size);
                    state_.pickups.push_back({source.key + "/" + std::to_string(roll) + "a", source.key, ammo->id, at, rounds, 0});
                }
        }
        state_.sources.push_back(source);
        changed = true;
    }
    // Pickups taken by someone else (LAN play) before they were rolled here.
    for (std::size_t e = 0; e < elsewhere_.size();) {
        const auto found = std::find_if(state_.pickups.begin(), state_.pickups.end(), [&](const auto &p) { return p.id == elsewhere_[e].first; });
        if (found == state_.pickups.end()) {++e; continue;}
        if (elsewhere_[e].second <= 0) state_.pickups.erase(found);
        else found->quantity = std::min(found->quantity, elsewhere_[e].second);
        elsewhere_.erase(elsewhere_.begin() + static_cast<std::ptrdiff_t>(e));
        changed = true;
    }
    return changed;
}
int LootWorld::nearest(Vector3 player, float radius,
                       const std::function<bool(Vector3)> &reachable) const {
    if (!finite(player) || !std::isfinite(radius) || radius <= 0)
        return -1;
    float best = radius * radius;
    int result = -1;
    for (std::size_t i = 0; i < state_.pickups.size(); ++i) {
        const auto &pickup = state_.pickups[i];
        const float distance = distance2(player, pickup.position);
        if (distance < best && (!reachable || reachable(pickup.position))) {
            best = distance;
            result = static_cast<int>(i);
        }
    }
    return result;
}
bool LootWorld::drop(const ItemRegistry &registry, const std::string &item_id, int quantity,
                     Vector3 position, int loaded, const std::string &id) {
    const auto *item = registry.find(item_id);
    if (!item || !finite(position) || quantity < 1 || quantity > 999 ||
        state_.pickups.size() >= max_pickups || loaded < 0 ||
        (loaded > 0 && (item->category != ItemCategory::Weapon ||
                        loaded > combat::definition(*item->weapon).magazine)))
        return false;
    if (item->category == ItemCategory::Weapon && quantity != 1)
        return false;
    position.y =
        std::max(position.y, world::terrain::TerrainHeight::sample(position.x, position.z)) + .06F;
    std::string key = id;
    if (!key.empty() && std::any_of(state_.pickups.begin(), state_.pickups.end(), [&](const auto &p) { return p.id == key; }))
        return false;
    while (key.empty() || std::any_of(state_.pickups.begin(), state_.pickups.end(),
                                      [&](const auto &p) { return p.id == key; })) {
        if (state_.next_drop >= 1000000000000ULL)
            return false;
        key = "drop_" + std::to_string(state_.next_drop++);
    }
    state_.pickups.push_back({key, "", item_id, position, quantity, loaded});
    return true;
}

void LootWorld::taken_elsewhere(const std::string &id, int remaining) {
    auto &pickups = state_.pickups;
    const auto found = std::find_if(pickups.begin(), pickups.end(), [&](const auto &p) { return p.id == id; });
    if (found == pickups.end()) {
        std::erase_if(elsewhere_, [&](const auto &e) { return e.first == id; });
        elsewhere_.emplace_back(id, remaining);
        return;
    }
    if (remaining <= 0) pickups.erase(found);
    else found->quantity = std::min(found->quantity, remaining);
}

const WorldPickup *LootWorld::find(const std::string &id) const {
    for (const auto &p : state_.pickups) if (p.id == id) return &p;
    return nullptr;
}
bool LootWorld::restore(const ItemRegistry &registry, LootWorldState state) {
    if (state.pickups.size() > max_pickups || state.sources.size() > 10000 || state.next_drop < 1 ||
        state.next_drop >= 1000000000000ULL)
        return false;
    std::unordered_set<std::string> sources, ids;
    for (const auto &source : state.sources)
        if (source.key.empty() || !valid_text(source.key) || !finite(source.position) ||
            !registry.table(source.table, false) || !sources.insert(source.key).second)
            return false;
    for (const auto &p : state.pickups) {
        const auto *item = registry.find(p.item);
        if (!item || p.id.empty() || !valid_text(p.id) || !ids.insert(p.id).second ||
            !finite(p.position) || p.quantity < 1 || p.quantity > 999 ||
            (!p.source.empty() && !sources.contains(p.source)) || p.loaded_rounds < 0)
            return false;
        if (item->category == ItemCategory::Weapon) {
            if (p.quantity != 1 || p.loaded_rounds > combat::definition(*item->weapon).magazine)
                return false;
        } else if (p.loaded_rounds != 0)
            return false;
    }
    state_ = std::move(state);
    return true;
}
} // namespace outland::game::inventory
