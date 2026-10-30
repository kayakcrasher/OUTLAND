#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }
namespace outland::game::life { struct Island; }

// Everything written on Verda: shopfronts, town signs, street names, bus stops, and the flags.
// Layout is plain data computed from the map and island life's places (testable headless);
// SignRenderer draws it.
namespace outland::game::culture {

// Homemade: hand-painted boards along Ferarbaro's back roads. Highway: the big green boards on the
// ring road and at its interchanges (exits, distances, the way onto the ring).
enum class SignKind : std::uint8_t { Shopfront, TownEntry, Distance, Street, BusStop, Notice, Homemade, Highway };

struct SignLine {
    std::string text;   // UTF-8 Esperanto
    float height{.4F};  // letter height in metres
};

struct Sign {
    SignKind kind{SignKind::Shopfront};
    std::string place;          // what it belongs to (place or asset id), for tests and debugging
    Vector3 position{};         // centre of the text face
    float yaw{0};               // the face looks along (sin yaw, 0, cos yaw)
    float max_width{6};         // the board never gets wider; text shrinks to fit
    std::vector<SignLine> front;
    std::vector<SignLine> back; // empty: plain board behind
    Color board{0, 122, 51, 255}, ink{255, 255, 255, 255};
    float post{0};              // >0: one post this tall from the ground up to the board's bottom
};

// Which cloth a pole flies: the national flag, the Apelacio - the backcountry's pine tree - or
// La Verda Mano's banner over its hideouts and the towns it holds.
enum class FlagDesign : unsigned char { Verda, Apelacio, VerdaMano };

struct FlagPole {
    std::string place;
    Vector3 base{};             // foot of the staff
    Vector3 tip{};              // top of the staff; the flag's hoist hangs from here
    Vector3 fly{1, 0, 0};       // horizontal direction the cloth streams toward
    float width{2.4F};          // cloth length along `fly`; height is half of it
    FlagDesign design{FlagDesign::Verda};
};

struct Signage {
    std::vector<Sign> signs;
    std::vector<FlagPole> flags;
};

// Builds every sign and flag. `island` supplies places (with their designated purposes) and the
// residents who run them. Deterministic for the same region and island.
Signage build_signage(const world::VerdaRegion& region, const life::Island& island);

// The Verda flag: five green and white stripes with a white hoist triangle carrying a green star,
// Cuba's layout in Esperanto's colours. CPU image, `width` x width/2.
Image flag_image(int width);
// The Apelacio flag, the backcountry's own and flown above all in Sovaĝejo's trailer park: the old
// Appeal to Heaven pine-tree flag in Esperanto - a green pine on white under the motto
// "APELACIO AL LA ĈIELO". The motto is drawn with `font` (one with ĉ); without it, only the tree.
inline constexpr const char* apelacio_motto = "APELACIO AL LA ĈIELO";
Image apelacio_image(int width, const Font* font);
inline constexpr Color verda_green{0, 153, 0, 255};
// La Standardo de la Verda Mano: the Ulster Banner in Esperanto - a green cross on white, and on it
// Esperanto's five-pointed star in white carrying the open green hand, under a gold crown topped
// with a green star. CPU image, `width` x width/2.
Image verda_mano_image(int width);
}
