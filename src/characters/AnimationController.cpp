#include "outland/assets/RaylibContract.hpp"
#include "outland/characters/AnimationController.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
namespace outland::characters {
AnimationAction AnimationController::classify(std::string_view name) {
    std::string token;
    AnimationAction result=AnimationAction::None;
    auto consume=[&] {
        if(token=="death" || token=="die" || token=="dead") result=AnimationAction::Death;
        else if(result!=AnimationAction::Death && (token=="attack" || token=="punch" || token=="jab" || token=="hit" || token=="fire")) result=AnimationAction::Attack;
        else if(result!=AnimationAction::Attack && result!=AnimationAction::Death) {
            if(token=="run" || token=="sprint") result=AnimationAction::Run;
            else if(token=="walk" && result!=AnimationAction::Run) result=AnimationAction::Walk;
            else if((token=="idle" || token=="rest" || token=="relax") && result==AnimationAction::None) result=AnimationAction::Idle;
        }
        token.clear();
    };
    for(unsigned char c:name) {
        if(std::isalnum(c)) token+=static_cast<char>(std::tolower(c));
        else consume();
    }
    consume();return result;
}
bool AnimationController::compatible(const Model& model,const ModelAnimation& clip) {
    if(model.skeleton.boneCount<=0 || model.skeleton.boneCount!=clip.boneCount || clip.keyframeCount<2 ||
        !model.skeleton.bones || !model.skeleton.bindPose || !clip.keyframePoses) return false;
    for(int i=0;i<model.skeleton.boneCount;++i)
        if(model.skeleton.bones[i].parent>=model.skeleton.boneCount || model.skeleton.bones[i].parent==i || model.skeleton.bones[i].parent < -1) return false;
    if(model.skeleton.boneCount>256)return false;
    for(int i=0;i<model.skeleton.boneCount;++i){int ancestor=i;for(int depth=0;ancestor>=0;++depth){if(depth>=model.skeleton.boneCount)return false;ancestor=model.skeleton.bones[ancestor].parent;}}
    if(!clip.keyframePoses[0] || !clip.keyframePoses[clip.keyframeCount-1]) return false;
    if(model.meshCount>0 && !model.meshes) return false;
    for(int i=0;i<model.meshCount;++i)
        if(model.meshes[i].boneCount>0 && model.meshes[i].boneCount!=clip.boneCount) return false;
    return true;
}
void AnimationController::advance(AnimationAction action,float dt) {
    if(action!=action_) { action_=action;elapsed_=0; }
    if(std::isfinite(dt) && dt>0) elapsed_+=dt;
}
AnimationSample AnimationController::sample(const Model& model,std::span<const ModelAnimation> clips) const {
    if(action_==AnimationAction::None) return {};
    auto find=[&](AnimationAction requested)->int {
        for(std::size_t i=0;i<clips.size();++i)
            if(classify(clips[i].name)==requested && compatible(model,clips[i])) return static_cast<int>(i);
        return -1;
    };
    int index=find(action_);
    bool fallback=index<0;
    if(index<0 && action_==AnimationAction::Run) index=find(AnimationAction::Walk);
    if(index<0 && action_!=AnimationAction::Death && action_!=AnimationAction::None) index=find(AnimationAction::Idle);
    if(index<0) return {};
    const auto& clip=clips[static_cast<std::size_t>(index)];
    // raylib 6.0 glTF decoder samples every 17ms. Never infer action from duration.
    const double frames=static_cast<double>(elapsed_)*1000.0/17.0;
    const auto selected=classify(clip.name);
    const bool loop=selected!=AnimationAction::Attack && selected!=AnimationAction::Death;
    const int frame=loop ? static_cast<int>(std::fmod(frames,clip.keyframeCount))
        : static_cast<int>(std::min(frames,static_cast<double>(clip.keyframeCount-1)));
    return {index,frame,fallback};
}
bool AnimationController::apply_sample(Model& model,std::span<const ModelAnimation> clips,AnimationSample sample,bool was_posed) {
    if(sample.clip>=0 && static_cast<std::size_t>(sample.clip)<clips.size()) {
        const auto& clip=clips[static_cast<std::size_t>(sample.clip)];
        if(compatible(model,clip) && sample.frame>=0 && sample.frame<clip.keyframeCount && clip.keyframePoses[sample.frame] && clip.keyframePoses[(sample.frame+1)%clip.keyframeCount]) {
            UpdateModelAnimation(model,clip,sample.frame);
            return true;
        }
    }
    if(was_posed && model.skeleton.bindPose && model.skeleton.bones && model.skeleton.boneCount>0) {
        auto* pose=model.skeleton.bindPose;
        ModelAnimation bind{};bind.boneCount=model.skeleton.boneCount;bind.keyframeCount=1;
        bind.keyframePoses=&pose;
        UpdateModelAnimation(model,bind,0);
    }
    return false;
}

}
