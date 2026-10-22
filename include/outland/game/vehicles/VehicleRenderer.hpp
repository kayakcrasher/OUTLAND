#pragma once
#include <array>
#include <memory>
#include "outland/assets/ModelCache.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
namespace outland::game::vehicles {
class VehicleRenderer {
  public:
    void draw(const VehicleSystem &system, const world::VerdaRegion &region, Vector3 camera);
    void camera(Camera3D &camera, const VehicleSystem &system, const world::VerdaRegion &region,
                float dt, float orbit, float pitch, const combat::CombatWorld &collision,
                std::string_view follow = {});   // `follow`: a car the player rides in (not drives)
    void reset_camera() { following_ = false; }
    // A trailer hitched behind the followed vehicle reaches this far behind its centre (metres; 0:
    // none): the camera rides higher and further back, over the trailer, so the road ahead shows.
    void set_trailer(float reach) { trailer_ = reach > 0 ? reach : 0; }

  private:
    assets::ModelCache models_{128};
    // Repainted bodies (game::autos paints): one cache per paint, each body recoloured once.
    std::array<std::unique_ptr<assets::ModelCache>, 10> painted_{};
    bool following_{false};
    float trailer_{0};
    Vector3 follow_position_{}, follow_target_{};
};
} // namespace outland::game::vehicles
