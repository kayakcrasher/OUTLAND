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

    // La Verda Mano's gable mural, painted the way Ulster's are: a framed wall with the Green
    // Hand's banner in the middle between two flags on poles, scrolls above and below ("LA VERDA
    // MANO", "NENIAM CEDI"), laurels, the hand in roundels, "VERDKERNO" and "FONDITA 1887".
    // `banner` is the flag's texture, `font` one with Esperanto's letters (outside drawing).
    static constexpr int mural_width = 1024, mural_height = 640;
    void prepare_mural(const Texture2D& banner, const Font& font);
    [[nodiscard]] bool mural_ready() const { return mural_ready_; }
    [[nodiscard]] const Texture2D& mural() const { return mural_.texture; }
    // On a wall: centred at `centre`, facing `normal`, `width` metres wide (inside BeginMode3D).
    void draw_mural(Vector3 centre, Vector3 normal, float width, Vector3 camera) const;

private:
    std::array<RenderTexture2D, gang_count> textures_{};
    bool ready_{false};
    RenderTexture2D mural_{};
    bool mural_ready_{false};
};

// The Verda Mano GT's paint (tools/generate_green_hand_car.py): the banner's star, hand and crown
// on the bonnet, and the banner on both front wings, at the car's `position` and `yaw` (degrees).
inline constexpr const char* green_hand_car = "green_hand_gt";
void draw_green_hand_livery(Vector3 position, float yaw, const Texture2D& banner, Vector3 camera);

} // namespace outland::game::mafia
