#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace outland::characters {
enum class CharacterPool { Civilian, Emergency, Hostile, Creature, Arms };
struct CharacterDefinition {
    std::string id, name, category, role, source_model, model_path, texture_path, source;
    CharacterPool pool{CharacterPool::Civilian};
    bool z_up{false};
    float facing_degrees{0};
    float height{1.85F};
};

// Metadata only: safe to load/query without a graphics context. Loading is transactional.
class CharacterRegistry {
public:
    bool load(const std::string& manifest_path, std::string& error);
    const std::vector<CharacterDefinition>& assets() const { return assets_; }
    const CharacterDefinition* find(const std::string& id) const;
    const CharacterDefinition* find_model(const std::string& path) const;
    const CharacterDefinition* choose(CharacterPool pool, std::uint64_t seed) const;
    const CharacterDefinition* player() const;
    std::vector<const CharacterDefinition*> pool(CharacterPool category) const;
private:
    std::vector<CharacterDefinition> assets_;
};
CharacterPool npc_pool_for_marker(const std::string& marker_id);
std::uint64_t character_seed(const std::string& key);
} // namespace outland::characters
