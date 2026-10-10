#include "outland/characters/SkeletalGait.hpp"
#include "raylib_animation_frame.hpp"
#include <raymath.h>
#include <cassert>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
using namespace outland::characters;
extern "C" void UpdateModelAnimation(Model,ModelAnimation,outland::test::AnimationFrame){}
int main() {
    BoneInfo bones[7]{};const char* names[]{"Hips","LeftUpLeg","LeftLeg","LeftFoot","RightUpLeg","RightLeg","RightFoot"};
    const int parents[]{-1,0,1,2,0,4,5};
    Transform bind[7]{};for(int i=0;i<7;++i){std::strcpy(bones[i].name,names[i]);bones[i].parent=parents[i];bind[i].rotation=QuaternionIdentity();bind[i].scale={1,1,1};}
    bind[0].translation={0,1,0};bind[1].translation={-.2F,1,0};bind[2].translation={-.2F,.5F,0};bind[3].translation={-.2F,0,0};
    bind[4].translation={.2F,1,0};bind[5].translation={.2F,.5F,0};bind[6].translation={.2F,0,0};
    Model model{};model.transform=MatrixIdentity();model.skeleton.boneCount=7;model.skeleton.bones=bones;model.skeleton.bindPose=bind;
    SkeletalGait gait;assert(gait.sample(model,AnimationAction::Walk,.25,-1,0));const auto pose=gait.pose();
    assert(pose[3].translation.z*pose[6].translation.z<0); // Opposing steps, not sliding bind pose.
    for(int i=1;i<7;++i)assert(std::abs(Vector3Distance(pose[i].translation,pose[parents[i]].translation)-Vector3Distance(bind[i].translation,bind[parents[i]].translation))<.0001F);
    assert(gait.sample(model,AnimationAction::Walk,1.25,-1,0));assert(Vector3Distance(gait.pose()[3].translation,pose[3].translation)<.0001F);
    assert(gait.sample(model,AnimationAction::Run,.25,-1,0));assert(std::abs(gait.pose()[3].translation.z)>std::abs(pose[3].translation.z));
    assert(gait.sample(model,AnimationAction::Idle,0,-1,0));assert(Vector3Distance(gait.pose()[3].translation,bind[3].translation)<.0001F);
    // Forward for this rig is -Z (up x left->right). Knees bend backward: at mid-swing the foot
    // trails the straight-leg line from hip through knee, never kicks ahead of it.
    for(const float speed:{1.4F,6.0F,10.0F}) for(const double cycle:{0.0,.1,.4,.5,.6,.9}) {
        assert(gait.sample(model,AnimationAction::Walk,cycle,speed,0));const auto& p=gait.pose();
        for(const int hip:{1,4}) {
            const auto straight=Vector3Add(p[hip+1].translation,Vector3Subtract(p[hip+1].translation,p[hip].translation));
            assert(-p[hip+2].translation.z<=-straight.z+1e-4F); // foot not ahead of a straight leg
        }
        // The lowest foot is planted on the ground: no floating, no sinking.
        assert(std::abs(std::min(p[3].translation.y,p[6].translation.y))<1e-4F);
    }
    assert(gait.sample(model,AnimationAction::Walk,0,0,0));{const auto& p=gait.pose();assert(Vector3Distance(p[2].translation,p[1].translation)>.49F);}
    // Mid-swing knee flexion is visible (left leg swinging through at cycle 0).
    assert(gait.sample(model,AnimationAction::Walk,0,1.4F,0));
    {const auto& p=gait.pose();const auto thigh=Vector3Normalize(Vector3Subtract(p[2].translation,p[1].translation)),shin=Vector3Normalize(Vector3Subtract(p[3].translation,p[2].translation));
     assert(std::acos(std::clamp(Vector3DotProduct(thigh,shin),-1.0F,1.0F))>.3F);}
    // Faster walking means a longer stride (running adds flight instead of an ever-longer stride).
    const auto reach=[&](float speed){assert(gait.sample(model,AnimationAction::Walk,.25,speed,0));return std::abs(gait.pose()[3].translation.z-gait.pose()[6].translation.z);};
    assert(reach(.8F)<reach(1.4F) && reach(1.4F)<reach(2.4F) && reach(0)<1e-4F);
    // The cycle keeps going across walk -> run, so the legs never snap.
    {
        AnimationController controller;controller.advance(AnimationAction::Walk,.5F,1.4F);
        const double before=controller.gait_cycle();controller.advance(AnimationAction::Run,.016F,6.0F);
        assert(controller.gait_cycle()>before && controller.gait_cycle()-before<.05);
        assert(AnimationController::gait_cadence(6)>AnimationController::gait_cadence(1.4F));
    }
    // The display transform (Z-up PSX bodies are turned upright when drawn) must not change the
    // swing: it is applied after skinning. Using it as the bone basis stretched arms across the screen.
    model.transform=MatrixMultiply(MatrixRotateX(-PI*.5F),MatrixScale(40,40,40));
    assert(gait.sample(model,AnimationAction::Walk,.25,-1,0));
    for(int i=0;i<7;++i)assert(Vector3Distance(gait.pose()[i].translation,pose[i].translation)<.0001F);
    model.transform=MatrixIdentity();
    // Poses are conjugated into the space raylib baked into the vertices (glTF mesh-node transform).
    {
        const auto skin=MatrixMultiply(MatrixScale(.01F,.01F,.01F),MatrixRotateX(PI*.5F));
        assert(gait.sample(model,AnimationAction::Walk,.25,-1,0));const auto bone_space=gait.pose();
        gait.to_skin_space(model,skin);
        const auto matrix=[](const Transform& t){return MatrixMultiply(MatrixMultiply(MatrixScale(t.scale.x,t.scale.y,t.scale.z),QuaternionToMatrix(t.rotation)),MatrixTranslate(t.translation.x,t.translation.y,t.translation.z));};
        for(int i=0;i<7;++i) {
            const auto raylib_skin=MatrixMultiply(MatrixInvert(matrix(bind[i])),matrix(gait.pose()[i]));
            const auto wanted=MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixInvert(skin),MatrixInvert(matrix(bind[i]))),matrix(bone_space[i])),skin);
            for(const Vector3 v:{Vector3{.003F,.01F,.002F},Vector3{-.002F,.004F,.009F}})
                assert(Vector3Distance(Vector3Transform(v,raylib_skin),Vector3Transform(v,wanted))<1e-5F);
        }
    }
    assert(!gait.sample(model,AnimationAction::Death,0,-1,0));
    assert(gait.sample(model,AnimationAction::Attack,0,-1,0)); // stance, not a T-pose
    assert(Vector3Distance(bind[3].translation,{-.2F,0,0})==0); // Bind data is never overwritten.
    bones[0].parent=1;assert(!gait.sample(model,AnimationAction::Walk,0,-1,0));bones[0].parent=-1;
    std::strcpy(bones[1].name,"unsupported");assert(!gait.sample(model,AnimationAction::Walk,0,-1,0));
    std::cout<<"[PASS] Skeletal walking/running, opposing legs, stride loop, limb lengths, idle and unknown/dead rig fallback\n";
}
