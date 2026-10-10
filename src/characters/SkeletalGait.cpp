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
bool SkeletalGait::sample(const Model& model,AnimationAction action,double cycle,float ground_speed,float facing) {
    if(!model.skeleton.bones || !model.skeleton.bindPose || model.skeleton.boneCount<=0 || model.skeleton.boneCount>256 ||
       !std::isfinite(cycle) || !std::isfinite(facing)) return false;
    // Death keeps the caller's fallback; an attack without a clip holds a stance instead of T-posing.
    if(action==AnimationAction::Death || action==AnimationAction::None) return false;
    if(action==AnimationAction::Attack) action=AnimationAction::Idle;
    auto find=[&](const char* name) {for(int i=0;i<model.skeleton.boneCount;++i) if(named(model.skeleton.bones[i],name))return i;return -1;};
    const int hips=find("Hips"),left=find("LeftUpLeg"),right=find("RightUpLeg");
    const int left_knee=find("LeftLeg"),right_knee=find("RightLeg");
    if(hips<0 || left<0 || right<0 || left_knee<0 || right_knee<0) return false;
    // Basis comes from the bind skeleton itself (hips->head is up, left->right hip is the swing
    // axis). The model's display transform is not the bone space: Z-up PSX bodies have Y-up bind
    // poses after import, and swinging about the display axes stretched arms across the screen.
    const auto* bind=model.skeleton.bindPose;
    int top=find("Head");
    if(top<0) top=find("Neck");
    // Without a head, the thigh (hip above knee) gives the vertical.
    const auto up=top>=0 ? Vector3Normalize(Vector3Subtract(bind[top].translation,bind[hips].translation)) :
        Vector3Normalize(Vector3Subtract(bind[left].translation,bind[left_knee].translation));
    auto lateral=Vector3Subtract(bind[right].translation,bind[left].translation);
    lateral=Vector3Normalize(Vector3Subtract(lateral,Vector3Scale(up,Vector3DotProduct(lateral,up))));
    if(!std::isfinite(up.x) || !std::isfinite(lateral.x) || !std::isfinite(lateral.y) || !std::isfinite(lateral.z) ||
       Vector3LengthSqr(up)<.5F || Vector3LengthSqr(lateral)<.5F) return false;
    (void)facing;
    // Forward is where the toes point; rigs without toes fall back to up x right.
    Vector3 forward=Vector3CrossProduct(up,lateral);
    const int left_foot=find("LeftFoot"),right_foot=find("RightFoot"),left_toe=find("LeftToeBase"),right_toe=find("RightToeBase");
    if(left_foot>=0 && left_toe>=0) {
        auto toes=Vector3Subtract(bind[left_toe].translation,bind[left_foot].translation);
        toes=Vector3Subtract(toes,Vector3Scale(up,Vector3DotProduct(toes,up)));
        if(Vector3LengthSqr(toes)>1e-8F && Vector3DotProduct(Vector3Normalize(toes),forward)<-.5F) forward=Vector3Scale(forward,-1);
    }
    // Sign that swings a hanging limb forward about the lateral axis; knees bend the other way.
    const float forward_sign=Vector3DotProduct(Vector3RotateByQuaternion(Vector3Scale(up,-1),QuaternionFromAxisAngle(lateral,.3F)),forward)>0 ? 1.0F : -1.0F;

    // Stride follows ground speed so the planted foot keeps pace with the body (leg ~0.9 m).
    float speed=std::isfinite(ground_speed) && ground_speed>=0 ? ground_speed : AnimationController::nominal_speed(action);
    if(action==AnimationAction::Idle && !(std::isfinite(ground_speed) && ground_speed>=0)) speed=0;
    const float cadence=AnimationController::gait_cadence(speed);
    const float run=std::clamp((speed-2.6F)/2.4F,0.0F,1.0F);     // 0 walk .. 1 run blend
    // Running covers ground in flight, so its thigh swing is shorter than the stride implies.
    const float swing=std::clamp(std::asin(std::min(1.0F,speed/(3.6F*cadence))),0.0F,.62F-.14F*run);
    const float reach=.12F*run; // runners drive the knee forward more than they extend behind
    const float moving=std::clamp(speed/.6F,0.0F,1.0F);
    const float phase=static_cast<float>(std::fmod(cycle,1.0))*2*PI;
    const float knee_swing=(.45F+1.15F*run)*moving; // flexion while the leg swings through (heel tucks)
    const float knee_stance=(.06F+.18F*run)*moving; // slight give under load
    const float elbow=(.18F+.9F*run)*std::max(moving,.35F);
    const float lean=.14F*run;

    // Per leg: thigh angle forward, knee flexion (>=0), for phase offset 0 (left) or PI (right).
    const auto leg=[&](float offset,float& thigh,float& knee) {
        const float p=phase+offset;
        thigh=swing*std::sin(p)+reach*std::max(0.0F,std::sin(p));
        // Flex peaks mid-swing (thigh moving forward), straight at heel strike, small in stance.
        const float swinging=std::max(0.0F,std::cos(p));
        knee=knee_swing*swinging*swinging+knee_stance*std::max(0.0F,-std::cos(p));
    };
    float left_thigh,left_flex,right_thigh,right_flex;
    leg(0,left_thigh,left_flex);leg(PI,right_thigh,right_flex);

    pose_.assign(model.skeleton.bindPose,model.skeleton.bindPose+model.skeleton.boneCount);
    std::vector<unsigned char> done(static_cast<std::size_t>(model.skeleton.boneCount));
    std::function<bool(int)> build=[&](int i) {
        if(done[i]==2)return true;
        if(done[i]==1)return false;
        done[i]=1;
        const int parent=model.skeleton.bones[i].parent;
        if(parent>=model.skeleton.boneCount || parent==i || parent<-1)return false;
        auto& out=pose_[i];const auto rest=model.skeleton.bindPose[i];
        Quaternion inherited=QuaternionIdentity();
        if(parent>=0) {
            if(!build(parent))return false;
            inherited=QuaternionMultiply(pose_[parent].rotation,QuaternionInvert(model.skeleton.bindPose[parent].rotation));
            out.translation=Vector3Add(pose_[parent].translation,Vector3RotateByQuaternion(Vector3Subtract(rest.translation,model.skeleton.bindPose[parent].translation),inherited));
            out.rotation=QuaternionMultiply(inherited,rest.rotation);
        }
        const auto& bone=model.skeleton.bones[i];
        if(named(bone,"LeftArm") || named(bone,"RightArm")) {
            // T-pose arms hang at the sides first.
            const int elbow_bone=find(named(bone,"LeftArm") ? "LeftForeArm":"RightForeArm");
            if(elbow_bone>=0) {
                const auto direction=Vector3RotateByQuaternion(Vector3Subtract(model.skeleton.bindPose[elbow_bone].translation,rest.translation),inherited);
                if(Vector3LengthSqr(direction)>.00001F)out.rotation=QuaternionMultiply(QuaternionFromVector3ToVector3(Vector3Normalize(direction),Vector3Scale(up,-1)),out.rotation);
            }
        }
        // All angles below are "forward" angles about the lateral axis: they swing a limb that
        // hangs down forward. The spine points up, so leaning the chest forward is negative.
        float angle=0;
        if(named(bone,"Spine"))angle=-lean;
        if(named(bone,"LeftUpLeg"))angle=left_thigh;
        if(named(bone,"RightUpLeg"))angle=right_thigh;
        if(named(bone,"LeftLeg"))angle=-left_flex;
        if(named(bone,"RightLeg"))angle=-right_flex;
        // Feet stay near level: undo the shin's pitch, letting the toes lift a little in swing.
        if(named(bone,"LeftFoot"))angle=-(left_thigh-left_flex)*.85F;
        if(named(bone,"RightFoot"))angle=-(right_thigh-right_flex)*.85F;
        // Arms swing against the legs (left arm with the right leg); elbows bend forward.
        if(named(bone,"LeftArm"))angle=-left_thigh*(.75F+.6F*run)+lean;
        if(named(bone,"RightArm"))angle=-right_thigh*(.75F+.6F*run)+lean;
        if(named(bone,"LeftForeArm") || named(bone,"RightForeArm"))angle=elbow;
        if(angle!=0)out.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(lateral,angle*forward_sign),out.rotation));
        done[i]=2;return true;
    };
    for(int i=0;i<model.skeleton.boneCount;++i)if(!build(i))return false;
    // Plant the lowest foot: a swung straight leg is shorter, so drop the body instead of letting
    // both feet float (and never push a foot through the ground).
    float lowest=1e9F;
    for(const int b:{left_foot,right_foot,left_toe,right_toe}) if(b>=0)
        lowest=std::min(lowest,Vector3DotProduct(Vector3Subtract(pose_[b].translation,bind[b].translation),up));
    if(left_foot<0 && right_foot<0) {
        // No feet: use the ends of the shins.
        for(const int b:{left_knee,right_knee}) lowest=std::min(lowest,Vector3DotProduct(Vector3Subtract(pose_[b].translation,bind[b].translation),up));
    }
    if(lowest<1e8F && std::abs(lowest)>1e-7F) for(auto& p:pose_) p.translation=Vector3Subtract(p.translation,Vector3Scale(up,lowest));
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
bool SkeletalGait::apply(Model& model,AnimationAction action,double cycle,float ground_speed,float facing,const Matrix* skin) {
    if(!sample(model,action,cycle,ground_speed,facing))return false;
    if(skin) to_skin_space(model,*skin);
    auto* frame=pose_.data();ModelAnimation animation{};animation.boneCount=model.skeleton.boneCount;
    animation.keyframeCount=1;animation.keyframePoses=&frame;
    UpdateModelAnimation(model,animation,0);return true;
}
}
