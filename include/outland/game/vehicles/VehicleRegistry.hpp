#pragma once
#include <array>
#include <raylib.h>
#include <string>
#include <string_view>
#include <vector>
namespace outland::game::vehicles {
struct VehicleDefinition {
    std::string id, name, body, wheel;
    float length{}, width{}, height{}, wheel_radius{}, front_z{}, rear_z{}, track{};
    float acceleration{}, brake{}, reverse{}, drag{}, top_speed{}, steer{}, health{},
        engine_health{}, tire_health{}, fuel{};
    Vector3 seat{}, exit{};
    float camera_distance{}, camera_height{}, max_slope{}, body_offset{};
    [[nodiscard]] std::array<Vector3, 4> anchors() const;
};
class VehicleRegistry {
  public:
    bool load(const std::string &path, std::string &error);
    const VehicleDefinition *find(std::string_view id) const;
    const std::vector<VehicleDefinition> &definitions() const { return definitions_; }
    static bool normalize(Model &model, float length);

  private:
    std::vector<VehicleDefinition> definitions_;
};
} // namespace outland::game::vehicles
