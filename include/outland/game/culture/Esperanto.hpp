#pragma once
#include "outland/game/life/LifeTypes.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Verda is a small Esperanto-speaking republic (see docs/verda-lore.md), so everything written in
// the world is in Esperanto. Text is UTF-8; the supersigned letters (ĉ ĝ ĥ ĵ ŝ ŭ) need a font
// that has them, which is why signs use assets/fonts rather than raylib's built-in font.
namespace outland::game::culture {

struct SignText {
    std::string title;      // large line, e.g. "BAKEJO"
    std::string subtitle;   // small line under it, may be empty
};

// Shopfront wording for a place. `ordinal` counts places of the same kind in the same town, nearest
// the centre first (the first office is the town hall); with `seed` (one per town) it picks the
// trade, so one shop is a bakery and the next a bookshop. `owner` is the family name of whoever
// runs it (may be empty).
SignText place_sign(life::PlaceKind kind, std::uint32_t seed, std::string_view owner, int ordinal, bool capital);

// The n-th street name of a town's grid (wraps around).
std::string street_name(int index);
std::string upper(std::string_view text); // UTF-8 aware, including ĉ → Ĉ
// Esperanto number style: "850 m", "2,1 km".
std::string format_distance(float metres);

// Every codepoint sign text may use; the sign font is loaded with exactly these.
const std::vector<int>& sign_codepoints();
std::vector<int> utf8_codepoints(std::string_view text);
bool printable(std::string_view text); // every codepoint is in sign_codepoints()

std::uint32_t text_hash(std::string_view text);
}
