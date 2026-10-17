#include "outland/game/interiors/Furnishing.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace outland::game::interiors {
namespace {
const std::string dir = "assets/verda/furniture/";
constexpr float storey_height = 3.6F, slab = .2F, wall = .3F;
constexpr float house_floor = .44F;   // the detached houses' foundation

std::uint64_t hash(const std::string& text) {
    std::uint64_t h = 1469598103934665603ULL;
    for (const char c : text) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
    // Mixed, so neighbours numbered in a row are not furnished in a pattern.
    h ^= h >> 30; h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27; h *= 0x94d049bb133111ebULL;
    return h ^ (h >> 31);
}

// Places in a building's own frame: x across the front, z from the front (-) to the back (+).
struct Frame {
    Vector3 centre;
    float yaw, y;
    Vector3 at(float x, float z) const {
        const float a = yaw * DEG2RAD;
        return {centre.x + x * std::cos(a) + z * std::sin(a), y, centre.z - x * std::sin(a) + z * std::cos(a)};
    }
};

// A Main Street front: the doors are alternate bays (3 m each); the middle of the window bay
// nearest the -X end.
float window_bay(const life::Place& place) {
    const int bays = std::max(1, static_cast<int>(std::lround(place.building_size.x / 3.0F)));
    const int first = (bays / 2) % 2 == 0 ? 1 : 0;
    return -place.building_size.x * .5F + (static_cast<float>(first) + .5F) * place.building_size.x / static_cast<float>(bays);
}

struct Layout {
    Frame frame;
    std::vector<Piece>& out;
    // `turn`: 0 back to the +Z wall (facing the front), 180 back to the front, -90 back to the
    // -X wall (facing +X), 90 back to the +X wall.
    void put(const char* model, float x, float z, float turn = 0, bool solid = true) const {
        out.push_back({dir + model, frame.at(x, z), frame.yaw + turn, solid});
    }
};
}

Vector3 footprint(const std::string& model) {
    struct Size { const char* file; float w, h, d; };
    static constexpr Size sizes[]{
        {"bar_counter.glb", 5.1F, 2.6F, 2.6F}, {"bar_stool.glb", .4F, .8F, .4F}, {"pub_table.glb", 1.8F, .95F, 1.8F},
        {"pub_booth.glb", 1.6F, 1.3F, 2.2F}, {"pool_table.glb", 2.6F, .87F, 1.5F}, {"jukebox.glb", .9F, 1.6F, .6F},
        {"dartboard.glb", 1.45F, 2.2F, .12F}, {"sofa_green.glb", 2.2F, .9F, .9F}, {"sofa_blue.glb", 2.2F, .9F, .9F},
        {"coffee_table.glb", 1.2F, .42F, .7F}, {"rug.glb", 2.6F, .02F, 1.8F}, {"tv_stand.glb", 1.6F, 1.3F, .45F},
        {"bed_double.glb", 2.8F, 1.1F, 2.2F}, {"kitchen_run.glb", 3.2F, 2.2F, .65F}, {"dining_set.glb", 1.8F, .95F, 1.9F},
        {"wardrobe.glb", 1.2F, 2.0F, .6F}, {"bookshelf.glb", 1.0F, 1.9F, .35F}, {"floor_lamp.glb", .45F, 1.65F, .45F},
        {"house_plant.glb", .4F, 1.1F, .4F}, {"clothes_rack.glb", 1.6F, 1.55F, .5F}, {"clothes_table.glb", 1.4F, .96F, .8F},
        {"mannequin_plinth.glb", .8F, .15F, .8F}, {"fitting_rooms.glb", 3.7F, 2.4F, 1.6F}, {"shop_counter.glb", 2.5F, 1.45F, .8F},
        {"hat_stand.glb", .9F, 1.6F, .5F}, {"store_mirror.glb", .9F, 2.0F, .3F}};
    for (const auto& size : sizes)
        if (model.size() >= std::char_traits<char>::length(size.file) && model.ends_with(size.file)) return {size.w, size.h, size.d};
    return {1, 1, 1};
}

