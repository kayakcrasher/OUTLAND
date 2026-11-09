#pragma once
#include <algorithm>
#include <cmath>
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
    // The size a vehicle is put in the world at (times the manifest's metres): the cars a touch
    // bigger than their models, so a grown man sits in one with his head under the roof; pickups
    // (already tall in the cab, and as long as the garage's ramps allow), vans, buses, rigs and
    // ambulances as they are.
    [[nodiscard]] float world_scale() const { return two_wheeled() ? 1.0F : height < 2.3F && length < 5.4F ? 1.08F : 1.0F; }
    // A motorcycle: its wheels one behind the other (no track). Ridden astride, out in the open; it
    // leans into its turns and carries one on the pillion.
    [[nodiscard]] bool two_wheeled() const { return track < .3F; }
};
// How far a two-wheeler leans into its turn (degrees about its length, + to its right): more the
// harder it turns and the faster it goes (0 for anything with four wheels).
[[nodiscard]] inline float lean_degrees(const VehicleDefinition &d, float steering, float speed) {
    if (!d.two_wheeled() || d.steer <= 0) return 0;
    const float pace = std::min(1.0F, std::abs(speed) / 14.0F);
    return -steering / d.steer * pace * 30.0F;
}
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
