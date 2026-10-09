#pragma once
#include "outland/assets/ModelCache.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
namespace outland::game::vehicles {
class VehicleRenderer {
  public:
    void draw(const VehicleSystem &system, const world::VerdaRegion &region, Vector3 camera);
    void camera(Camera3D &camera, const VehicleSystem &system, const world::VerdaRegion &region,
                float dt, float orbit, float pitch, const combat::CombatWorld &collision);
    void reset_camera() { following_ = false; }

  private:
    assets::ModelCache models_{128};
    bool following_{false};
    Vector3 follow_position_{}, follow_target_{};
};
} // namespace outland::game::vehicles
