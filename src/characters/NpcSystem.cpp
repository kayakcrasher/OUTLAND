#include "outland/characters/NpcSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/GameplayMarker.hpp"
#include <cmath>
#include <unordered_set>
namespace outland::characters {
void NpcSystem::reconcile(const world::VerdaRegion& region,const CharacterRegistry& registry) {
    std::vector<NpcInstance> next;
    std::unordered_set<std::string> seen;
    for (const auto& settlement:region.settlements()) for (const auto& marker:settlement.gameplay_markers) {
        if (!marker.enabled || marker.type!=world::GameplayMarkerType::NpcSpawn) continue;
        if (!std::isfinite(marker.position.x) || !std::isfinite(marker.position.y) ||
            !std::isfinite(marker.position.z) || !std::isfinite(marker.rotation_y)) continue;
        const std::string key=std::to_string(settlement.id.size())+":"+settlement.id+marker.id;
        if (!seen.insert(key).second) continue;
        const auto* definition=registry.choose(npc_pool_for_marker(marker.id),character_seed(key));
        if (!definition) continue; // Never substitute a civilian for an empty hostile/creature pool.
        next.push_back({key,definition->id,marker.position,marker.rotation_y});
    }
    actors_=std::move(next);
}
}
