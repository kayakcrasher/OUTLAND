#pragma once

#include "outland/assets/AssetDefinition.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace outland::assets {

class AssetRegistry {
public:
    bool add(AssetDefinition definition);

    [[nodiscard]]
    const AssetDefinition* find(
        const std::string& id
    ) const;

    [[nodiscard]]
    bool contains(
        const std::string& id
    ) const;

    [[nodiscard]]
    std::size_t size() const;

    void load_verda_defaults();

private:
    std::unordered_map<
        std::string,
        AssetDefinition
    > assets_;
};

} // namespace outland::assets
