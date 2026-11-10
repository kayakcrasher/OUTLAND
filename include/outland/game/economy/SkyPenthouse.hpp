#pragma once
#include "outland/game/economy/Economy.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

// The penthouse at the top of Kristala Turo (world::tower), Verda's tallest building: a glass suite
// on the 25th floor with a terrace all round and a pool, reached by the elevator from the lobby.
// For sale in the campaign: buy it up there (E / USE); then it is home - sleep in its bed till
// morning. Ownership is kept with the wallet (EconomyState::properties).
namespace outland::game::economy {

struct SkyPenthouse {
    static constexpr const char* property_id = "kristala_turo_penthouse";
    static constexpr std::int64_t price = 100000;

    [[nodiscard]] static bool owned(const EconomyState& wallet) {
        return std::find(wallet.properties.begin(), wallet.properties.end(), property_id) != wallet.properties.end();
    }
    // Pays and takes the keys; returns what to tell the player.
    static std::string buy(EconomyState& wallet) {
        if (owned(wallet)) return "The penthouse is already yours";
        if (wallet.money < price) return "The Kristala Turo penthouse costs " + std::to_string(price) + " kronoj - you have " + std::to_string(wallet.money);
        wallet.money -= price;
        wallet.properties.emplace_back(property_id);
        return "Bought the penthouse at the top of Kristala Turo! The whole island at your feet - welcome home.";
    }
};

} // namespace outland::game::economy
