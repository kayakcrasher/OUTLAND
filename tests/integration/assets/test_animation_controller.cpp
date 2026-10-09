#include "outland/characters/AnimationController.hpp"
#include "raylib_animation_frame.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
using namespace outland::characters;
namespace {int updates=0,last_frames=0,last_frame=-1;Transform* last_pose=nullptr;}
extern "C" void UpdateModelAnimation(Model,ModelAnimation clip,outland::test::AnimationFrame frame) {
    ++updates;last_frames=clip.keyframeCount;last_frame=static_cast<int>(frame);last_pose=clip.keyframePoses[last_frame];
}
int main() {
    BoneInfo bones[2]{};std::strcpy(bones[0].name,"root");bones[0].parent=-1;std::strcpy(bones[1].name,"child");bones[1].parent=0;
    Transform bind[2]{},frames[4][2]{};Transform* poses[4]{frames[0],frames[1],frames[2],frames[3]};
    Model model{};model.skeleton.boneCount=2;model.skeleton.bones=bones;model.skeleton.bindPose=bind;
    ModelAnimation clips[3]{};
    for(auto& c:clips){c.boneCount=2;c.keyframeCount=4;c.keyframePoses=poses;}
    std::strcpy(clips[0].name,"Idle");std::strcpy(clips[1].name,"Walk");std::strcpy(clips[2].name,"Attack Punch");
    AnimationController controller;
    assert(controller.sample(model,{}).clip==-1);
    assert(controller.sample(model,clips).clip==0 && !controller.sample(model,clips).fallback);
    controller.advance(AnimationAction::Walk,.0171F);assert(controller.sample(model,clips).frame==1);
    controller.advance(AnimationAction::Walk,.068F);assert(controller.sample(model,clips).frame==1); // Loops.
    controller.advance(AnimationAction::Run,0);auto sample=controller.sample(model,clips);assert(sample.clip==1 && sample.fallback && sample.frame==0);
    controller.advance(AnimationAction::Attack,10);sample=controller.sample(model,clips);assert(sample.clip==2 && sample.frame==3 && !sample.fallback);
    controller.restart();assert(controller.sample(model,clips).frame==0);
    controller.advance(AnimationAction::Death,10);assert(controller.sample(model,clips).clip==-1); // Never idle a corpse.
    std::strcpy(clips[2].name,"Death");sample=controller.sample(model,clips);assert(sample.clip==2 && sample.frame==3);
    controller.advance(AnimationAction::None,0);assert(controller.sample(model,clips).clip==-1);
    controller.advance(AnimationAction::Attack,0);assert(controller.sample(model,clips).clip==0 && controller.sample(model,clips).fallback);
    const auto before=controller.elapsed();controller.advance(AnimationAction::Attack,std::numeric_limits<float>::quiet_NaN());assert(controller.elapsed()==before);
    assert(AnimationController::classify("mixamo.com|Layer0")==AnimationAction::None);
    assert(AnimationController::classify("Right jab")==AnimationAction::Attack);
    assert(AnimationController::classify("DEATH attack")==AnimationAction::Death);
    assert(AnimationController::classify("walking")==AnimationAction::None);
    assert(AnimationController::compatible(model,clips[0]));
    bones[1].parent=1;assert(!AnimationController::compatible(model,clips[0]));bones[1].parent=0;
    clips[0].boneCount=3;assert(!AnimationController::compatible(model,clips[0]));clips[0].boneCount=2;
    clips[0].keyframeCount=1;assert(!AnimationController::compatible(model,clips[0]));clips[0].keyframeCount=4;
    poses[0]=nullptr;assert(!AnimationController::compatible(model,clips[0]));poses[0]=frames[0];
    assert(AnimationController::apply_sample(model,clips,{1,2,false},false));
    assert(updates==1 && last_frames==4 && last_frame==2 && last_pose==frames[2]);
    assert(!AnimationController::apply_sample(model,clips,{},true));
    assert(updates==2 && last_frames==1 && last_frame==0 && last_pose==bind); // Another actor's pose is cleared.
    assert(!AnimationController::apply_sample(model,clips,{},false) && updates==2);
    assert(!AnimationController::apply_sample(model,clips,{999,0,false},false) && updates==2);
    model.skeleton.boneCount=0;assert(controller.sample(model,clips).clip==-1);
    std::cout<<"[PASS] Animation clock, named clips, loops, one-shots, rig checks and clean missing-clip/skin fallback\n";
}
