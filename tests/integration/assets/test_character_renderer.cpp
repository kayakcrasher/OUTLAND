// Headless renderer verification: mock GPU entry points, exercise real registry/cache/preparation.
#include "outland/characters/CharacterRenderer.hpp"
#include "raylib_animation_frame.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace {
int loads=0, unloads=0, draws=0, missing_checks=0;
unsigned next_texture=10;
std::unordered_map<float*,std::string> paths;
std::unordered_set<unsigned> released;
float drawn_yaw=0;
Vector3 drawn_feet{};
}
extern "C" {
ModelAnimation* LoadModelAnimations(const char*,int* count){*count=0;return nullptr;}
void UnloadModelAnimations(ModelAnimation*,int){}
void UpdateModelAnimation(Model,ModelAnimation,outland::test::AnimationFrame){}
const char* GetApplicationDirectory(){return "/mock/";}
bool FileExists(const char* path){
    if (std::strstr(path,"Character_03.glb")){++missing_checks;return false;}
    return true;
}
void TraceLog(int,const char*,...){}
unsigned rlGetTextureIdDefault(){return 1;}
Model LoadModel(const char* path){
    ++loads;
    Model result{};result.transform=MatrixIdentity();
    result.meshCount=1;result.meshes=new Mesh[1]{};
    result.meshes[0].vertexCount=2;
    const bool psx=std::strstr(path,"/psx/")!=nullptr;
    result.meshes[0].vertices=psx ? new float[6]{-.005F,-.008F,0,.005F,.008F,.046F}
        : new float[6]{-.8F,0,-.2F,.8F,2.79F,.2F};
    if (std::strstr(path,"Character_02.glb")) result.meshes[0].vertices[5]=0; // Degenerate body height.
    paths[result.meshes[0].vertices]=path;
    result.materialCount=1;result.materials=new Material[1]{};
    result.materials[0].maps=new MaterialMap[MATERIAL_MAP_BRDF+1]{};
    const auto texture=next_texture++;
    result.materials[0].maps[MATERIAL_MAP_ALBEDO].texture.id=texture;
    result.materials[0].maps[MATERIAL_MAP_NORMAL].texture.id=texture;
    return result;
}
BoundingBox GetModelBoundingBox(Model model){
    auto* v=model.meshes[0].vertices;
    // Match raylib's two-corner implementation so our rotation-safe utility is exercised.
    return {Vector3Transform({v[0],v[1],v[2]},model.transform),
        Vector3Transform({v[3],v[4],v[5]},model.transform)};
}
void UnloadTexture(Texture2D texture){assert(texture.id!=1 && released.insert(texture.id).second);}
void UnloadModel(Model model){
    ++unloads;
    paths.erase(model.meshes[0].vertices);
    delete[] model.meshes[0].vertices;delete[] model.meshes;
    delete[] model.materials[0].maps;delete[] model.materials;
}
void DrawModelEx(Model model,Vector3 feet,Vector3,float yaw,Vector3 scale,Color tint){
    ++draws;drawn_yaw=yaw;drawn_feet=feet;
    const auto bounds=outland::assets::transformed_model_bounds(model);
    assert(bounds.max.x>=bounds.min.x && bounds.max.z>=bounds.min.z);
    assert(std::abs(bounds.min.y)<.0001F);
    assert(std::abs(bounds.max.y-1.85F)<.0001F);
    assert(std::abs(bounds.min.x+bounds.max.x)<.0001F);
    assert(std::abs(bounds.min.z+bounds.max.z)<.0001F);
    assert(scale.x==1 && scale.y==1 && scale.z==1);
    assert(tint.r==255 && tint.g==255 && tint.b==255);
}
}
int main(){
    using namespace outland::characters;
    CharacterRegistry registry;std::string error;
    assert(registry.load(std::string(OUTLAND_SOURCE_DIR)+"/assets/verda/characters/character_manifest.tsv",error));
    {
        CharacterRenderer renderer(registry,"/mock");
        renderer.draw_player({10,2,30},0);
        assert(loads==1 && draws==1 && drawn_yaw==-90 && drawn_feet.x==10 && drawn_feet.y==2);
        renderer.draw_player({10,2,30},PI*.5F);
        assert(loads==1 && draws==2 && std::abs(drawn_yaw)<.0001F); // No second upload/preparation.
        renderer.draw("rebel_modern_rebel_soldier_character",{0,0,0},180);
        assert(loads==2 && draws==3 && drawn_yaw==180);
        renderer.draw("arms_arms_rig",{0,0,0},0);
        renderer.draw("unknown",{0,0,0},0);
        assert(loads==2 && draws==3); // Arm rigs never become NPC/player bodies.
        renderer.draw("character_02",{0,0,0},0);
        renderer.draw("character_02",{0,0,0},0);
        assert(loads==3 && unloads==1 && draws==3); // Invalid body released and failure suppressed.
        renderer.draw("character_03",{0,0,0},0);
        renderer.draw("character_03",{0,0,0},0);
        assert(loads==3 && missing_checks==2 && draws==3); // One cache attempt (package + cwd).
        renderer.draw("character_01",{0,0,0},NAN);
        renderer.draw("character_01",{0,0,0},0,-1);
        renderer.draw("character_01",{NAN,0,0},0);
        assert(draws==3);
        for(const auto& asset:registry.assets()) if(asset.role=="npc") renderer.draw(asset.id,{0,0,0},0);
        assert(loads>64 && unloads>1); // Real LRU eviction path across production body pool.
        renderer.draw_player({0,0,0},0); // Reload after eviction still prepares upright body.
    }
    assert(paths.empty() && loads==unloads && released.size()==static_cast<std::size_t>(loads));
    std::cout<<"[PASS] Headless player/NPC model preparation, facing, texture ownership, failure handling and eviction\n";
}
