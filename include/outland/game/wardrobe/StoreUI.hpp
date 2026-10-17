#pragma once
#include "outland/game/wardrobe/Wardrobe.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <vector>

// Vestaĵejo Verda's counter and fitting rooms: browse every look and hat, wearing each as you go
// (the game draws the player in the one picked), buy or put on. A panel down the right of the
// screen, so the player stands in view. Mouse, touch or keys: LEFT/RIGHT looks, UP/DOWN hats,
// ENTER buy or wear the look, T the hat, X no hat, ESC close.
namespace outland::game::wardrobe {

struct StoreAction { bool close{false}, buy_look{false}, buy_hat{false}, no_hat{false}; };

class StoreUI {
public:
    // Open on a look (a mannequin's, or what you have on); `hat` -1 for none.
    void open(int look, int hat);
    [[nodiscard]] int look() const { return look_; }
    [[nodiscard]] int hat() const { return hat_; }   // -1: no hat
    StoreAction input(const Catalogue& catalogue, int width, int height, bool focused);
    void draw(const Catalogue& catalogue, const economy::EconomyState& state, bool member, const std::string& status,
              int width, int height) const;
    static Rectangle button(int index, int width, int height);
    void set_font(const Font& font) { font_ = font; }   // one with the Esperanto letters
private:
    Font font_{};
    std::vector<Vector2> clicks(bool focused);
    std::unordered_set<int> previous_;
    bool warm_{true};
    int look_{0}, hat_{-1};
};

} // namespace outland::game::wardrobe