Shell shell_of(const life::Place& place) {
    if (!place.indoor || place.sealed) return Shell::None;
    if (place.building_id.find("main_street") != std::string::npos && place.building_size.z >= 19) return Shell::MainStreet;
    if (place.building_id.find("okcidento_house") != std::string::npos && place.building_size.x >= 9 && place.building_size.z >= 8) return Shell::House;
    return Shell::None;
}

int storeys_of(const life::Place& place) {
    return std::max(1, static_cast<int>(std::floor((place.building_size.y - .9F) / storey_height + .5F)));
}

float floor_height(const life::Place& place, int storey) {
    return shell_of(place) == Shell::House ? house_floor : static_cast<float>(storey) * storey_height + slab;
}

bool furnished_home(const life::Place& place) {
    return place.kind == life::PlaceKind::Home && shell_of(place) != Shell::None && hash(place.building_id) % 4 == 1;
}

bool inside(const life::Place& place, Vector3 at, float margin) {
    const float a = place.building_yaw * DEG2RAD, dx = at.x - place.interior.x, dz = at.z - place.interior.z;
    const float x = dx * std::cos(a) - dz * std::sin(a), z = dx * std::sin(a) + dz * std::cos(a);
    return std::abs(x) < place.building_size.x * .5F - margin && std::abs(z) < place.building_size.z * .5F - margin;
}

// A pub on a Main Street ground floor: the bar along the back wall (clear of the back door at
// x = -w + 1.2 .. 2.6), stools in front; tables down the -X wall with the dartboard between;
// a booth and the jukebox on the +X wall behind the stairs; a pool table (or another table) in
// the back half. The front, the doors and the middle stay open.
std::vector<Piece> furnish_pub(const life::Place& place, float base) {
    std::vector<Piece> out;
    if (shell_of(place) != Shell::MainStreet) return out;
    const Layout l{{place.interior, place.building_yaw, base + floor_height(place, 0)}, out};
    const float w = place.building_size.x * .5F, d = place.building_size.z * .5F;
    const bool wide = w > 7;
    const float bar = -w + 2.9F + 2.55F + (wide ? 1.5F : 0), bar_z = d - wall - 1.3F;
    l.put("bar_counter.glb", bar, bar_z);
    for (int i = 0; i < 4; ++i) l.put("bar_stool.glb", bar - 1.8F + 1.2F * static_cast<float>(i), bar_z - 1.75F);
    for (const float z : {4.6F, 1.2F}) l.put("pub_table.glb", -w + wall + 1.3F, z);
    l.put("dartboard.glb", -w + wall + .07F, -2.6F, -90, false);
    l.put("pub_booth.glb", w - wall - .85F, 5.3F);
    l.put("jukebox.glb", w - wall - .35F, 1.6F, 90);
    if (wide) l.put("pool_table.glb", 1.4F, 3.6F, 90);
    else l.put("pub_table.glb", 1.4F, 3.9F);
    l.put("house_plant.glb", window_bay(place), -d + wall + .7F);
    return out;
}

