#include "outland/assets/RaylibContract.hpp"
#include "outland/characters/AnimationController.hpp"
#include "outland/characters/SkeletalGait.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <raymath.h>
using namespace outland::characters;
int main() {
    BoneInfo bones[2]{};
    bones[0].parent = -1;
    bones[1].parent = 0;
    std::strcpy(bones[0].name, "root");
    std::strcpy(bones[1].name, "child");
    Transform bind[2]{}, current[2]{}, frames[2][2]{};
    Matrix matrices[2]{};
    for (int i = 0; i < 2; ++i) {
        bind[i].rotation = QuaternionIdentity();
        bind[i].scale = {1, 1, 1};
        frames[0][i] = frames[1][i] = bind[i];
    }
    frames[1][1].translation = {2, 0, 0};
    Transform *poses[]{frames[0], frames[1]};
    ModelAnimation clip{};
    std::strcpy(clip.name, "Walk");
    clip.boneCount = 2;
    clip.keyframeCount = 2;
    clip.keyframePoses = poses;
    Model model{};
    model.skeleton = {2, bones, bind};
    model.currentPose = current;
    model.boneMatrices = matrices;
    model.transform = MatrixIdentity();
    assert(AnimationController::compatible(model, clip));
    UpdateModelAnimation(model, clip, .5F);
    assert(std::abs(current[1].translation.x - 1) < .0001F &&
           std::abs(matrices[1].m12 - 1) < .0001F);
    assert(AnimationController::apply_sample(model, {&clip, 1}, {0, 1, false}, true));
    assert(current[1].translation.x == 2);
    assert(!AnimationController::apply_sample(model, {}, {}, true));
    assert(current[1].translation.x == 0 && matrices[1].m12 == 0);
    bones[0].parent = 1;
    assert(!AnimationController::compatible(model, clip));
    bones[0].parent = -1;
    std::cout << "[PASS] Actual raylib 6 fractional keyframes, currentPose, bone matrices, bind "
                 "reset and cyclic rig rejection (headless)\n";
}
