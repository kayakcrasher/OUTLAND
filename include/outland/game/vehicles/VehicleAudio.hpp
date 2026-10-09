#pragma once
#include "outland/game/vehicles/VehicleSystem.hpp"
#include <array>
namespace outland::game::vehicles {
class VehicleAudio {
  public:
    ~VehicleAudio();
    void update(const VehicleSystem &system, const world::VerdaRegion &region, float dt,
                bool paused, float volume = 1);
    void stop();

  private:
    bool initialized_{false}, looping_{false}, parking_{false};
    float volume_{1};
    Music engine_{};
    std::array<Sound, 7> sounds_{};
    float door_timer_{0}, last_speed_{0}, acceleration_cooldown_{0};
    void initialize();
    void play(std::size_t index);
};
} // namespace outland::game::vehicles