// A home: a Main Street ground-floor flat or a detached house's one open room.
std::vector<Piece> furnish_home(const life::Place& place, float base) {
    std::vector<Piece> out;
    const auto shell = shell_of(place);
    if (shell == Shell::None) return out;
    const Layout l{{place.interior, place.building_yaw, base + floor_height(place, 0)}, out};
    const bool blue = (hash(place.building_id) >> 8) % 2;
    const char* sofa = blue ? "sofa_blue.glb" : "sofa_green.glb";
    if (shell == Shell::MainStreet) {
        const float w = place.building_size.x * .5F, d = place.building_size.z * .5F;
        // The kitchen on the back wall (the back door is at the -X end), the table in front of it.
        l.put("kitchen_run.glb", 1.5F, d - wall - .33F);
        l.put("dining_set.glb", 1.5F, d - wall - 3.0F);
        // Sitting room on the -X wall, the television facing the sofa across the rug.
        l.put(sofa, -w + wall + .45F, -.6F, -90);
        l.put("rug.glb", -w + wall + 2.0F, -.6F, 90, false);
        l.put("coffee_table.glb", -w + wall + 1.75F, -.6F, 90);
        l.put("tv_stand.glb", -w + wall + 3.5F, -.6F, 90);
        l.put("floor_lamp.glb", -w + wall + .35F, -2.2F);
        // The bed further back on the -X wall, the wardrobe by it.
        l.put("bed_double.glb", -w + wall + 1.1F, 4.6F, -90);
        l.put("wardrobe.glb", -w + wall + .3F, 7.6F, -90);
        l.put("bookshelf.glb", w - wall - .17F, 3.4F, 90);
        l.put("house_plant.glb", window_bay(place), -d + wall + .7F);
    } else {
        const float w = place.building_size.x * .5F - .35F, d = place.building_size.z * .5F - .35F;
        // Kitchen and table at the back left, the bed at the back right, the sitting room at the
        // front left; the door (middle of the front) and the middle of the room stay clear.
        l.put("kitchen_run.glb", -w + 1.7F, d - .33F);
        l.put("dining_set.glb", -w + 1.8F, d - 2.3F);
        l.put("bed_double.glb", w - 1.1F, d - 1.6F, 90);
        l.put(sofa, -w + .45F, -d + 2.2F, -90);
        l.put("rug.glb", -w + 2.1F, -d + 2.2F, 90, false);
        l.put("coffee_table.glb", -w + 1.85F, -d + 2.2F, 90);
        l.put("tv_stand.glb", -w + 3.6F, -d + 2.2F, 90);
        l.put("wardrobe.glb", w - .3F, -d + 1.6F, 90);
        l.put("bookshelf.glb", .5F, d - .17F);
        l.put("house_plant.glb", -1.3F, -d + .4F);
    }
    return out;
}

std::vector<Piece> furnish(const std::vector<life::Place>& places, float (*ground)(float, float), const std::string& skip) {
    std::vector<Piece> out;
    for (const auto& place : places) {
        if (place.id == skip || shell_of(place) == Shell::None) continue;
        const float base = shell_of(place) == Shell::House && ground ? ground(place.interior.x, place.interior.z) : place.interior.y;
        std::vector<Piece> pieces;
        if (place.kind == life::PlaceKind::Pub) pieces = furnish_pub(place, base);
        else if (furnished_home(place)) pieces = furnish_home(place, base);
        out.insert(out.end(), pieces.begin(), pieces.end());
    }
    return out;
}

const life::Place* store_site(const std::vector<life::Place>& places, int capital, Vector3 centre) {
    const life::Place* best = nullptr;
    float score = 1e30F;
    for (const auto& place : places) {
        if (place.settlement != capital || shell_of(place) != Shell::MainStreet || place.kind == life::PlaceKind::Pub) continue;
        const float distance = std::hypot(place.interior.x - centre.x, place.interior.z - centre.z);
        // Three floors 18 m wide hold every look; a smaller building comes after any of those.
        const float s = distance + (storeys_of(place) >= 3 && place.building_size.x >= 17 ? 0 : 10000);
        if (s < score) {score = s; best = &place;}
    }
    return best;
}

