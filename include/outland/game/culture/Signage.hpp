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

enum class SignKind : std::uint8_t { Shopfront, TownEntry, Distance, Street, BusStop, Notice };

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

struct FlagPole {
    std::string place;
    Vector3 base{};             // foot of the staff
    Vector3 tip{};              // top of the staff; the flag's hoist hangs from here
    Vector3 fly{1, 0, 0};       // horizontal direction the cloth streams toward
    float width{2.4F};          // cloth length along `fly`; height is half of it
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
inline constexpr Color verda_green{0, 153, 0, 255};
}
