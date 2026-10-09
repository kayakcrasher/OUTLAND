#include "outland/assets/ModelCache.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
int loads=0, unloads=0;
unsigned int next_texture=10;
std::vector<unsigned int> released;
}
extern "C" {
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
    std::cout << "[PASS] GPU cache hits, LRU eviction, texture ownership, ground pivots and cleanup\n";
}