// Vestaĵejo Verda: the ground floor has the window display (a mannequin behind each window bay),
// a row of mannequins down the -X wall, the counter at the back, the fitting rooms and a mirror
// at the back right, the hat stand, racks and tables in the middle; the floors above are
// galleries of mannequins between racks, clear of the stairs on the +X side.
ClothingStore clothing_store(const life::Place& place, int looks) {
    ClothingStore store;
    store.place_id = place.id;
    store.yaw = place.building_yaw;
    store.width = place.building_size.x;
    store.storeys = storeys_of(place);
    const float w = place.building_size.x * .5F, d = place.building_size.z * .5F;
    const float base = place.interior.y;
    const Frame ground{place.interior, place.building_yaw, base + floor_height(place, 0)};
    const Layout l{ground, store.pieces};
    store.centre = ground.at(0, 0);
    store.front = ground.at(1.5F, -d - 1.6F);
    store.sign = Frame{place.interior, place.building_yaw, base + 3.27F}.at(0, -d - .13F);
    const auto mannequin = [&](int storey, float x, float z, float turn) {
        if (static_cast<int>(store.mannequins.size()) >= looks) return;
        const Frame f{place.interior, place.building_yaw, base + floor_height(place, storey)};
        store.pieces.push_back({dir + "mannequin_plinth.glb", f.at(x, z), f.yaw + turn, true});
        Vector3 top = f.at(x, z);
        top.y += .15F;
        store.mannequins.push_back({top, f.yaw + turn, storey});   // a figure's yaw: it faces (sin, cos)
    };
    // The window display: every bay that is not a door (the doors are alternate bays).
    const int bays = static_cast<int>(std::lround(place.building_size.x / 3.0F));
    for (int i = 0; i < bays; ++i) {
        if (i % 2 == (bays / 2) % 2) continue;
        mannequin(0, -w + (static_cast<float>(i) + .5F) * place.building_size.x / static_cast<float>(bays), -d + 1.3F, 180);
    }
    for (float z = -4.6F; z <= 6.0F; z += 1.75F) mannequin(0, -w + wall + .7F, z, 90);   // facing into the shop
    // The counter, the fitting rooms and the mirror at the back; the hat stand on the +X wall.
    l.put("shop_counter.glb", -w + 3.6F, d - 2.6F);
    store.counter = ground.at(-w + 3.6F, d - 3.6F);
    l.put("fitting_rooms.glb", w - wall - 2.0F, d - wall - .8F);
    store.fitting = ground.at(w - wall - 2.0F, d - 2.6F);
    l.put("store_mirror.glb", w - wall - .2F, 3.0F, 90);
    l.put("hat_stand.glb", w - wall - .45F, 0.8F, 90);
    store.hat_yaw = ground.yaw - 90;   // the hats' fronts (+Z) toward the shop
    for (const float x : {-.25F, .25F}) {
        // The stand's heads, turned with it (its local x becomes the wall's z).
        Vector3 head = ground.at(w - wall - .45F, .8F - x);
        head.y += 1.53F;
        store.hats.push_back(head);
    }
    for (const float x : {-2.4F, 1.6F}) {
        l.put("clothes_rack.glb", x, -2.6F);
        l.put("clothes_rack.glb", x, 4.4F);
        l.put("clothes_table.glb", x, 1.0F + (x > 0 ? 1.4F : 0));
    }
    l.put("house_plant.glb", -w + wall + .4F, d - wall - .4F);
    // Upstairs: rows of mannequins, racks between, clear of the stairs (x < w - 3.3).
    for (int s = 1; s < store.storeys; ++s) {
        const Frame f{place.interior, place.building_yaw, base + floor_height(place, s)};
        const Layout up{f, store.pieces};
        const float right = w - 3.6F;
        const int columns = std::max(1, static_cast<int>((right - (-w + wall + .8F)) / 2.6F) + 1);
        for (int c = 0; c < columns; ++c)
            for (const float z : {-7.0F, -3.5F, 0.0F, 3.5F, 7.0F})
                mannequin(s, -w + wall + .8F + 2.6F * static_cast<float>(c), z, 180);
        for (const float z : {-5.25F, 5.25F}) up.put("clothes_rack.glb", -w + 3.5F, z);
        up.put("store_mirror.glb", -w + wall + .2F, 1.75F, -90);
    }
    return store;
}

} // namespace outland::game::interiors
