#pragma once
#include "outland/game/economy/Economy.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace outland::characters { class CharacterRegistry; }

// What Vestaĵejo Verda sells: every set of clothes on the island. A look is a whole outfit - one
// of the civilian bodies (each is dressed differently), or La Verda Mano's own clothes on their
// body (the white shirt with the green hand, the officers' charcoal suit; members only). A hat
// goes with any look: the Vakeroj's cattleman hat, the patrol's tan campaign hat.
//
// What you own and what you have on are kept in the save (economy::EconomyState: wardrobe,
// wearing, hat). The clothes you started in are always yours, free.
namespace outland::game::wardrobe {

struct Look {
    std::string id;          // "look_character_07", "look_verda_mano_kapo"
    std::string name;        // shown in the shop
    std::string character;   // the body (characters::CharacterRegistry id)
    std::string outfit;      // a texture swapped onto that body ("" for its own), see outfit_path
    int price{0};            // kronoj
    bool green_hand{false};  // sold only to La Verda Mano
};
struct Hat {
    std::string id, name, model;
    int price{0};
};

inline constexpr std::string_view green_hand_body = "character_05";
std::string outfit_path(std::string_view outfit);   // "verda_mano_kapo" -> its PNG

class Catalogue {
public:
    // Every civilian body in the registry (in the manifest's order, the player's own first),
    // then the Green Hand's two.
    void build(const characters::CharacterRegistry& registry);
    const std::vector<Look>& looks() const { return looks_; }
    const std::vector<Hat>& hats() const { return hats_; }
    const Look* look(std::string_view id) const;
    const Hat* hat(std::string_view id) const;
    const std::string& own_clothes() const { return own_; }   // the player's starting look
private:
    std::vector<Look> looks_;
    std::vector<Hat> hats_;
    std::string own_;
};

bool owns(const economy::EconomyState& state, const Catalogue& catalogue, std::string_view id);
// The look the player has on (their own clothes when nothing else is).
const Look* wearing(const economy::EconomyState& state, const Catalogue& catalogue);
const Hat* wearing_hat(const economy::EconomyState& state, const Catalogue& catalogue);

// Buy a look or hat (all or nothing) and put it on; already yours: just put it on. Returns what
// to tell the player.
std::string buy(economy::EconomyState& state, const Catalogue& catalogue, std::string_view id, bool green_hand_member);
// Put on something you own (a look, a hat, or "" for no hat). False if it is not yours.
bool wear(economy::EconomyState& state, const Catalogue& catalogue, std::string_view id);
void take_off_hat(economy::EconomyState& state);

} // namespace outland::game::wardrobe
