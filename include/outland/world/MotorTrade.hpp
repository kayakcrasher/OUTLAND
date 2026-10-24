#pragma once
#include <raylib.h>
#include <array>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }

// Where the motor trade's buildings keep their working parts (game::autos): Marnafto's pumps,
// the Meĥanikejo bays, Aŭtovendejo Verda's display spots and sales desk. The numbers are the
// city kit's (tools/generate_city_kit.py: gas_station(), mechanic_shop(), car_lot()), in each
// model's own frame: x across, z from the front (-z, the street) to the back.
namespace outland::world::motor {

inline constexpr const char* gas_model = "assets/verda/city/civic/gas_station.glb";
inline constexpr const char* mechanic_model = "assets/verda/city/civic/mechanic_shop.glb";
inline constexpr const char* car_lot_model = "assets/verda/city/civic/car_lot.glb";
inline constexpr const char* oil_rig_model = "assets/verda/city/civic/oil_rig.glb";
inline constexpr const char* depot_model = "assets/verda/city/civic/truck_depot.glb";   // Verda Transporto's shed: two drive-in service bays

inline constexpr std::array<float, 2> gas_islands{-3.5F, 3.5F};    // the pump islands' x
inline constexpr float gas_pumps_z0 = -6.2F, gas_pumps_z1 = -1.8F; // ...their length
inline constexpr std::array<float, 2> mechanic_bays{-5.0F, 1.0F};  // the bays' x
inline constexpr float mechanic_bay_half = 2.3F, mechanic_bay_z0 = -5.6F, mechanic_bay_z1 = 5.4F;
inline constexpr std::array<float, 2> car_lot_rows{-6.0F, 3.0F};
inline constexpr std::array<float, 5> car_lot_spots{-14.0F, -8.0F, -2.0F, 4.0F, 10.0F};
inline constexpr Vector3 car_lot_desk{13.6F, 0, 7.6F};              // stand here to buy (in front of the desk)
inline constexpr Vector3 car_lot_delivery{15.6F, 0, -4.0F};         // a bought car waits here, nose to the street
inline constexpr std::array<float, 2> depot_bays{-12.0F, 12.0F};    // the depot's service bays' x (behind its two doorways)
inline constexpr float depot_bay_half = 3.6F, depot_bay_z0 = -10.6F, depot_bay_z1 = 10.6F;

enum class Kind { Gas, Mechanic, CarLot, Depot };
struct Site {
    Kind kind{Kind::Gas};
    std::string asset;     // the building's world asset id
    Vector3 origin{};      // the model's own origin on the ground
    float yaw{0};          // degrees
    [[nodiscard]] Vector3 to_world(Vector3 local) const;
    [[nodiscard]] Vector3 to_local(Vector3 world) const;
};
// Every gas station, workshop and car lot in the region (found by model).
[[nodiscard]] std::vector<Site> sites(const VerdaRegion& region);

// A car (its centre) beside one of the station's pumps.
[[nodiscard]] bool at_pump(const Site& gas, Vector3 car);
// The workshop bay (or the depot's service bay) a car (its centre) stands in, or -1.
[[nodiscard]] int bay_at(const Site& shop, Vector3 car);
// Inside the depot's shed (its walls' footprint): a rig drives in only without its trailer.
inline constexpr float depot_half_x = 18.4F, depot_half_z = 11.4F;
[[nodiscard]] bool in_depot(const Site& depot, Vector3 p);

} // namespace outland::world::motor
