#include "outland/game/vehicles/VehicleRegistry.hpp"
#include "outland/assets/ModelCache.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <raymath.h>
#include <set>
#include <sstream>
namespace outland::game::vehicles {
std::array<Vector3, 4> VehicleDefinition::anchors() const {
    return {{{-track * .5F, wheel_radius, front_z},
             {track * .5F, wheel_radius, front_z},
             {-track * .5F, wheel_radius, rear_z},
             {track * .5F, wheel_radius, rear_z}}};
}
const VehicleDefinition *VehicleRegistry::find(std::string_view id) const {
    for (const auto &d : definitions_)
        if (d.id == id)
            return &d;
    return nullptr;
}
bool VehicleRegistry::normalize(Model &model, float length) {
    const auto bounds = assets::transformed_model_bounds(model);
    const float size = bounds.max.z - bounds.min.z;
    if (!std::isfinite(size) || size <= .0001F || !std::isfinite(length) || length <= 0)
        return false;
    const float scale = length / size;
    model.transform = MatrixMultiply(model.transform, MatrixScale(scale, scale, scale));
    return true;
}
bool VehicleRegistry::load(const std::string &path, std::string &error) {
    std::ifstream in(path);
    std::string line;
    if (!std::getline(in, line) || !line.starts_with("id\tname\tbody\twheel\t")) {
        error = "Missing vehicle manifest/header";
        return false;
    }
    std::vector<VehicleDefinition> parsed;
    std::set<std::string> ids;
    const auto root =
        std::filesystem::path(path).parent_path().parent_path().parent_path().parent_path();
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        std::vector<std::string> columns;
        std::stringstream split(line);
        std::string value;
        while (std::getline(split, value, '\t'))
            columns.push_back(value);
        if (columns.size() != 30) {
            error = "Vehicle manifest requires 30 columns";
            return false;
        }
        VehicleDefinition d;
        d.id = columns[0];
        d.name = columns[1];
        d.body = columns[2];
        d.wheel = columns[3];
        float *fields[]{
            &d.length,      &d.width,     &d.height,          &d.wheel_radius,  &d.front_z,
            &d.rear_z,      &d.track,     &d.acceleration,    &d.brake,         &d.reverse,
            &d.drag,        &d.top_speed, &d.steer,           &d.health,        &d.engine_health,
            &d.tire_health, &d.fuel,      &d.seat.x,          &d.seat.y,        &d.seat.z,
            &d.exit.x,      &d.exit.z,    &d.camera_distance, &d.camera_height, &d.max_slope,
            &d.body_offset};
        for (std::size_t i = 0; i < 26; ++i) {
            try {
                std::size_t used = 0;
                *fields[i] = std::stof(columns[i + 4], &used);
                if (used != columns[i + 4].size() || !std::isfinite(*fields[i]))
                    throw 1;
            } catch (...) {
                error = "Invalid vehicle tuning";
                return false;
            }
        }
        auto asset = [&](const std::string &p) {
            return p.starts_with("assets/verda/vehicles/") && p.find("..") == std::string::npos &&
                   std::filesystem::is_regular_file(root / p);
        };
        if (d.id.empty() || d.id.size() > 80 ||
            d.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos ||
            !ids.insert(d.id).second || !asset(d.body) || (!d.wheel.empty() && !asset(d.wheel)) ||
            d.length <= 0 || d.width <= 0 || d.height <= 0 || d.wheel_radius <= 0 ||
            d.front_z <= d.rear_z || d.track <= 0 || d.top_speed <= 0 || d.top_speed > 60 ||
            d.acceleration <= 0 || d.brake <= 0 || d.reverse <= 0 || d.drag < 0 || d.steer <= 0 ||
            d.steer > 45 || d.max_slope <= 0 || d.max_slope > 40 || d.health <= 0 ||
            d.engine_health <= 0 || d.tire_health <= 0 || d.fuel < 0 || d.fuel > 1 ||
            d.length > 30 || d.width > 10 || d.height > 10 || d.track > d.width * 1.5F ||
            d.wheel_radius > d.height || d.front_z > d.length * .5F || d.rear_z < -d.length * .5F ||
            d.camera_distance <= 0 || d.camera_height <= 0) {
            error = "Invalid vehicle definition/bounds/path";
            return false;
        }
        parsed.push_back(std::move(d));
        if (parsed.size() > 64) {
            error = "Vehicle definition limit exceeded";
            return false;
        }
    }
    if (parsed.empty()) {
        error = "Empty vehicle catalog";
        return false;
    }
    definitions_ = std::move(parsed);
    error.clear();
    return true;
}
} // namespace outland::game::vehicles
