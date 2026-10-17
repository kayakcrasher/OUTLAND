#include "outland/game/wardrobe/Wardrobe.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include <algorithm>
#include <cstdint>

namespace outland::game::wardrobe {
namespace {
std::uint64_t hash(std::string_view text) {
    std::uint64_t h = 1469598103934665603ULL;
    for (const char c : text) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
    return h;
}
// "character_female_07" -> "Virina stilo 7", "character_29" -> "Stilo 29", "character_27_hm" ->
// "Protekta kostumo 27" (the yellow hazard suits).
std::string look_name(const std::string& id) {
    std::string digits;
    for (const char c : id) if (c >= '0' && c <= '9') digits += c;
    while (digits.size() > 1 && digits.front() == '0') digits.erase(digits.begin());
    const bool female = id.find("female") != std::string::npos;
    const bool work = id.find("_hm") != std::string::npos;
    std::string name = work ? "Protekta kostumo" : "Stilo";
    if (female) name = work ? "Virina protekta kostumo" : "Virina stilo";
    return name + (digits.empty() ? "" : " " + digits);
}
}

std::string outfit_path(std::string_view outfit) {
    return "assets/verda/characters/outfits/" + std::string(outfit) + ".png";
}

void Catalogue::build(const characters::CharacterRegistry& registry) {
    looks_.clear();
    hats_.clear();
    const auto* player = registry.player();
    own_ = player ? "look_" + player->id : std::string();
    if (player) looks_.push_back({own_, "Viaj propraj vestoj", player->id, "", 0, false});
    for (const auto* body : registry.pool(characters::CharacterPool::Civilian)) {
        if (player && body->id == player->id) continue;
        looks_.push_back({"look_" + body->id, look_name(body->id), body->id, "",
                          180 + static_cast<int>(hash(body->id) % 8) * 45, false});
    }
    if (registry.find(std::string(green_hand_body))) {
        looks_.push_back({"look_verda_mano_soldato", "Ĉemizo de la Verda Mano", std::string(green_hand_body), "verda_mano_soldato", 400, true});
        looks_.push_back({"look_verda_mano_kapo", "Kostumo de la Verda Mano", std::string(green_hand_body), "verda_mano_kapo", 2500, true});
    }
    hats_.push_back({"hat_cowboy", "Vakera ĉapelo", "assets/verda/ranch/cowboy_hat.glb", 150});
    hats_.push_back({"hat_campaign", "Kampanja ĉapelo", "assets/verda/ranch/trooper_hat.glb", 300});
}

const Look* Catalogue::look(std::string_view id) const {
    for (const auto& l : looks_) if (l.id == id) return &l;
    return nullptr;
}

const Hat* Catalogue::hat(std::string_view id) const {
    for (const auto& h : hats_) if (h.id == id) return &h;
    return nullptr;
}

bool owns(const economy::EconomyState& state, const Catalogue& catalogue, std::string_view id) {
    if (!id.empty() && id == catalogue.own_clothes()) return true;
    return std::find(state.wardrobe.begin(), state.wardrobe.end(), id) != state.wardrobe.end();
}

const Look* wearing(const economy::EconomyState& state, const Catalogue& catalogue) {
    if (const auto* look = catalogue.look(state.wearing)) return look;
    return catalogue.look(catalogue.own_clothes());
}

const Hat* wearing_hat(const economy::EconomyState& state, const Catalogue& catalogue) {
    return catalogue.hat(state.hat);
}

bool wear(economy::EconomyState& state, const Catalogue& catalogue, std::string_view id) {
    if (id.empty()) {state.hat.clear(); return true;}
    if (!owns(state, catalogue, id)) return false;
    if (catalogue.hat(id)) {state.hat = std::string(id); return true;}
    if (!catalogue.look(id)) return false;
    // Your own clothes are worn as "nothing bought on".
    state.wearing = id == catalogue.own_clothes() ? std::string() : std::string(id);
    return true;
}

void take_off_hat(economy::EconomyState& state) { state.hat.clear(); }

std::string buy(economy::EconomyState& state, const Catalogue& catalogue, std::string_view id, bool green_hand_member) {
    const auto* look = catalogue.look(id);
    const auto* hat = catalogue.hat(id);
    if (!look && !hat) return "Not sold here";
    const std::string name = look ? look->name : hat->name;
    if (owns(state, catalogue, id)) {
        wear(state, catalogue, id);
        return "You put on: " + name;
    }
    if (look && look->green_hand && !green_hand_member) return "Only La Verda Mano wear the green hand";
    const int price = look ? look->price : hat->price;
    if (state.money < price) return "Not enough kronoj for " + name;
    if (state.wardrobe.size() >= 256) return "Your wardrobe is full";
    state.money -= price;
    // Your own clothes are never in the list (always yours); anything bought is.
    if (std::string(id) != catalogue.own_clothes()) state.wardrobe.emplace_back(id);
    wear(state, catalogue, id);
    return "Bought " + name + " for " + std::to_string(price) + " kr";
}

} // namespace outland::game::wardrobe
