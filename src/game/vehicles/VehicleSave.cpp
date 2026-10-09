#include "outland/game/vehicles/VehicleSystem.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
namespace outland::game::vehicles {
bool VehicleSystem::save_state(const world::VerdaRegion &region, const std::string &path,
                               std::string &error) const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    const auto temp = path + ".tmp";
    std::ofstream out(temp);
    if (!out) {
        error = "Vehicle state is not writable";
        return false;
    }
    out << std::setprecision(9) << "OUTLAND_VEHICLES 1\n";
    for (const auto &v : vehicles_)
        if (const auto *a = asset(region, v.id)) {
            const auto &p = a->vehicle;
            out << "VEHICLE " << std::quoted(a->id) << ' ' << std::quoted(p.definition) << ' '
                << p.home.x << ' ' << p.home.y << ' ' << p.home.z << ' ' << p.home_yaw << ' '
                << a->size.x << ' ' << a->size.y << ' ' << a->size.z << ' ' << a->position.x << ' '
                << a->position.y << ' ' << a->position.z << ' ' << a->rotation_y << ' ' << p.health
                << ' ' << p.engine << ' ' << p.fuel << ' ' << p.enabled << ' ' << p.destroyed;
            for (float tire : p.tires)
                out << ' ' << tire;
            out << '\n';
        }
    out << "END\n";
    out.flush();
    if (!out) {
        error = "Vehicle save write failed";
        return false;
    }
    out.close();
    fs::rename(temp, path, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    error.clear();
    return true;
}
bool VehicleSystem::load_state(world::VerdaRegion &region, const std::string &path,
                               std::string &error) {
    std::ifstream in(path);
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "OUTLAND_VEHICLES" || version != 1) {
        error = "Invalid vehicle state header";
        return false;
    }
    std::vector<world::WorldAsset> parsed;
    std::set<std::string> ids;
    std::string record;
    bool ended = false;
    while (in >> record) {
        if (record == "END") {
            ended = true;
            break;
        }
        if (record != "VEHICLE" || parsed.size() >= 128) {
            error = "Invalid vehicle state record";
            return false;
        }
        world::WorldAsset a;
        auto &p = a.vehicle;
        int enabled = 0, destroyed = 0;
        if (!(in >> std::quoted(a.id) >> std::quoted(p.definition) >> p.home.x >> p.home.y >>
              p.home.z >> p.home_yaw >> a.size.x >> a.size.y >> a.size.z >> a.position.x >>
              a.position.y >> a.position.z >> a.rotation_y >> p.health >> p.engine >> p.fuel >>
              enabled >> destroyed)) {
            error = "Truncated vehicle state";
            return false;
        }
        for (auto &tire : p.tires)
            if (!(in >> tire) || !std::isfinite(tire) || tire < 0 || tire > 100) {
                error = "Invalid tire state";
                return false;
            }
        const float values[]{p.home.x,     p.home.y, p.home.z,     p.home_yaw,   a.size.x,
                             a.size.y,     a.size.z, a.position.x, a.position.y, a.position.z,
                             a.rotation_y, p.health, p.engine,     p.fuel};
        for (float f : values)
            if (!std::isfinite(f)) {
                error = "Non-finite vehicle state";
                return false;
            }
        if (a.id.empty() || a.id.size() > 512 || !ids.insert(a.id).second ||
            !registry_.find(p.definition) || a.size.x <= 0 || a.size.y <= 0 || a.size.z <= 0 ||
            p.health < 0 || p.health > 100 || p.engine < 0 || p.engine > 100 || p.fuel < 0 ||
            p.fuel > 1 || (enabled != 0 && enabled != 1) || (destroyed != 0 && destroyed != 1)) {
            error = "Invalid vehicle state values";
            return false;
        }
        p.enabled = enabled;
        p.destroyed = destroyed;
        parsed.push_back(a);
    }
    if (!ended || (in >> record)) {
        error = "Vehicle state missing END or has trailing data";
        return false;
    }
    for (const auto &saved : parsed)
        if (auto *target = asset(region, saved.id)) {
            const auto &home = target->vehicle.home;
            const auto &s = saved.vehicle;
            if (target->vehicle.definition != s.definition || home.x != s.home.x ||
                home.y != s.home.y || home.z != s.home.z ||
                target->vehicle.home_yaw != s.home_yaw || target->size.x != saved.size.x ||
                target->size.y != saved.size.y || target->size.z != saved.size.z)
                continue; // Authored edits take precedence over stale gameplay state.
            const auto marker = target->vehicle.marker;
            target->vehicle = s;
            target->vehicle.marker = marker;
            target->position = saved.position;
            target->rotation_y = saved.rotation_y;
        }
    reconcile(region);
    error.clear();
    return true;
}
} // namespace outland::game::vehicles
