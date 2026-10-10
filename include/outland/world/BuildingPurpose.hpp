#pragma once
#include <cstdint>
#include <string_view>

namespace outland::world {

// What a building is for, authored in the Creator. Explore's island life uses it to decide who
// lives and works where; Auto keeps the old behaviour (designated from style, name and town).
enum class BuildingPurpose : std::uint8_t {
    Auto, Home, Shop, Pub, Church, Police, Clinic, Office, Garage, Industrial, Dock, Farm, Vacant
};
inline constexpr int building_purpose_count = 13;

constexpr const char* building_purpose_name(BuildingPurpose purpose) {
    switch(purpose) {
        case BuildingPurpose::Home:return "HOME";
        case BuildingPurpose::Shop:return "SHOP";
        case BuildingPurpose::Pub:return "PUB";
        case BuildingPurpose::Church:return "CHURCH";
        case BuildingPurpose::Police:return "POLICE";
        case BuildingPurpose::Clinic:return "CLINIC";
        case BuildingPurpose::Office:return "OFFICE";
        case BuildingPurpose::Garage:return "GARAGE";
        case BuildingPurpose::Industrial:return "INDUSTRY";
        case BuildingPurpose::Dock:return "DOCK";
        case BuildingPurpose::Farm:return "FARM";
        case BuildingPurpose::Vacant:return "VACANT";
        default:return "AUTO";
    }
}

// Cycle order for the builder's USE button. Purpose markers skip Auto (a marker always means something).
constexpr BuildingPurpose next_building_purpose(BuildingPurpose purpose, int direction = 1, bool allow_auto = true) {
    int value = static_cast<int>(purpose);
    for(int i = 0; i < building_purpose_count; ++i) {
        value = (value + (direction < 0 ? building_purpose_count - 1 : 1)) % building_purpose_count;
        if(allow_auto || value != 0) break;
    }
    return static_cast<BuildingPurpose>(value);
}

constexpr bool valid_building_purpose(int value) { return value >= 0 && value < building_purpose_count; }
}
