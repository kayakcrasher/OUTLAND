#pragma once
#include "outland/game/economy/Economy.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

// The hidden stash: somebody left a holdall with a million kronoj in an empty house on the island
// (where: game::interiors::stash_site). Pick it up (E / USE) and it is the player's, once per
// Explore wallet; the wallet remembers it was taken (EconomyState::properties).
namespace outland::game::economy {

struct Stash {
    static constexpr const char* property_id = "hidden_stash";
    static constexpr std::int64_t amount = 1000000;

    [[nodiscard]] static bool taken(const EconomyState& wallet) {
        return std::find(wallet.properties.begin(), wallet.properties.end(), property_id) != wallet.properties.end();
    }
    // Into the wallet; returns what to tell the player.
    static std::string take(EconomyState& wallet) {
        if (taken(wallet)) return "The holdall is empty";
        if (wallet.properties.size() >= 64) return "You can't carry any more";
        wallet.money += amount;
        wallet.properties.emplace_back(property_id);
        return "A holdall stuffed with cash - 1,000,000 kronoj! Somebody's going to miss this.";
    }
};

} // namespace outland::game::economy
