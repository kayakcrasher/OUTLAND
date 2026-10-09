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
    if(model.boneCount<=0 || model.boneCount!=clip.boneCount || clip.frameCount<2 ||
        !model.bones || !model.bindPose || !clip.bones || !clip.framePoses) return false;
    for(int i=0;i<model.boneCount;++i)
        if(model.bones[i].parent!=clip.bones[i].parent || std::strncmp(model.bones[i].name,clip.bones[i].name,32)!=0) return false;
    if(!clip.framePoses[0] || !clip.framePoses[clip.frameCount-1]) return false;
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
    // raylib 5.5 glTF decoder samples every 17ms. Never infer action from duration.
    const double frames=static_cast<double>(elapsed_)*1000.0/17.0;
    const auto selected=classify(clip.name);
    const bool loop=selected!=AnimationAction::Attack && selected!=AnimationAction::Death;
    const int frame=loop ? static_cast<int>(std::fmod(frames,clip.frameCount))
        : static_cast<int>(std::min(frames,static_cast<double>(clip.frameCount-1)));
    return {index,frame,fallback};
}
bool AnimationController::apply_sample(Model& model,std::span<const ModelAnimation> clips,AnimationSample sample,bool was_posed) {
    if(sample.clip>=0 && static_cast<std::size_t>(sample.clip)<clips.size()) {
        const auto& clip=clips[static_cast<std::size_t>(sample.clip)];
        if(compatible(model,clip) && sample.frame>=0 && sample.frame<clip.frameCount && clip.framePoses[sample.frame]) {
            UpdateModelAnimation(model,clip,sample.frame);
            return true;
        }
    }
    if(was_posed && model.bindPose && model.bones && model.boneCount>0) {
        auto* pose=model.bindPose;
        ModelAnimation bind{};bind.boneCount=model.boneCount;bind.frameCount=1;
        bind.bones=model.bones;bind.framePoses=&pose;
        UpdateModelAnimation(model,bind,0);
    }
    return false;
}

}
