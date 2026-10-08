#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace outland::creator {

enum class CreatorAssetType {
    Building,
    Nature,
    Prop,
    Road,
    Gameplay
};

struct CreatorAsset {
    std::string_view id;
    std::string_view name;
    CreatorAssetType type;

    // Approximate footprint used by the placement ghost
    // and later by placement validation.
    float width;
    float depth;
};

class CreatorAssetCatalog {
public:
    static constexpr std::array<CreatorAsset, 10> assets{{
        {
            "rural_house",
            "Rural House",
            CreatorAssetType::Building,
            8.0F,
            7.0F
        },
        {
            "two_story_house",
            "Two-Story House",
            CreatorAssetType::Building,
            9.0F,
            9.0F
        },
        {
            "shop",
            "Shop",
            CreatorAssetType::Building,
            11.0F,
            8.0F
        },
        {
            "garage",
            "Garage",
            CreatorAssetType::Building,
            12.0F,
            10.0F
        },
        {
            "oak_tree",
            "Oak Tree",
            CreatorAssetType::Nature,
            4.0F,
            4.0F
        },
        {
            "tall_tree",
            "Tall Tree",
            CreatorAssetType::Nature,
            3.0F,
            3.0F
        },
        {
            "round_bush",
            "Round Bush",
            CreatorAssetType::Nature,
            2.0F,
            2.0F
        },
        {
            "scrub_bush",
            "Scrub Bush",
            CreatorAssetType::Nature,
            2.5F,
            2.5F
        },
        {
            "rock",
            "Rock",
            CreatorAssetType::Nature,
            2.0F,
            2.0F
        },
        {
            "wood_fence",
            "Wood Fence",
            CreatorAssetType::Prop,
            3.0F,
            0.4F
        }
    }};

    [[nodiscard]]
    static constexpr std::size_t size() {
        return assets.size();
    }

    [[nodiscard]]
    static constexpr const CreatorAsset&
    get(std::size_t index) {
        return assets[index % assets.size()];
    }
};

} // namespace outland::creator
