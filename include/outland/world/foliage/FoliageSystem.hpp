#pragma once

#include <raylib.h>

namespace outland::world::foliage {

class FoliageSystem {
public:
    FoliageSystem();

    void draw(
        const Vector3& camera_position
    ) const;

private:
    static float hash(
        int x,
        int z,
        int salt
    );

    static void draw_grass_clump(
        Vector3 position,
        float height,
        float width,
        Color color
    );

    static void draw_weed(
        Vector3 position,
        float height,
        Color color
    );

    static void draw_shrub(
        Vector3 position,
        float scale,
        Color color
    );

    static void draw_flower(
        Vector3 position,
        float height,
        Color flower_color
    );
};

} // namespace outland::world::foliage
