#pragma once
#include "outland/game/inventory/Inventory.hpp"
#include "outland/game/life/LifeTypes.hpp"
#include <cstdint>
#include <functional>
namespace outland::game::economy {
struct Stock { std::string shop, offer; int quantity{0}; };
struct EconomyState {
    std::int64_t money{250}; // whole Verdan kronoj, never floating point currency
    float restock_remaining{600};
    std::vector<Stock> stock;
    std::vector<std::string> properties; // homes the player owns (economy::Penthouse::property_id)
    std::vector<std::string> wardrobe;   // looks and hats bought at Vestaĵejo Verda (game::wardrobe ids)
    std::string wearing, hat;            // what the player has on; empty: their own clothes, no hat
};
bool valid_state(const EconomyState&);
struct Offer { std::string id, kind, item; int buy{0}, sell{0}, stock{0}; };
struct Shop { std::string id, name, kind; Vector3 door{}; };
class Economy {
public:
    bool load(const inventory::ItemRegistry&, const std::string& path, std::string& error);
    void build(const std::vector<life::Place>&);
    const std::vector<Shop>& shops() const { return shops_; }
    const std::vector<Offer>& offers() const { return offers_; }
    const Shop* nearest(Vector3 feet, const std::function<bool(Vector3)>& reachable={}) const;
    int stock(const EconomyState&, const Shop&, const Offer&) const;
    // All-or-nothing transfers: capacity, cash and stock are checked before changing either state.
    std::string trade(EconomyState&, inventory::Inventory&, const inventory::ItemRegistry&,
                      const Shop&, const Offer&, int quantity, bool selling) const;
    bool update(EconomyState&, float dt, bool paused) const;
private:
    std::vector<Offer> offers_; std::vector<Shop> shops_;
};
}
