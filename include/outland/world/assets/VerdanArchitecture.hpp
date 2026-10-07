#pragma once

#include <raylib.h>

namespace outland::world::assets {

struct HouseStyle {
    Color plaster;
    Color trim;
    Color roof;
    Color foundation;
    Color glass;

    bool upper_floor{false};
    bool balcony{false};
    bool weathered{true};
};

class VerdanArchitecture {
public:
    static void draw_house(
        Vector3 ground_position,
        Vector3 size,
        const HouseStyle& style,
        bool detailed = true
    );

private:
    static void draw_window(
        Vector3 position,
        float width,
        float height,
        const HouseStyle& style
    );

    static void draw_door(
        Vector3 position,
        const HouseStyle& style
    );

    static void draw_balcony(
        Vector3 position,
        float width
    );

    static void draw_roof(
        Vector3 position,
        Vector3 building_size,
        const HouseStyle& style
    );
};

} // namespace outland::world::assets
