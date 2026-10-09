#include "outland/assets/ModelCache.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
int loads=0, unloads=0, animation_loads=0, animation_unloads=0;
unsigned int next_texture=10;
std::vector<unsigned int> released;
}
extern "C" {
ModelAnimation* LoadModelAnimations(const char* path,int* count){
    ++animation_loads;if(std::strstr(path,"no_clips")){*count=0;return nullptr;}
    *count=1;return new ModelAnimation[1]{};
}
void UnloadModelAnimations(ModelAnimation* clips,int count){assert(count==1);++animation_unloads;delete[] clips;}
void UpdateModelAnimation(Model,ModelAnimation,int){}
const char* GetApplicationDirectory() { return "/mock/"; }
bool FileExists(const char* path) { return std::strstr(path,"missing")==nullptr; }
void TraceLog(int, const char*, ...) {}
unsigned int rlGetTextureIdDefault() { return 1; }
Model LoadModel(const char*) {
    ++loads;
    Model model{}; model.transform=MatrixIdentity();
    model.meshCount=1; model.meshes=new Mesh[1]{};
    model.materialCount=1; model.materials=new Material[1]{};
    model.materials[0].maps=new MaterialMap[MATERIAL_MAP_BRDF+1]{};
    const unsigned int id=next_texture++;
    model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture.id=id;
    model.materials[0].maps[MATERIAL_MAP_NORMAL].texture.id=id; // Shared within model.
    model.materials[0].maps[MATERIAL_MAP_ROUGHNESS].texture.id=1; // raylib default must survive.
    return model;
}
BoundingBox GetModelBoundingBox(Model) { return {{2,-1,3},{4,5,7}}; }
void UnloadTexture(Texture2D texture) { released.push_back(texture.id); }
void UnloadModel(Model model) {
    ++unloads;
    for (int i=0; i<model.materialCount; ++i) delete[] model.materials[i].maps;
    delete[] model.materials; delete[] model.meshes;
}
}

int main() {
    {
        outland::assets::ModelCache cache(2);
        assert(!cache.load("") && !cache.load("missing.glb") && loads==0);
        Model* a=cache.load("a.glb");
        assert(a && cache.size()==1 && loads==1);
        assert(a->transform.m12==-3 && a->transform.m13==1 && a->transform.m14==-5);
        assert(cache.load("a.glb")==a && loads==1);
        assert(cache.load("b.glb") && loads==2);
        assert(cache.load("a.glb")==a); // A is most recently used.
        assert(cache.load("c.glb") && cache.size()==2 && loads==3);
        assert(unloads==1 && released.size()==1 && released[0]==11); // B evicted, once.
        cache.clear();
        assert(cache.size()==0 && unloads==3 && released.size()==3);
        for (auto id : released) assert(id!=1);
    }
    assert(unloads==3); // Destruction after clear never unloads twice.
    {
        outland::assets::ModelCache cache(0); // Minimum usable capacity is one.
        assert(cache.load("d.glb")); assert(cache.load("e.glb"));
        assert(cache.size()==1 && unloads==4);
    }
    assert(unloads==5 && released.size()==5);
    {
        outland::assets::ModelCache cache(1);
        auto* a=cache.load("animated.glb");assert(animation_loads==0 && cache.animations(a).empty());
        assert(cache.load("animated.glb",{},true)==a && animation_loads==1 && cache.animations(a).size()==1);
        cache.mark_posed(a,true);assert(cache.posed(a));
        assert(cache.load("animated.glb",{},true)==a && animation_loads==1);
        auto* b=cache.load("no_clips.glb",{},true);assert(b && animation_loads==2 && animation_unloads==1 && cache.animations(b).empty() && !cache.posed(b));
        assert(cache.load("no_clips.glb",{},true)==b && animation_loads==2); // Failure remembered, no per-frame retry.
        assert(cache.load("other.glb",{},true) && animation_loads==3);
        cache.clear();assert(animation_unloads==2);
    }
    std::cout << "[PASS] GPU cache hits, LRU eviction, texture ownership, ground pivots and cleanup\n";
}
