#include "outland/assets/RaylibContract.hpp"
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
    if(!model.skeleton.bones || !model.skeleton.bindPose || model.skeleton.boneCount<=0 || model.skeleton.boneCount>256 || !std::isfinite(seconds) || !std::isfinite(facing)) return false;
    if(action!=AnimationAction::Idle && action!=AnimationAction::Walk && action!=AnimationAction::Run) return false;
    auto find=[&](const char* name) {for(int i=0;i<model.skeleton.boneCount;++i) if(named(model.skeleton.bones[i],name))return i;return -1;};
    const int hips=find("Hips"),left=find("LeftUpLeg"),right=find("RightUpLeg");
    if(hips<0 || left<0 || right<0 || find("LeftLeg")<0 || find("RightLeg")<0) return false;
    // Basis comes from the bind skeleton itself (hips->head is up, left->right hip is the swing
    // axis). The model's display transform is not the bone space: Z-up PSX bodies have Y-up bind
    // poses after import, and swinging about the display axes stretched arms across the screen.
    const auto& bind=model.skeleton.bindPose;
    int top=find("Head");
    if(top<0) top=find("Neck");
    // Without a head, the thigh (hip above knee) gives the vertical.
    const auto up=top>=0 ? Vector3Normalize(Vector3Subtract(bind[top].translation,bind[hips].translation)) :
        Vector3Normalize(Vector3Subtract(bind[left].translation,bind[find("LeftLeg")].translation));
    auto lateral=Vector3Subtract(bind[right].translation,bind[left].translation);
    lateral=Vector3Normalize(Vector3Subtract(lateral,Vector3Scale(up,Vector3DotProduct(lateral,up))));
    if(!std::isfinite(up.x) || !std::isfinite(lateral.x) || !std::isfinite(lateral.y) || !std::isfinite(lateral.z) ||
       Vector3LengthSqr(up)<.5F || Vector3LengthSqr(lateral)<.5F) return false;
    (void)facing;
    const float phase=static_cast<float>(std::fmod(seconds,1000.0))*(action==AnimationAction::Run ? 2.6F:1.5F)*2*PI;
    const float stride=action==AnimationAction::Idle ? 0 : action==AnimationAction::Run ? .65F:.38F;
    const float wave=std::sin(phase)*stride;
    pose_.assign(model.skeleton.bindPose,model.skeleton.bindPose+model.skeleton.boneCount);
    std::vector<unsigned char> done(static_cast<std::size_t>(model.skeleton.boneCount));
    std::function<bool(int)> build=[&](int i) {
        if(done[i]==2)return true;
        if(done[i]==1)return false;
        done[i]=1;
        const int parent=model.skeleton.bones[i].parent;
        if(parent>=model.skeleton.boneCount || parent==i || parent<-1)return false;
        auto& out=pose_[i];const auto bind=model.skeleton.bindPose[i];
        Quaternion inherited=QuaternionIdentity();
        if(parent>=0) {
            if(!build(parent))return false;
            inherited=QuaternionMultiply(pose_[parent].rotation,QuaternionInvert(model.skeleton.bindPose[parent].rotation));
            out.translation=Vector3Add(pose_[parent].translation,Vector3RotateByQuaternion(Vector3Subtract(bind.translation,model.skeleton.bindPose[parent].translation),inherited));
            out.rotation=QuaternionMultiply(inherited,bind.rotation);
        }
        if(named(model.skeleton.bones[i],"LeftArm") || named(model.skeleton.bones[i],"RightArm")) {
            const int elbow=find(named(model.skeleton.bones[i],"LeftArm") ? "LeftForeArm":"RightForeArm");
            if(elbow>=0) {
                const auto direction=Vector3RotateByQuaternion(Vector3Subtract(model.skeleton.bindPose[elbow].translation,bind.translation),inherited);
                if(Vector3LengthSqr(direction)>.00001F)out.rotation=QuaternionMultiply(QuaternionFromVector3ToVector3(Vector3Normalize(direction),Vector3Scale(up,-1)),out.rotation);
            }
        }
        float angle=0;
        if(named(model.skeleton.bones[i],"LeftUpLeg"))angle=wave;
        if(named(model.skeleton.bones[i],"RightUpLeg"))angle=-wave;
        if(named(model.skeleton.bones[i],"LeftLeg"))angle=std::max(0.0F,-std::sin(phase))*stride*1.1F;
        if(named(model.skeleton.bones[i],"RightLeg"))angle=std::max(0.0F,std::sin(phase))*stride*1.1F;
        if(named(model.skeleton.bones[i],"LeftArm"))angle=-wave*.65F;
        if(named(model.skeleton.bones[i],"RightArm"))angle=wave*.65F;
        if(angle!=0)out.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(lateral,angle),out.rotation));
        done[i]=2;return true;
    };
    for(int i=0;i<model.skeleton.boneCount;++i)if(!build(i))return false;
    return true;
}
void SkeletalGait::to_skin_space(const Model& model,const Matrix& skin) {
    // raylib skins with inverse(bind) then pose, both in bone space, but applies the glTF mesh
    // node's world transform (e.g. a +90 X / 0.01 Armature) to the vertices only. Conjugate each
    // pose so the deformation happens in the space the vertices live in:
    //   pose' = bind -> skin^-1 -> bind^-1 -> pose -> skin   (applied left to right)
    const auto inverse=MatrixInvert(skin);
    const auto matrix=[](const Transform& t){return MatrixMultiply(MatrixMultiply(MatrixScale(t.scale.x,t.scale.y,t.scale.z),QuaternionToMatrix(t.rotation)),MatrixTranslate(t.translation.x,t.translation.y,t.translation.z));};
    for(int i=0;i<model.skeleton.boneCount && i<static_cast<int>(pose_.size());++i) {
        const auto bind=matrix(model.skeleton.bindPose[i]);
        const auto result=MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixMultiply(bind,inverse),MatrixInvert(bind)),matrix(pose_[i])),skin);
        Transform t{};MatrixDecompose(result,&t.translation,&t.rotation,&t.scale);
        pose_[i]=t;
    }
}
bool SkeletalGait::apply(Model& model,AnimationAction action,double seconds,float facing,const Matrix* skin) {
    if(!sample(model,action,seconds,facing))return false;
    if(skin) to_skin_space(model,*skin);
    auto* frame=pose_.data();ModelAnimation animation{};animation.boneCount=model.skeleton.boneCount;
    animation.keyframeCount=1;animation.keyframePoses=&frame;
    UpdateModelAnimation(model,animation,0);return true;
}
}
