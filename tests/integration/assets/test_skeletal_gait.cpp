#include "outland/characters/SkeletalGait.hpp"
#include "raylib_animation_frame.hpp"
#include <raymath.h>
#include <cassert>
#include <cstring>
#include <cmath>
#include <iostream>
using namespace outland::characters;
extern "C" void UpdateModelAnimation(Model,ModelAnimation,outland::test::AnimationFrame){}
int main() {
    BoneInfo bones[7]{};const char* names[]{"Hips","LeftUpLeg","LeftLeg","LeftFoot","RightUpLeg","RightLeg","RightFoot"};
    const int parents[]{-1,0,1,2,0,4,5};
    Transform bind[7]{};for(int i=0;i<7;++i){std::strcpy(bones[i].name,names[i]);bones[i].parent=parents[i];bind[i].rotation=QuaternionIdentity();bind[i].scale={1,1,1};}
    bind[0].translation={0,1,0};bind[1].translation={-.2F,1,0};bind[2].translation={-.2F,.5F,0};bind[3].translation={-.2F,0,0};
    bind[4].translation={.2F,1,0};bind[5].translation={.2F,.5F,0};bind[6].translation={.2F,0,0};
    Model model{};model.transform=MatrixIdentity();model.boneCount=7;model.bones=bones;model.bindPose=bind;
    SkeletalGait gait;assert(gait.sample(model,AnimationAction::Walk,1.0/6,0));const auto pose=gait.pose();
    assert(pose[3].translation.z*pose[6].translation.z<0); // Opposing steps, not sliding bind pose.
    for(int i=1;i<7;++i)assert(std::abs(Vector3Distance(pose[i].translation,pose[parents[i]].translation)-Vector3Distance(bind[i].translation,bind[parents[i]].translation))<.0001F);
    assert(gait.sample(model,AnimationAction::Walk,1.0/6+2.0/3,0));assert(Vector3Distance(gait.pose()[3].translation,pose[3].translation)<.0001F);
    assert(gait.sample(model,AnimationAction::Run,1.0/10.4,0));assert(std::abs(gait.pose()[3].translation.z)>std::abs(pose[3].translation.z));
    assert(gait.sample(model,AnimationAction::Idle,0,0));assert(Vector3Distance(gait.pose()[3].translation,bind[3].translation)<.0001F);
    assert(!gait.sample(model,AnimationAction::Death,0,0));
    assert(!gait.sample(model,AnimationAction::Attack,0,0));
    assert(Vector3Distance(bind[3].translation,{-.2F,0,0})==0); // Bind data is never overwritten.
    bones[0].parent=1;assert(!gait.sample(model,AnimationAction::Walk,0,0));bones[0].parent=-1;
    std::strcpy(bones[1].name,"unsupported");assert(!gait.sample(model,AnimationAction::Walk,0,0));
    std::cout<<"[PASS] Skeletal walking/running, opposing legs, stride loop, limb lengths, idle and unknown/dead rig fallback\n";
}
