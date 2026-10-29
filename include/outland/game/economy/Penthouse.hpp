#pragma once
#include "outland/game/economy/Economy.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raylib.h>
#include <cstdint>
#include <string>

// The penthouse at the top of Stela Turo, Verda City's art deco skyscraper: in Explore it is for
// sale. Walk up the stairs to the 17th floor and buy it (E / USE) for `price` kronoj; then it is
// home: sleep in its bed till morning (healed). Ownership is kept with the Explore wallet (EconomyState::properties).
namespace outland::game::economy {

class Penthouse {
public:
    static constexpr const char* model = "assets/verda/city/towers/stela_turo.glb";
    static constexpr const char* property_id = "stela_turo_penthouse";
    static constexpr std::int64_t price = 5000;
    // In the model (tools/generate_city_kit.py stela_turo): the penthouse floor's top, the area
    // inside its walls, the bed and where you stand when you wake up.
    static constexpr float floor_y = 16 * 3.6F + .25F;
    static constexpr float half_x = 10.15F, half_z = 8.15F;
    static constexpr Vector3 bed{-4.5F, floor_y, 5.5F};
    static constexpr Vector3 wake{-4.5F, floor_y, 3.4F};
    static constexpr Vector3 sale{-1.0F, floor_y, -2.0F};

    // Finds the tower in the world (false when it is not built).
    bool locate(const world::VerdaRegion& region);
    [[nodiscard]] bool found() const { return found_; }
    [[nodiscard]] Vector3 to_world(Vector3 local) const;
    [[nodiscard]] Vector3 to_local(Vector3 world) const;
    // Feet on the penthouse floor, inside its walls.
    [[nodiscard]] bool inside(Vector3 feet) const;
    [[nodiscard]] bool near_bed(Vector3 feet, float reach = 2.2F) const;

    [[nodiscard]] static bool owned(const EconomyState& wallet);
    // Pays and takes the keys; returns what to tell the player.
    static std::string buy(EconomyState& wallet);

private:
    bool found_{false};
    Vector3 origin_{};
    float yaw_{0};
};

} // namespace outland::game::economy
