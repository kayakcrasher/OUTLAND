#include "outland/assets/AssetRegistry.hpp"

#include <utility>

namespace outland::assets {

bool AssetRegistry::add(
    AssetDefinition definition
) {
    if (definition.id.empty()) {
        return false;
    }

    const std::string id =
        definition.id;

    return assets_.emplace(
        id,
        std::move(definition)
    ).second;
}

const AssetDefinition* AssetRegistry::find(
    const std::string& id
) const {
    const auto it =
        assets_.find(id);

    if (it == assets_.end()) {
        return nullptr;
    }

    return &it->second;
}

bool AssetRegistry::contains(
    const std::string& id
) const {
    return assets_.contains(id);
}

std::size_t AssetRegistry::size() const {
    return assets_.size();
}

void AssetRegistry::load_verda_defaults() {
    assets_.clear();

    add({
        "verda_house_rural_01",
        "Verdan Rural House 01",
        AssetCategory::Building,
        CollisionType::Compound,
        "assets/verda/models/buildings/house_rural_01.glb",
        "assets/verda/lod/house_rural_01_lod1.glb",
        "assets/verda/lod/house_rural_01_lod2.glb",
        500.0F,
        90.0F,
        220.0F,
        true,
        true,
        true
    });

    add({
        "verda_house_two_story_01",
        "Verdan Two Story House 01",
        AssetCategory::Building,
        CollisionType::Compound,
        "assets/verda/models/buildings/house_two_story_01.glb",
        "assets/verda/lod/house_two_story_01_lod1.glb",
        "assets/verda/lod/house_two_story_01_lod2.glb",
        600.0F,
        110.0F,
        260.0F,
        true,
        true,
        true
    });

    add({
        "verda_shop_01",
        "Verdan Village Shop 01",
        AssetCategory::Building,
        CollisionType::Compound,
        "assets/verda/models/buildings/shop_01.glb",
        "assets/verda/lod/shop_01_lod1.glb",
        "assets/verda/lod/shop_01_lod2.glb",
        500.0F,
        90.0F,
        220.0F,
        true,
        true,
        true
    });

    add({
        "verda_concrete_wall_01",
        "Verdan Concrete Wall 01",
        AssetCategory::Infrastructure,
        CollisionType::Box,
        "assets/verda/models/infrastructure/concrete_wall_01.glb",
        "",
        "",
        250.0F,
        80.0F,
        160.0F,
        true,
        true,
        false
    });

    add({
        "verda_utility_pole_01",
        "Verdan Utility Pole 01",
        AssetCategory::Infrastructure,
        CollisionType::Box,
        "assets/verda/models/infrastructure/utility_pole_01.glb",
        "",
        "",
        350.0F,
        100.0F,
        220.0F,
        true,
        true,
        false
    });

    add({
        "verda_tree_deciduous_01",
        "Verdan Deciduous Tree 01",
        AssetCategory::Vegetation,
        CollisionType::Box,
        "assets/verda/models/vegetation/tree_deciduous_01.glb",
        "assets/verda/lod/tree_deciduous_01_lod1.glb",
        "assets/verda/lod/tree_deciduous_01_lod2.glb",
        450.0F,
        70.0F,
        180.0F,
        true,
        true,
        false
    });

    add({
        "verda_rock_01",
        "Verdan Field Rock 01",
        AssetCategory::Prop,
        CollisionType::Box,
        "assets/verda/models/props/rock_01.glb",
        "",
        "",
        220.0F,
        70.0F,
        150.0F,
        true,
        true,
        false
    });

    add({
        "verda_sign_espera",
        "Espera Road Sign",
        AssetCategory::Infrastructure,
        CollisionType::Box,
        "assets/verda/models/infrastructure/sign_espera.glb",
        "",
        "",
        250.0F,
        80.0F,
        160.0F,
        true,
        true,
        false
    });
}

} // namespace outland::assets
