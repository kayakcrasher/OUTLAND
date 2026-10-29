#pragma once
#include "outland/game/Settings.hpp"
#include "outland/game/hud/Instruments.hpp"
#include "outland/world/Zones.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <optional>
#include <vector>

namespace outland::world { class VerdaRegion; }

namespace outland::game {

// The island map (MAPO): the whole of Verda with its five zones, the roads and towns, and where
// the player stands and faces. It doubles as the pause menu for settings. Modal while open.
// Tapping the map sets a waypoint (tapping it again clears it); the game shows it on the compass
// and as a column of light, along with the places the compass marks.
class MapScreen {
public:
    struct View {
        Vector3 player{};
        float yaw{0};             // radians; the player faces (sin yaw, cos yaw) in x/z
        float circle_radius{-1};  // Battle Royale safe circle, <0 none
        Vector2 circle_center{};  // (x, z)
        float next_radius{-1};    // the circle it will close to (white), <0 none
        Vector2 next_center{};
        // The helicopters' lines across the island and where each is, while flying; `yours` is
        // the one the player rides (drawn brighter).
        struct Flight { Vector3 start{}, end{}, helicopter{}; bool yours{false}; };
        bool flight{false};
        std::vector<Flight> flights;
        std::vector<Vector3> stops;                    // bus stops
        std::vector<std::pair<Vector3, float>> buses;  // buses on the line: where, and heading (degrees)
        std::vector<std::pair<Vector3, Color>> turf;    // Mafio hideouts: where, and the gang's colour
        struct Base { Vector3 at{}; Color color{}; std::string name; bool plain{false}; };   // plain: no " - BAZO"
        std::vector<Base> bases;                        // each gang's main base (its county's main town)
        std::vector<Vector3> cleared;                   // hideouts the Policio broke up
        // Verda's yin and yang (Explore): the law's share of the towns and the player's crew's,
        // drawn as the emblem with a ring showing each share. law < 0: not shown.
        float law{-1}, crime{0};
        Color crime_color{46, 204, 96, 255};
        std::optional<Vector3> home;                    // Stela Turo: the penthouse for sale, or your home
        // The gang map (a gangster's map): every turf in its holder's colour with the hold left,
        // the tags on the walls, and who is biggest.
        struct TurfMark { Vector3 centre{}; float radius{150}; Color color{}; std::string name, holder; float hold{100}; bool yours{false}, ranch{false}; };
        bool gang_map{false};
        std::vector<TurfMark> territory;
        std::vector<std::pair<Vector3, Color>> tags;
        std::vector<std::pair<std::string, Color>> standings;   // biggest first: "La Verda Mano  4 towns, 2 ranches"
        std::string gang_goal;                                    // "Take the island: 3 of 10 turfs" 
        bool home_owned{false};
        // The outbreak (Zombie Survival's story): the island washed red - theirs - but for the
        // clearings (round the player, the hideout, the holdouts found); a label over ground zero.
        bool danger{false};
        std::vector<std::pair<Vector3, float>> clear;
        std::optional<Vector3> ground_zero;
        // The compass's marks (the job, the hospital, stations, shops...), drawn as on the compass.
        std::vector<hud::CompassMark> marks;
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
    static Vector2 to_world(Rectangle area, Vector2 point);   // (x, z)

    // The player's waypoint, set by tapping the map.
    [[nodiscard]] const std::optional<Vector3>& pin() const { return pin_; }
    void set_pin(Vector3 at) { pin_ = at; }
    void clear_pin() { pin_.reset(); }
    // A tap on the map at `point`: on the pin (within `grab` pixels) clears it, elsewhere moves it there.
    void tap_map(Rectangle area, Vector2 point, float grab);

private:
    std::vector<Vector2> clicks(bool focused);
    void bake(const world::VerdaRegion& region);
    bool open_{false}, warm_{true};
    std::unordered_set<int> previous_;
    Texture2D texture_{};
    bool baked_{false};
    std::vector<Vector2> zone_labels_; // zone centroids in world x/z
    std::optional<Vector3> pin_;
};

}
