#pragma once
#include "outland/game/economy/Economy.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

// The mansion on the roof of Parkumejo Centra (world::garage): three storeys and a private parking
// bay at the door, for sale in Explore. Buy it inside (E / USE); then it is home - sleep in the
// master bedroom till morning, wake up there when you die, and the gold-lined bay by the door is
// yours to park in. Ownership is kept with the Explore wallet (EconomyState::properties).
namespace outland::game::economy {

struct SkyMansion {
    static constexpr const char* property_id = "parkumejo_mansion";
    static constexpr std::int64_t price = 60000;

    [[nodiscard]] static bool owned(const EconomyState& wallet) {
        return std::find(wallet.properties.begin(), wallet.properties.end(), property_id) != wallet.properties.end();
    }
    // Pays and takes the keys; returns what to tell the player.
    static std::string buy(EconomyState& wallet) {
        if (owned(wallet)) return "The mansion is already yours";
        if (wallet.money < price) return "The mansion costs " + std::to_string(price) + " kronoj - you have " + std::to_string(wallet.money);
        wallet.money -= price;
        wallet.properties.emplace_back(property_id);
        return "Bought the mansion on Parkumejo Centra! Welcome home - the gold bay by the door is yours.";
    }
};

} // namespace outland::game::economy
