#include "outland/game/economy/Economy.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <set>
namespace outland::game::economy {
bool valid_state(const EconomyState& s) {
    if(s.money<0 || s.money>1000000000 || !std::isfinite(s.restock_remaining) || s.restock_remaining<0 || s.restock_remaining>600 || s.stock.size()>4096) return false;
    std::set<std::pair<std::string,std::string>> keys;
    for(const auto& v:s.stock) if(v.shop.empty() || v.offer.empty() || v.quantity<0 || v.quantity>99999 ||
        !inventory::valid_text(v.shop) || !inventory::valid_text(v.offer) || !keys.emplace(v.shop,v.offer).second) return false;
    std::set<std::string> owned;
    if(s.properties.size()>64) return false;
    for(const auto& p:s.properties) if(p.empty() || !inventory::valid_text(p) || !owned.insert(p).second) return false;
    std::set<std::string> clothes;
    if(s.wardrobe.size()>256) return false;
    for(const auto& c:s.wardrobe) if(c.empty() || c.size()>100 || !inventory::valid_text(c) || !clothes.insert(c).second) return false;
    for(const auto* on:{&s.wearing,&s.hat}) if(!on->empty() && (on->size()>100 || !inventory::valid_text(*on) || !clothes.contains(*on))) return false;
    return true;
}
bool Economy::load(const inventory::ItemRegistry& items, const std::string& path, std::string& error) {
    std::ifstream in(path); std::string line; std::vector<Offer> parsed; std::set<std::string> ids;
    error.clear();
    if(!std::getline(in,line) || line!="id\tkind\titem\tbuy\tsell\tstock") {error="Invalid shop catalog header";return false;}
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line); Offer o; std::string extra;
        if(!std::getline(row,o.id,'\t') || !std::getline(row,o.kind,'\t') || !std::getline(row,o.item,'\t') ||
           !(row>>o.buy>>o.sell>>o.stock) || (row>>extra) || !items.find(o.item) || !ids.insert(o.id).second ||
           o.id.empty() || o.id.size()>100 || !inventory::valid_text(o.id) ||
           (o.kind!="general" && o.kind!="medical" && o.kind!="hardware" && o.kind!="furniture" && o.kind!="pub" && o.kind!="guns") ||
           o.buy<1 || o.buy>1000000 || o.sell<0 || o.sell>o.buy/2 || o.stock<1 || o.stock>99999 || parsed.size()>=1024) {
            error="Invalid shop offer: "+o.id;return false;
        }
        if(std::any_of(parsed.begin(),parsed.end(),[&](const Offer& v){return v.kind==o.kind && v.item==o.item;})) {error="Duplicate shop item";return false;}
        parsed.push_back(std::move(o));
    }
    if(in.bad() || parsed.empty()) {error="Empty shop catalog";return false;}
    offers_=std::move(parsed);return true;
}
void Economy::build(const std::vector<life::Place>& places) {
    shops_.clear();
    for(const auto& place:places) {
        std::string kind;
        switch(place.kind) {
        case life::PlaceKind::Shop:
            kind=place.building_id.find("gun_shop")!=std::string::npos ? "guns" :
                place.building_id.find("furniture")!=std::string::npos ? "furniture" :
                place.building_id.find("hardware")!=std::string::npos ? "hardware" : "general";
            break;
        case life::PlaceKind::Clinic: kind="medical";break;
        case life::PlaceKind::Garage: kind="hardware";break;
        case life::PlaceKind::Pub: kind="pub";break;
        default:continue;
        }
        if(shops_.size()>=256) break;
        shops_.push_back({place.id,kind=="guns" ? std::string("Armilejo") : kind+" store",kind,place.door});
    }
}
const Shop* Economy::nearest(Vector3 feet, const std::function<bool(Vector3)>& reachable) const {
    const Shop* best=nullptr;float distance=3.5F*3.5F;
    for(const auto& shop:shops_) {
        const float d=(feet.x-shop.door.x)*(feet.x-shop.door.x)+(feet.y-shop.door.y)*(feet.y-shop.door.y)+(feet.z-shop.door.z)*(feet.z-shop.door.z);
        if(d<distance && (!reachable || reachable(shop.door))) {distance=d;best=&shop;}
    }
    return best;
}
int Economy::stock(const EconomyState& state, const Shop& shop, const Offer& offer) const {
    const auto at=std::find_if(state.stock.begin(),state.stock.end(),[&](const Stock& s){return s.shop==shop.id && s.offer==offer.id;});
    return at==state.stock.end() ? offer.stock : at->quantity;
}
std::string Economy::trade(EconomyState& state, inventory::Inventory& bag, const inventory::ItemRegistry& items,
                          const Shop& shop, const Offer& offer, int qty, bool selling) const {
    if(!valid_state(state) || qty<1 || qty>999 || offer.kind!=shop.kind ||
        std::none_of(shops_.begin(),shops_.end(),[&](const Shop& s){return s.id==shop.id && s.kind==shop.kind;}) ||
        std::none_of(offers_.begin(),offers_.end(),[&](const Offer& o){return o.id==offer.id && o.item==offer.item && o.buy==offer.buy && o.sell==offer.sell && o.stock==offer.stock && o.kind==offer.kind;})) return "Invalid trade";
    inventory::Inventory next=bag;
    const auto price=static_cast<std::int64_t>(selling ? offer.sell : offer.buy)*qty;
    const int available=stock(state,shop,offer);
    if(selling) {
        if(offer.sell==0) return "This store does not buy that item";
        if(state.money>1000000000-price) return "Wallet limit reached";
        if(bag.count(offer.item)<qty) return "You do not carry enough";
        int remaining=qty;
        for(std::size_t i=0;i<bag.state().stacks.size() && remaining>0;++i) if(bag.state().stacks[i].item==offer.item) {
            const int take=std::min(remaining,bag.state().stacks[i].quantity);
            if(!bag.can_remove(items,i,take)) return "Unequip or empty your container first";
            remaining-=take;
        }
        next.remove(offer.item,qty);
        if(next.count(offer.item)==0 && items.find(offer.item)->action==inventory::ItemAction::Light) next.effects().light=false;
    } else {
        if(state.money<price) return "Not enough kronoj";
        if(available<qty) return "Out of stock - stores restock every 10 minutes";
        if(next.add(items,offer.item,qty)!=qty) return "Bag full or weapon already owned";
    }
    // Validate removal of equipped containers and all other inventory invariants too.
    inventory::Inventory checked;
    if(!checked.restore(items,next.state())) return "Cannot carry the resulting inventory";
    auto at=std::find_if(state.stock.begin(),state.stock.end(),[&](const Stock& s){return s.shop==shop.id && s.offer==offer.id;});
    if(at==state.stock.end()) {
        if(state.stock.size()>=4096) return "Store ledger full";
        state.stock.push_back({shop.id,offer.id,available});at=std::prev(state.stock.end());
    }
    at->quantity=selling ? std::min(offer.stock,available+qty) : available-qty;
    state.money+=selling ? price : -price;bag=std::move(checked);
    return std::string(selling ? "Sold " : "Bought ")+items.find(offer.item)->name+" x"+std::to_string(qty);
}
bool Economy::update(EconomyState& s, float dt, bool paused) const {
    if(paused || !std::isfinite(dt) || dt<=0) return false;
    s.restock_remaining=std::max(0.0F,s.restock_remaining-std::min(dt,.25F));
    if(s.restock_remaining>0) return false;
    s.stock.clear();s.restock_remaining=600;return true;
}
}
