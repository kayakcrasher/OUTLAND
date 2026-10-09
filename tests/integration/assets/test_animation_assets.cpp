#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/AnimationController.hpp"
#include "outland/characters/AnimationAssetCatalog.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace outland::characters;
int main() {
    SetTraceLogLevel(LOG_WARNING);
    CharacterRegistry registry;std::string error;
    assert(registry.load(std::string(OUTLAND_SOURCE_DIR)+"/assets/verda/characters/character_manifest.tsv",error));
    int clips_seen=0,bodies=0,mapped_body=0;
    for(const auto& asset:registry.assets()) {
        auto info=std::find_if(animation_asset_catalog.begin(),animation_asset_catalog.end(),[&](const auto& entry){return entry.id==asset.id;});
        assert(info!=animation_asset_catalog.end());
        int count=0;auto* clips=LoadModelAnimations((std::filesystem::path(OUTLAND_SOURCE_DIR)/asset.model_path).c_str(),&count);
        assert(count==info->clips);int mapped=0;
        for(int i=0;i<count;++i) {
            assert(clips && clips[i].boneCount==info->joints && clips[i].frameCount>=2);
            for(int frame=0;frame<clips[i].frameCount;++frame) assert(clips[i].framePoses[frame]);
            if(AnimationController::classify(clips[i].name)!=AnimationAction::None) ++mapped;
            Model rig{};rig.boneCount=clips[i].boneCount;rig.bones=clips[i].bones;rig.bindPose=clips[i].framePoses[0];
            assert(AnimationController::compatible(rig,clips[i]));
        }
        assert(mapped==info->mapped_clips);
        if(asset.pool!=CharacterPool::Arms) {++bodies;mapped_body+=mapped;}
        clips_seen+=count;
        if(clips) UnloadModelAnimations(clips,count);
    }
    assert(bodies==80 && clips_seen==108 && mapped_body==0);
    std::cout<<"[PASS] Real raylib CPU decoder audited all 82 production GLBs and 108 clips without a window\n";
}
