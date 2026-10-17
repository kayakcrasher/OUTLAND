#include "outland/characters/CharacterRegistry.hpp"
#include "outland/game/interiors/Furnishing.hpp"
#include "outland/game/wardrobe/Wardrobe.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>

using namespace outland::game;

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {std::cerr << "[FAIL] " << message << '\n'; ++failures;}
}
life::Place main_street(const std::string& id, life::PlaceKind kind, float width, int storeys, float yaw) {
    life::Place p;
    p.id = p.building_id = id;
    p.kind = kind;
    p.interior = {100, 6.2F, -40};
    p.building_size = {width, storeys * 3.6F + .9F, 20};
    p.building_yaw = yaw;
    return p;
}
life::Place house(const std::string& id, float yaw) {
    life::Place p;
    p.id = p.building_id = id;
    p.kind = life::PlaceKind::Home;
    p.interior = {-238, 0, -174};
    p.building_size = {11, 7, 9};
    p.building_yaw = yaw;
    return p;
}
// Local (x across the front, z front to back) of a world point in the place's frame.
Vector2 local(const life::Place& p, Vector3 at) {
    const float a = p.building_yaw * DEG2RAD, dx = at.x - p.interior.x, dz = at.z - p.interior.z;
    return {dx * std::cos(a) - dz * std::sin(a), dx * std::sin(a) + dz * std::cos(a)};
}
}

