#pragma once
#include "outland/world/Zones.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <vector>

namespace outland::world { class VerdaRegion; }

namespace outland::game {

// Player settings, kept beside the saves (profiles/settings.cfg).
struct GameSettings {
    enum class Graphics : int { Auto, Fast, Sharp };
    float look_sensitivity{2.25F};
    float volume{.8F};
    bool muted{false};
    Graphics graphics{Graphics::Auto};
    static constexpr float min_sensitivity = .75F, max_sensitivity = 5.0F;
    static std::string path_for(const std::string& map_path);
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

// The island map (MAPO): the whole of Verda with its five zones, the roads and towns, and where
// the player stands and faces. It doubles as the pause menu for settings. Modal while open.
class MapScreen {
public:
    struct View {
        Vector3 player{};
        float yaw{0};             // radians; the player faces (sin yaw, cos yaw) in x/z
        float circle_radius{-1};  // Battle Royale safe circle round the island centre, <0 none
        std::vector<Vector3> stops;                    // bus stops
        std::vector<std::pair<Vector3, float>> buses;  // buses on the line: where, and heading (degrees)
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
