#include "outland/game/career/CampRenderer.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::game::career {
namespace {
const Color gravel{150, 146, 136, 255}, paint{236, 236, 228, 255}, navy{22, 36, 84, 255}, navy_dark{14, 24, 58, 255},
            post{96, 98, 100, 255}, mesh{170, 176, 180, 70}, earth{112, 92, 62, 255}, earth_dark{92, 74, 50, 255},
            wood{150, 118, 74, 255}, card{214, 196, 150, 255}, sandbag{168, 150, 108, 255}, red{200, 40, 36, 255},
            amber{240, 170, 40, 255}, yellow{236, 206, 60, 255}, flag_blue{40, 90, 200, 255};

struct Frame {
    const TrainingCamp& camp;
    const std::function<float(float, float)>& ground;
    // A box sitting on the ground at local (x, z): size w (across) x h x d (downrange), lifted by `lift`.
    void box(float x, float z, float w, float h, float d, Color c, float lift = 0) const {
        const Vector3 at = camp.local(x, z);
        const float y = ground(at.x, at.z) + lift;
        rlPushMatrix();
        rlTranslatef(at.x, y + h * .5F, at.z);
        rlRotatef(camp.yaw(), 0, 1, 0);
        DrawCube({0, 0, 0}, w, h, d, c);
        rlPopMatrix();
    }
};
}

void draw_camp(const TrainingCamp& camp, Vector3 camera, float time, const std::function<float(float, float)>& ground) {
    if (!camp.placed()) return;
    if (Vector3Distance(camp.line(), camera) > 420) return;
    const Frame f{camp, ground};
    const float W = TrainingCamp::half_width, D = TrainingCamp::depth, B = TrainingCamp::behind;

    // The gravel apron behind the line, and the line itself.
    for (float x = -W + 2; x < W; x += 4)
        for (float z = -B + 2; z < 0; z += 4) f.box(x, z, 4.02F, .06F, 4.02F, gravel, -.02F);
    f.box(0, .1F, 26, .07F, .18F, paint);
    // Booth dividers between the lanes, sandbags at the ends.
    for (const float x : {-6.0F, 6.0F}) f.box(x, -.2F, .14F, 1.1F, 2.2F, navy_dark);
    for (const float x : {-13.5F, 13.5F})
        for (int row = 0; row < 3; ++row) f.box(x, -.3F, 2.2F - row * .4F, .3F, .7F, sandbag, row * .3F);
    // The stand: a lectern with the drill board.
    f.box(0, -1.2F, .7F, 1.05F, .45F, navy);
    f.box(0, -1.2F, .76F, .06F, .5F, paint, 1.05F);
    f.box(0, -1.45F, .6F, .5F, .06F, card, 1.0F);

    // Distance posts on both sides: 10 m yellow, 15 m amber, 25 m red; a board at each.
    const std::pair<float, Color> marks[]{{10.0F, yellow}, {15.0F, amber}, {25.0F, red}};
    for (const auto& [z, c] : marks)
        for (const float x : {-W + 3, W - 3}) {
            f.box(x, z, .12F, 1.6F, .12F, post);
            f.box(x, z, .7F, .45F, .06F, paint, 1.2F);
            f.box(x, z, .7F, .12F, .07F, c, 1.62F);
        }
    // The berm behind the targets: an earth bank, stepped.
    for (int k = 0; k < 4; ++k)
        f.box(0, D - 4.5F + k * .9F, W * 2 - 4, 1.3F + k * .9F, 1.0F, k % 2 ? earth : earth_dark);

    // The fence: posts every 4 m, two rails, a see-through mesh; the gate behind the line.
    const auto fence = [&](float x0, float z0, float x1, float z1) {
        const float length = std::hypot(x1 - x0, z1 - z0);
        const int posts = std::max(1, static_cast<int>(length / 4));
        for (int i = 0; i <= posts; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(posts);
            const float x = x0 + (x1 - x0) * t, z = z0 + (z1 - z0) * t;
            if (z0 == z1 && z0 < 0 && std::abs(x) < 3) continue;       // the gate
            f.box(x, z, .1F, 2.3F, .1F, post);
            if (i == posts) break;
            const float xm = x + (x1 - x0) / static_cast<float>(posts) * .5F, zm = z + (z1 - z0) / static_cast<float>(posts) * .5F;
            if (z0 == z1 && z0 < 0 && std::abs(xm) < 3) continue;
            const bool across = z0 == z1;
            const float span = length / static_cast<float>(posts);
            for (const float y : {1.05F, 2.2F}) f.box(xm, zm, across ? span : .05F, .05F, across ? .05F : span, post, y);
            f.box(xm, zm, across ? span : .02F, 2.1F, across ? .02F : span, mesh, .1F);
        }
    };
    fence(-W, -B, W, -B);
    fence(-W, D, W, D);
    fence(-W, -B, -W, D);
    fence(W, -B, W, D);
    // The range hut (navy, white roof, a door) and a flagpole with the Policio's blue and white.
    f.box(-W + 4, -B + 3, 3.4F, 2.6F, 3.0F, navy);
    f.box(-W + 4, -B + 3, 3.8F, .2F, 3.4F, paint, 2.6F);
    f.box(-W + 4 + 1.71F, -B + 3, .02F, 2.0F, .9F, navy_dark);
    f.box(W - 3, -B + 2, .1F, 7.0F, .1F, post);
    const float wave = std::sin(time * 2.2F) * .06F;
    f.box(W - 3 - .65F, -B + 2, 1.2F, .35F, .03F + wave * .1F, flag_blue, 6.2F);
    f.box(W - 3 - .65F, -B + 2, 1.2F, .35F, .03F, paint, 5.85F);

    // The target: a silhouette card on a frame, hinged at its foot (it lies flat until it pops up).
    const auto& t = camp.target();
    if (camp.phase() == CampPhase::Running && (t.up || t.raise > 0)) {
        rlPushMatrix();
        rlTranslatef(t.base.x, t.base.y, t.base.z);
        rlRotatef(camp.yaw(), 0, 1, 0);
        DrawCube({0, .08F, -.15F}, .9F, .16F, .5F, wood);              // the base box
        rlRotatef((1 - t.raise) * 90, 1, 0, 0);                         // falls back, away from the line
        const float w = TrainingCamp::target_width, h = TrainingCamp::target_height;
        DrawCube({0, h * .5F + .1F, 0}, w + .08F, h, .04F, card);
        DrawCube({0, .95F, -.025F}, w * .78F, 1.0F, .02F, t.hit ? red : navy);            // torso
        DrawCube({0, 1.58F, -.025F}, w * .42F, .3F, .02F, t.hit && t.headshot ? red : navy); // head
        DrawCube({0, 1.0F, -.03F}, .14F, .14F, .02F, paint);                             // the ring
        rlPopMatrix();
    }
}

} // namespace outland::game::career