int main() {
    // ---- furniture
    {
        const auto pub = main_street("capital_verda:pub:creator_city_main_street_dark_12m_2st_kit9", life::PlaceKind::Pub, 12, 2, 90);
        check(interiors::shell_of(pub) == interiors::Shell::MainStreet && interiors::storeys_of(pub) == 2, "a Main Street pub is a known shell");
        const auto pieces = interiors::furnish_pub(pub, pub.interior.y);
        std::set<std::string> models;
        bool inside = true, clear_middle = true, clear_front = true, clear_stairs = true, level = true;
        for (const auto& piece : pieces) {
            models.insert(piece.model.substr(piece.model.rfind('/') + 1));
            const auto size = interiors::footprint(piece.model);
            inside = inside && interiors::inside(pub, piece.at, .3F);
            const Vector2 l = local(pub, piece.at);
            clear_middle = clear_middle && std::hypot(l.x, l.y) > 1.5F;
            // The front doors: alternate 3 m bays (bay i is a door when i % 2 == (bays / 2) % 2).
            const int bays = static_cast<int>(std::lround(pub.building_size.x / 3)), bay = static_cast<int>(std::floor((l.x + pub.building_size.x * .5F) / 3));
            clear_front = clear_front && !(l.y < -pub.building_size.z * .5F + 2.5F && bay % 2 == (bays / 2) % 2);
            // The ground-floor flight up the +X wall (x > 4.4, z -7.5 .. -2.4).
            clear_stairs = clear_stairs && !(l.x + size.x * .5F > 4.3F && l.y < -2.3F && l.y > -7.6F);
            level = level && std::abs(piece.at.y - (pub.interior.y + .2F)) < 1e-3F;
        }
        for (const char* want : {"bar_counter.glb", "bar_stool.glb", "pub_table.glb", "pub_booth.glb", "jukebox.glb", "dartboard.glb"})
            check(models.contains(want), "a pub has its bar, stools, tables, booth, jukebox and darts");
        check(inside, "the pub's furniture is inside the building");
        check(clear_middle && clear_front && clear_stairs, "the middle, the front and the stairs stay clear");
        check(level, "on the ground floor (0.2 m slab)");
        const auto wide = interiors::furnish_pub(main_street("x:pub:main_street_red_18m_3st", life::PlaceKind::Pub, 18, 3, 0), 0);
        bool pool = false;
        for (const auto& piece : wide) pool = pool || piece.model.ends_with("pool_table.glb");
        check(pool, "a wide pub has a pool table");
    }
    {
        // About one home in four, never all; the same ones every time.
        int furnished = 0, total = 0;
        for (int i = 1; i <= 400; ++i) {
            const auto h = house("capital_verda:home:verda_okcidento_house_" + std::to_string(i), 180);
            ++total;
            furnished += interiors::furnished_home(h);
            check(interiors::furnished_home(h) == interiors::furnished_home(h), "the choice is fixed");
        }
        check(furnished > total / 6 && furnished < total / 3, "about a quarter of the homes are furnished");
        // A furnished house: kitchen, table, sofa and television, bed, wardrobe; all inside.
        life::Place h;
        for (int i = 1; i <= 400 && h.id.empty(); ++i) {
            const auto c = house("capital_verda:home:verda_okcidento_house_" + std::to_string(i), 180);
            if (interiors::furnished_home(c)) h = c;
        }
        const auto pieces = interiors::furnish_home(h, 3.0F);
        std::set<std::string> models;
        bool inside = true, door = true;
        for (const auto& piece : pieces) {
            models.insert(piece.model.substr(piece.model.rfind('/') + 1));
            inside = inside && interiors::inside(h, piece.at, .2F);
            const Vector2 l = local(h, piece.at);
            door = door && !(std::abs(l.x) < 1.0F && l.y < -h.building_size.z * .5F + 1.5F);   // the doorway, middle of the front
            check(std::abs(piece.at.y - 3.44F) < 1e-3F, "a house's floor is its foundation, 0.44 m up");
        }
        for (const char* want : {"kitchen_run.glb", "dining_set.glb", "tv_stand.glb", "bed_double.glb", "wardrobe.glb"})
            check(models.contains(want), "a furnished home has a kitchen, a table, a television, a bed and a wardrobe");
        check(models.contains("sofa_green.glb") || models.contains("sofa_blue.glb"), "and a sofa");
        check(inside && door, "a house's furniture is inside, the doorway clear");
        // Shops, offices and Creator-built places are left as they are.
        auto shop = main_street("a:shop:main_street_red_12m_2st", life::PlaceKind::Shop, 12, 2, 0);
        check(interiors::furnish({shop}, nullptr).empty(), "shops are not furnished");
        life::Place marker = h;
        marker.building_id = "creator_marker_building_purpose_kit_3";
        marker.building_size = {3, 3, 3};
        check(interiors::shell_of(marker) == interiors::Shell::None && !interiors::furnished_home(marker), "unknown shells are left alone");
    }
    // ---- the clothing store
    {
        std::vector<life::Place> places{main_street("capital_verda:home:main_street_tan_12m_2st_kit3", life::PlaceKind::Home, 12, 2, 0),
                                        main_street("capital_verda:home:main_street_tan_18m_3st_kit1", life::PlaceKind::Home, 18, 3, 0),
                                        main_street("capital_verda:pub:main_street_red_18m_3st_kit2", life::PlaceKind::Pub, 18, 3, 0)};
        places[0].interior = {10, 0, 0};
        places[1].interior = {-99, 0, 20};
        places[2].interior = {5, 0, 5};
        for (auto& p : places) p.settlement = 0;
        const auto* site = interiors::store_site(places, 0, {0, 0, 0});
        check(site && site->id == places[1].id, "the store takes the nearest three-storey 18 m building, never the pub");
        const auto store = interiors::clothing_store(*site, 46);
        check(store.storeys == 3 && store.mannequins.size() == 46, "every look but your own on a mannequin, over three floors");
        std::set<int> floors;
        bool inside = true, apart = true;
        for (std::size_t i = 0; i < store.mannequins.size(); ++i) {
            const auto& m = store.mannequins[i];
            floors.insert(m.storey);
            inside = inside && interiors::inside(*site, m.at, .3F);
            check(std::abs(m.at.y - (site->interior.y + interiors::floor_height(*site, m.storey) + .15F)) < 1e-3F, "a mannequin stands on its plinth");
            for (std::size_t j = 0; j < i; ++j)
                if (store.mannequins[j].storey == m.storey)
                    apart = apart && std::hypot(m.at.x - store.mannequins[j].at.x, m.at.z - store.mannequins[j].at.z) > 1.2F;
        }
        check(floors.size() == 3 && inside && apart, "mannequins on every floor, inside, room to walk between");
        check(store.hats.size() == 2 && interiors::inside(*site, store.counter, .5F) && interiors::inside(*site, store.fitting, .5F),
              "a counter, fitting rooms and a hat stand with two hats");
        // The window display faces the street.
        const auto& window = store.mannequins.front();
        const float a = window.yaw * DEG2RAD;
        check(local(*site, {window.at.x + std::sin(a), 0, window.at.z + std::cos(a)}).y < local(*site, window.at).y, "the window mannequins face out");
    }
    // ---- the wardrobe
    {
        outland::characters::CharacterRegistry registry;
        std::string error;
        if (!registry.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/characters/character_manifest.tsv", error)) {
            std::cerr << error << '\n';
            return 1;
        }
        wardrobe::Catalogue catalogue;
        catalogue.build(registry);
        const auto civilians = registry.pool(outland::characters::CharacterPool::Civilian).size();
        check(catalogue.looks().size() == civilians + 2, "every civilian body's clothes, and the Green Hand's two");
        check(catalogue.hats().size() == 2, "the cattleman's hat and the campaign hat");
        std::set<std::string> ids;
        bool priced = true;
        for (const auto& look : catalogue.looks()) {
            ids.insert(look.id);
            priced = priced && (look.id == catalogue.own_clothes() ? look.price == 0 : look.price >= 150);
            check(registry.find(look.character) != nullptr, "every look is a real body");
            if (!look.outfit.empty()) check(std::filesystem::exists(std::string(OUTLAND_SOURCE_DIR) + "/" + wardrobe::outfit_path(look.outfit)), "outfit textures exist");
        }
        for (const auto& hat : catalogue.hats()) check(std::filesystem::exists(std::string(OUTLAND_SOURCE_DIR) + "/" + hat.model), "hat models exist");
        check(ids.size() == catalogue.looks().size() && priced, "unique looks; your own clothes are free, the rest cost");

        economy::EconomyState state;
        state.money = 1000;
        const auto& own = catalogue.own_clothes();
        check(wardrobe::wearing(state, catalogue)->id == own && !wardrobe::wearing_hat(state, catalogue), "you start in your own clothes, no hat");
        const auto& look = catalogue.looks()[3];
        const auto message = wardrobe::buy(state, catalogue, look.id, false);
        check(message.starts_with("Bought") && state.money == 1000 - look.price && wardrobe::owns(state, catalogue, look.id) &&
              wardrobe::wearing(state, catalogue)->id == look.id, "buy a look: paid for, yours, on");
        check(wardrobe::buy(state, catalogue, look.id, false).starts_with("You put on") && state.money == 1000 - look.price, "never paid twice");
        check(wardrobe::wear(state, catalogue, own) && state.wearing.empty(), "back into your own clothes");
        check(!wardrobe::wear(state, catalogue, catalogue.looks()[4].id), "you cannot wear what you have not bought");
        const auto money = state.money;
        check(wardrobe::buy(state, catalogue, "look_verda_mano_kapo", false).find("Verda Mano") != std::string::npos && state.money == money,
              "the Green Hand's suit is for members only");
        state.money = 100;
        check(wardrobe::buy(state, catalogue, "look_verda_mano_kapo", true).starts_with("Not enough") && state.money == 100, "too dear: nothing changes");
        state.money = 5000;
        check(wardrobe::buy(state, catalogue, "look_verda_mano_kapo", true).starts_with("Bought") && state.wearing == "look_verda_mano_kapo",
              "a member buys the suit");
        check(wardrobe::buy(state, catalogue, "hat_cowboy", false).starts_with("Bought") && state.hat == "hat_cowboy" &&
              wardrobe::wearing_hat(state, catalogue)->id == "hat_cowboy", "a hat goes with any look");
        wardrobe::take_off_hat(state);
        check(state.hat.empty() && wardrobe::owns(state, catalogue, "hat_cowboy"), "a hat off is still yours");
        check(economy::valid_state(state), "the wardrobe saves");
    }
    if (failures) return 1;
    std::cout << "[PASS] furniture in pubs and some homes; Vestaĵejo Verda and the wardrobe\n";
    return 0;
}
