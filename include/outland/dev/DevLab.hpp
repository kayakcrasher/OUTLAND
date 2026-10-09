#pragma once

#include <raylib.h>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::dev {

enum class DevLocation {
    TrainingGround,
    Espera,
    Forest,
    OpenField,
    BuildingTest,
    VehicleTest,
    ZombieTest,
    PortoLuma,
    SouthHaven,
    Roka
};

class DevLab {
public:
    // One mode owner for Creator input, updates and drawing.
    void begin_builder(){building_=true;tools_open_=false;build_toggle_=false;vehicle_spawn_=false;return_requested_=false;teleport_requested_=false;}
    bool building()const{return building_;}
    void toggle_build(){building_=!building_;}
    void update(int width=0,int height=0,bool travel_shortcuts=true);
    void draw_tools(int width,int height,bool building)const;
    bool owns_point(Vector2 point,int width,int height)const;
    bool tools_open()const{return tools_open_;}
    bool take_build_toggle(){bool value=build_toggle_;build_toggle_=false;return value;}
    bool take_vehicle_spawn(){bool value=vehicle_spawn_;vehicle_spawn_=false;return value;}
    bool take_return(){bool value=return_requested_;return_requested_=false;return value;}

    void draw_overlay(
        Vector3 player_position,
        float yaw,
        float pitch,
        bool grounded
    ) const;

    [[nodiscard]]
    Vector3 spawn_position(const world::VerdaRegion& region) const;

    [[nodiscard]]
    bool teleport_requested() const;

    void clear_teleport();

private:
    bool building_{true};
    bool tools_open_{false},build_toggle_{false},vehicle_spawn_{false},return_requested_{false},had_touch_{false};
    std::vector<int> touch_ids_;
    DevLocation location_{
        DevLocation::TrainingGround
    };

    bool teleport_requested_{
        false
    };
};

} // namespace outland::dev
