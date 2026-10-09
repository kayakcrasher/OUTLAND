#include "outland/characters/SkeletalGait.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <functional>
namespace outland::characters {
namespace {
bool named(const BoneInfo& bone,const char* suffix) {
    const std::string_view name(bone.name,static_cast<std::size_t>(std::find(bone.name,bone.name+32,'\0')-bone.name));
    return name==suffix || name.ends_with(std::string(":")+suffix);
}
}
bool SkeletalGait::sample(const Model& model,AnimationAction action,double seconds,float facing) {
    if(!model.bones || !model.bindPose || model.boneCount<=0 || model.boneCount>256 || !std::isfinite(seconds) || !std::isfinite(facing)) return false;
    if(action!=AnimationAction::Idle && action!=AnimationAction::Walk && action!=AnimationAction::Run) return false;
    auto find=[&](const char* name) {for(int i=0;i<model.boneCount;++i) if(named(model.bones[i],name))return i;return -1;};
    const int hips=find("Hips"),left=find("LeftUpLeg"),right=find("RightUpLeg");
    if(hips<0 || left<0 || right<0 || find("LeftLeg")<0 || find("RightLeg")<0) return false;
    const auto inverse=MatrixInvert(model.transform);
    // Basis follows CharacterRenderer's facing convention and model transform,
    // so Z-up PSX and Y-up rebel rigs receive the same sagittal leg swing.
    const auto up=Vector3Normalize(Vector3Subtract(Vector3Transform({0,1,0},inverse),Vector3Transform({0,0,0},inverse)));
    const auto world_forward=Vector3RotateByAxisAngle({0,0,1},{0,1,0},-facing*DEG2RAD);
    const auto forward=Vector3Normalize(Vector3Subtract(Vector3Transform(world_forward,inverse),Vector3Transform({0,0,0},inverse)));
    const auto lateral=Vector3Normalize(Vector3CrossProduct(up,forward));
    if(!std::isfinite(lateral.x) || !std::isfinite(lateral.y) || !std::isfinite(lateral.z) || Vector3LengthSqr(lateral)<.5F)return false;
    const float phase=static_cast<float>(std::fmod(seconds,1000.0))*(action==AnimationAction::Run ? 2.6F:1.5F)*2*PI;
    const float stride=action==AnimationAction::Idle ? 0 : action==AnimationAction::Run ? .65F:.38F;
    const float wave=std::sin(phase)*stride;
    pose_.assign(model.bindPose,model.bindPose+model.boneCount);
    std::vector<unsigned char> done(static_cast<std::size_t>(model.boneCount));
    std::function<bool(int)> build=[&](int i) {
        if(done[i]==2)return true;
        if(done[i]==1)return false;
        done[i]=1;
        const int parent=model.bones[i].parent;
        if(parent>=model.boneCount || parent==i || parent<-1)return false;
        auto& out=pose_[i];const auto bind=model.bindPose[i];
        Quaternion inherited=QuaternionIdentity();
        if(parent>=0) {
            if(!build(parent))return false;
            inherited=QuaternionMultiply(pose_[parent].rotation,QuaternionInvert(model.bindPose[parent].rotation));
            out.translation=Vector3Add(pose_[parent].translation,Vector3RotateByQuaternion(Vector3Subtract(bind.translation,model.bindPose[parent].translation),inherited));
            out.rotation=QuaternionMultiply(inherited,bind.rotation);
        }
        if(named(model.bones[i],"LeftArm") || named(model.bones[i],"RightArm")) {
            const int elbow=find(named(model.bones[i],"LeftArm") ? "LeftForeArm":"RightForeArm");
            if(elbow>=0) {
                const auto direction=Vector3RotateByQuaternion(Vector3Subtract(model.bindPose[elbow].translation,bind.translation),inherited);
                if(Vector3LengthSqr(direction)>.00001F)out.rotation=QuaternionMultiply(QuaternionFromVector3ToVector3(Vector3Normalize(direction),Vector3Scale(up,-1)),out.rotation);
            }
        }
        float angle=0;
        if(named(model.bones[i],"LeftUpLeg"))angle=wave;
        if(named(model.bones[i],"RightUpLeg"))angle=-wave;
        if(named(model.bones[i],"LeftLeg"))angle=std::max(0.0F,-std::sin(phase))*stride*1.1F;
        if(named(model.bones[i],"RightLeg"))angle=std::max(0.0F,std::sin(phase))*stride*1.1F;
        if(named(model.bones[i],"LeftArm"))angle=-wave*.65F;
        if(named(model.bones[i],"RightArm"))angle=wave*.65F;
        if(angle!=0)out.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(lateral,angle),out.rotation));
        done[i]=2;return true;
    };
    for(int i=0;i<model.boneCount;++i)if(!build(i))return false;
    return true;
}
bool SkeletalGait::apply(Model& model,AnimationAction action,double seconds,float facing) {
    if(!sample(model,action,seconds,facing))return false;
    auto* frame=pose_.data();ModelAnimation animation{};animation.boneCount=model.boneCount;
    animation.frameCount=1;animation.bones=model.bones;animation.framePoses=&frame;
    UpdateModelAnimation(model,animation,0);return true;
}
}
