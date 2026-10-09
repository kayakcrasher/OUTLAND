#pragma once
#include "outland/characters/CharacterRegistry.hpp"
#include <raylib.h>
#include <string>
#include <vector>
namespace outland::world { class VerdaRegion; }
namespace outland::characters {
struct NpcInstance {
    std::string spawn_key, character_id;
    Vector3 position{};
    float yaw_degrees{0};
};
class NpcSystem {
public:
    // Runtime actors are derived from existing saves; never write actors into the map.
    void reconcile(const world::VerdaRegion& region,const CharacterRegistry& registry);
    const std::vector<NpcInstance>& actors() const { return actors_; }
private:
    std::vector<NpcInstance> actors_;
};
}
