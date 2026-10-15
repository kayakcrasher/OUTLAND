#pragma once
#include "outland/game/Settings.hpp"
#include "outland/world/Zones.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <vector>

namespace outland::world { class VerdaRegion; }

namespace outland::game {

// The island map (MAPO): the whole of Verda with its five zones, the roads and towns, and where
// the player stands and faces. It doubles as the pause menu for settings. Modal while open.
class MapScreen {
public:
    struct View {
        Vector3 player{};
        float yaw{0};             // radians; the player faces (sin yaw, cos yaw) in x/z
        float circle_radius{-1};  // Battle Royale safe circle, <0 none
        Vector2 circle_center{};  // (x, z)
        float next_radius{-1};    // the circle it will close to (white), <0 none
        Vector2 next_center{};
        // The helicopters' line across the island (start, end) and where they are, while flying.
        bool flight{false};
        Vector3 flight_start{}, flight_end{};
        std::vector<Vector3> helicopters;
        std::vector<Vector3> stops;                    // bus stops
        std::vector<std::pair<Vector3, float>> buses;  // buses on the line: where, and heading (degrees)
        std::vector<std::pair<Vector3, Color>> turf;    // Mafio hideouts: where, and the gang's colour
        const Font* font{nullptr};
    };
    enum class Change { None, Settings, Close };

    bool is_open() const { return open_; }
    void open() { open_ = true; warm_ = true; }
    void close() { open_ = false; }
    // The top-bar MAP button, for opening it during play.
    static Rectangle map_button(int width, int height);
    bool tapped_map_button(int width, int height, bool focused);
    // While open: taps on its buttons; keys M / Escape close it. Returns what changed.
    Change update(GameSettings& settings, int width, int height, bool focused);
    void draw(const world::VerdaRegion& region, const View& view, const GameSettings& settings, int width, int height);
    void unload();

    // Layout (screen space), shared by drawing and hit testing.
    static Rectangle map_area(int width, int height);
    static Rectangle settings_button(int index, int width, int height);
    static Rectangle close_button(int width, int height);
    // World <-> map image: the island spans +-extent metres; north (-z) is up.
    static constexpr float extent = 2450;
    static Vector2 to_map(Rectangle area, float x, float z);

private:
    std::vector<Vector2> clicks(bool focused);
    void bake(const world::VerdaRegion& region);
    bool open_{false}, warm_{true};
    std::unordered_set<int> previous_;
    Texture2D texture_{};
    bool baked_{false};
    std::vector<Vector2> zone_labels_; // zone centroids in world x/z
};

}
