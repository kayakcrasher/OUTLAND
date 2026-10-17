#include "outland/game/economy/Homes.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <algorithm>
#include <cmath>

namespace outland::game::economy {
namespace {
std::uint64_t mix(const std::string& text) {
    std::uint64_t h = 1469598103934665603ULL;
    for (const char c : text) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
    h ^= h >> 30; h *= 0xbf58476d1ce4e5b9ULL;
    return h ^ (h >> 31);
}
int number_in(const std::string& id) {
    int n = 0;
    bool any = false;
    for (std::size_t i = id.find_last_of('_') + 1; i < id.size(); ++i)
        if (id[i] >= '0' && id[i] <= '9') {n = n * 10 + (id[i] - '0'); any = true;}
    return any ? n : 0;
}
// A point in a building's frame: x across the front, z from the front (-) to the back (+).
Vector3 at(Vector3 centre, float yaw, float x, float z) {
    const float a = yaw * DEG2RAD;
    return {centre.x + x * std::cos(a) + z * std::sin(a), centre.y, centre.z - x * std::sin(a) + z * std::cos(a)};
}
}

std::vector<Listing> plan_homes(const world::VerdaRegion& region) {
    const auto& towns = region.settlements();
    const auto town_of = [&](Vector3 p) {
        int best = 0;
        float d = 1e30F;
        for (std::size_t t = 0; t < towns.size(); ++t) {
            if (towns[t].id.starts_with("ranch_")) continue;
            const float f = std::hypot(p.x - towns[t].center.x, p.z - towns[t].center.z);
            if (f < d) {d = f; best = static_cast<int>(t);}
        }
        return best;
    };
    // Main Street flats in the towns other than Verda City: the two furthest from each centre.
    std::vector<Listing> flats;
    for (std::size_t t = 0; t < towns.size(); ++t) {
        const auto& town = towns[t];
        if (town.id == "capital_verda" || town.id.starts_with("ranch_")) continue;
        std::vector<const world::WorldAsset*> rows;
        for (const auto& owner : towns)
            for (const auto& asset : owner.assets)
                if (asset.model_path.find("/city/main_street/") != std::string::npos && asset.purpose == world::BuildingPurpose::Auto &&
                    town_of(asset.position) == static_cast<int>(t))
                    rows.push_back(&asset);
        if (rows.size() < 6) continue;   // a town needs its rows for shops, a pub and the gangs
        std::sort(rows.begin(), rows.end(), [&](const auto* a, const auto* b) {
            const float da = std::hypot(a->position.x - town.center.x, a->position.z - town.center.z);
            const float db = std::hypot(b->position.x - town.center.x, b->position.z - town.center.z);
            return da != db ? da > db : a->id < b->id;
        });
        for (std::size_t k = 0; k < 2; ++k) {
            const auto& asset = *rows[k];
            Listing home;
            home.building = asset.id;
            home.id = "home_" + asset.id;
            home.name = (town.name.empty() ? town.id : town.name) + " - apartamento " + std::to_string(k + 1);
            home.settlement = static_cast<int>(t);
            home.centre = asset.position;
            home.yaw = asset.rotation_y;
            home.size = asset.size;
            home.house = false;
            home.price = 1500 + static_cast<std::int64_t>(mix(asset.id) % 7) * 250;
            home.board = at(home.centre, home.yaw, 3.6F, -home.size.z * .5F - 2.2F);
            flats.push_back(std::move(home));
        }
    }
    // Detached houses in Okcidenta Verda, spread through the suburb, for the rest.
    std::vector<std::pair<int, const world::Building*>> houses;
    std::vector<int> house_town;
    for (std::size_t t = 0; t < towns.size(); ++t)
        for (const auto& b : towns[t].buildings)
            if (b.id.find("okcidento_house") != std::string::npos && b.enterable && b.purpose != world::BuildingPurpose::Vacant)
                houses.push_back({number_in(b.id), &b});
    std::sort(houses.begin(), houses.end(), [](const auto& a, const auto& b) {return a.first < b.first;});
    const int wanted = std::max(0, home_listings - static_cast<int>(flats.size()));
    std::vector<Listing> out;
    for (int k = 0; k < wanted && !houses.empty(); ++k) {
        const auto* b = houses[static_cast<std::size_t>((static_cast<std::size_t>(k) * houses.size()) / static_cast<std::size_t>(wanted) +
                                                        houses.size() / static_cast<std::size_t>(2 * wanted)) % houses.size()].second;
        Listing home;
        home.building = b->id;
        home.id = "home_" + b->id;
        home.name = "Okcidenta Verda - domo " + std::to_string(number_in(b->id));
        home.settlement = town_of(b->position);
        home.centre = {b->position.x, world::terrain::TerrainHeight::sample(b->position.x, b->position.z), b->position.z};
        home.yaw = b->rotation_y;
        home.size = b->size;
        home.house = true;
        home.price = 3500 + static_cast<std::int64_t>(mix(b->id) % 9) * 500;
        home.board = at(home.centre, home.yaw, 2.6F, -home.size.z * .5F - 1.3F);   // in the front garden, inside the fence
        out.push_back(std::move(home));
    }
    for (auto& flat : flats) {
        if (static_cast<int>(out.size()) >= home_listings) break;
        out.push_back(std::move(flat));
    }
    return out;
}

std::vector<Vacated> vacate(world::VerdaRegion& region, const std::vector<Listing>& homes) {
    std::vector<Vacated> out;
    for (auto& town : region.runtime_settlements()) {
        for (auto& b : town.buildings)
            for (const auto& home : homes)
                if (b.id == home.building) {out.push_back({b.id, b.purpose, false}); b.purpose = world::BuildingPurpose::Vacant;}
        for (auto& a : town.assets)
            for (const auto& home : homes)
                if (a.id == home.building) {out.push_back({a.id, a.purpose, true}); a.purpose = world::BuildingPurpose::Vacant;}
    }
    return out;
}

void restore_purposes(world::VerdaRegion& region, const std::vector<Vacated>& vacated) {
    for (auto& town : region.runtime_settlements()) {
        for (auto& b : town.buildings)
            for (const auto& v : vacated) if (!v.asset && b.id == v.id) b.purpose = v.purpose;
        for (auto& a : town.assets)
            for (const auto& v : vacated) if (v.asset && a.id == v.id) a.purpose = v.purpose;
    }
}

bool owns(const EconomyState& wallet, const Listing& home) {
    return std::find(wallet.properties.begin(), wallet.properties.end(), home.id) != wallet.properties.end();
}

std::string buy(EconomyState& wallet, const Listing& home) {
    if (owns(wallet, home)) return "It is already yours - welcome home";
    if (wallet.money < home.price)
        return home.name + " costs " + std::to_string(home.price) + " kr - you have " + std::to_string(wallet.money);
    if (wallet.properties.size() >= 64) return "You own all the homes you can";
    wallet.money -= home.price;
    wallet.properties.push_back(home.id);
    return "Bought " + home.name + "! The keys are yours.";
}

bool inside(const Listing& home, Vector3 feet, float margin) {
    const float a = home.yaw * DEG2RAD, dx = feet.x - home.centre.x, dz = feet.z - home.centre.z;
    const float x = dx * std::cos(a) - dz * std::sin(a), z = dx * std::sin(a) + dz * std::cos(a);
    return std::abs(x) < home.size.x * .5F - margin && std::abs(z) < home.size.z * .5F - margin &&
           feet.y > home.centre.y - 1.0F && feet.y < home.centre.y + 3.0F;
}

life::Place place_of(const Listing& home) {
    life::Place place;
    place.id = place.building_id = home.building;
    place.kind = life::PlaceKind::Home;
    place.settlement = home.settlement;
    place.interior = home.centre;
    place.building_yaw = home.yaw;
    place.building_size = home.size;
    place.indoor = true;
    return place;
}

} // namespace outland::game::economy
