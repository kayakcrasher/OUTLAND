#pragma once
#include "outland/game/build/PlayerBuilds.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <vector>

namespace outland::world { class VerdaRegion; }

// Explore's BUILD mode: a ghost of the chosen piece follows the ground where the camera looks;
// PLACE (F or Enter) builds it, TURN (R) rotates it, REMOVE (Backspace) takes away the nearest
// piece you built, and < > (Tab, 1-6) change the piece. Walking and looking work as usual;
// the combat buttons are hidden. K or BUILD opens and closes it.
namespace outland::game::build {

class BuildMode {
public:
    struct Result {
        bool changed{false};        // pieces were added or removed: re-apply to the region
        bool transit{false};        // a road or bus stop changed: the bus line needs a rebuild
    };
    [[nodiscard]] bool active() const { return active_; }
    void set_active(bool active);
    // Mouse look: the left button places, the right removes, the wheel picks the piece.
    void set_mouse_aim(bool on) { mouse_aim_ = on; }
    static Rectangle toggle_button(int width, int height);
    // Screen points the bar owns while building (so the sticks ignore them).
    [[nodiscard]] bool owns_point(Vector2 point, int width, int height) const;
    // True when BUILD was tapped (or K pressed) this frame.
    bool toggled(int width, int height, bool focused);
    // While active: reads the bar and keys, moves the ghost to where `camera`+`look` meets the
    // ground (within reach of `feet`), and builds or removes.
    Result update(world::VerdaRegion& region, PlayerBuilds& builds, Vector3 camera, Vector3 look, Vector3 feet,
                  int width, int height, bool focused);
    void draw_world(float time_seconds) const;
    void draw_hud(const PlayerBuilds& builds, int width, int height) const;
    // A message shown for a few seconds whether or not the bar is open ("The Kovac family moved in").
    void notify(std::string text, float seconds = 5);
    void tick(float dt) { if (note_time_ > 0) note_time_ -= dt; }
    [[nodiscard]] Piece tool() const { return tool_; }
private:
    enum class Button { Previous, Next, Turn, Place, Remove, Done, Count };
    static Rectangle button(Button which, int width, int height);
    std::vector<Vector2> clicks(bool focused);
    Vector3 aim_ground(Vector3 camera, Vector3 look, Vector3 feet) const;
    Vector3 snap_road_end(const world::VerdaRegion& region, Vector3 p) const;
    bool active_{false}, mouse_aim_{false};
    Piece tool_{Piece::Cottage};
    float yaw_{0};
    bool turned_{false};          // the player turned it: stop facing them automatically
    bool road_started_{false};
    Vector3 road_start_{};
    Placed ghost_{};
    bool ghost_valid_{false};
    std::string why_;
    std::string note_;
    float note_time_{0};
    std::unordered_set<int> previous_touches_;
    bool warm_{true};
};

} // namespace outland::game::build
