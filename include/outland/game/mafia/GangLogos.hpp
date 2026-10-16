#pragma once
#include "outland/game/mafia/Territory.hpp"
#include <raylib.h>
#include <array>

// The gangs' tags, as sprayed on Verda's walls: each gang's emblem and initials in its colour,
// black-outlined, with overspray and drips - La Verda Mano an open hand, Montaj Lupoj a wolf's
// head, La Ora Ankro an anchor, Marĉaj Ranoj a frog, Feraj Hundoj a spiked collar.
namespace outland::game::mafia {

class GangLogos {
public:
    static constexpr int width = 256, height = 128;
    static constexpr float wall_width = 1.9F, wall_height = .95F;   // metres on the wall

    ~GangLogos() { unload(); }
    // Paint the five logos (needs a GL context, outside BeginDrawing / BeginMode3D).
    void prepare();
    void unload();
    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] const Texture2D& texture(int gang) const { return textures_[static_cast<std::size_t>(gang)].texture; }
    // A tag on its wall (inside BeginMode3D).
    void draw(const Tag& tag, Vector3 camera) const;
    // The logo flat on screen (HUD, map legend).
    void draw_flat(int gang, Rectangle where) const;

private:
    std::array<RenderTexture2D, gang_count> textures_{};
    bool ready_{false};
};

} // namespace outland::game::mafia
