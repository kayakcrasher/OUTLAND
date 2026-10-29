#pragma once
#include "outland/game/life/LifeTypes.hpp"
#include <raylib.h>
#include <string>
#include <vector>

// Furniture inside Verda's buildings (models from tools/generate_furniture.py). Every pub is
// furnished - a bar with stools and a back bar of bottles, tables, a booth, a jukebox, a
// dartboard and, in the wide ones, a pool table - and every home somebody lives in (a kitchen,
// a dining table, a sofa and the television, a bed, a wardrobe; houses laid out one way or its
// mirror, some with a lamp or a plant more, by the building). Homes nobody lives in (for sale)
// stand empty.
//
// Two shells are known: the Main Street rows (open floors, stairs along the +X wall, the front
// doors in alternate bays and a back door) and the detached houses (one open room, the door in
// the middle of the front). Buildings put together in the Creator have no known shell and stay
// as they are. Pieces keep the middle of the room clear: residents walk to the centre.
//
// Vestaĵejo Verda, the clothing store, takes a Main Street building in Verda City
// (clothing_store): every look on a mannequin, over three floors, a counter, fitting rooms, a
// hat stand, racks and tables of clothes.
namespace outland::game::interiors {

struct Piece {
    std::string model;     // assets/verda/furniture/*.glb
    Vector3 at{};          // world, on the floor
    float yaw{0};          // degrees (the model's front, -Z, turned by this)
    bool solid{true};      // collides (a rug does not)
};

// A piece's size (width x height x depth, metres), as tools/generate_furniture.py builds it.
Vector3 footprint(const std::string& model);

enum class Shell : std::uint8_t { None, MainStreet, House };
Shell shell_of(const life::Place& place);
int storeys_of(const life::Place& place);   // a Main Street building's floors (from its height)
// Floor heights above the place's base (`interior.y`; a house's base is the ground).
float floor_height(const life::Place& place, int storey);

// A home with a known shell (the ones furnished when somebody lives there); every pub.
bool furnished_home(const life::Place& place);
// Every furnished place's pieces, `ground(x, z)` giving a house's ground height. The store's
// building (`skip`) is left for clothing_store. `lived_in` (by place index): a home is furnished
// only if somebody lives there; without it every home is.
std::vector<Piece> furnish(const std::vector<life::Place>& places, float (*ground)(float, float), const std::string& skip = {},
                           const std::vector<bool>* lived_in = nullptr);
std::vector<Piece> furnish_pub(const life::Place& place, float base);
std::vector<Piece> furnish_home(const life::Place& place, float base);

// Vestaĵejo Verda.
struct Mannequin { Vector3 at{}; float yaw{0}; int storey{0}; };  // `at` on top of its plinth
struct ClothingStore {
    std::string place_id;
    Vector3 centre{}, front{}, sign{};   // centre on the ground floor; the sign's centre over the door
    float yaw{0}, width{0};
    int storeys{1};
    Vector3 counter{};                   // where you stand to buy (in front of the till)
    Vector3 fitting{};                   // in front of the fitting rooms
    std::vector<Mannequin> mannequins;   // window display first, then the ground floor, then upstairs
    std::vector<Vector3> hats;           // where the hat stand's hats sit
    float hat_yaw{0};
    std::vector<Piece> pieces;
};
// The store's building: the Main Street building in the capital (settlement `capital`) nearest
// its centre `centre`, three storeys and 18 m wide if there is one, not a pub. Null if none.
const life::Place* store_site(const std::vector<life::Place>& places, int capital, Vector3 centre);
// `looks`: how many to put on show (plinths only for those).
ClothingStore clothing_store(const life::Place& place, int looks = 1000);
// Inside the building's footprint (any floor), with a margin.
bool inside(const life::Place& place, Vector3 at, float margin = 0);

} // namespace outland::game::interiors
