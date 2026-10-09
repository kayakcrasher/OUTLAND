// Manual graphics smoke: DISPLAY=:99 outland_asset_model_smoke /path/to/pack [expected_count]
#include "outland/assets/ModelCache.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    assert(argc>=2 && argc<=4);
    const bool allow_untextured=argc==4 && std::string(argv[3])=="--allow-untextured";
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320,240,"OUTLAND model import smoke");
    assert(IsWindowReady());
    outland::assets::ModelCache cache(16);
    int count=0, textured=0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[1])) {
        const auto extension=entry.path().extension();
        if (extension!=".gltf" && extension!=".glb") continue;
        Model* model=cache.load(entry.path().string());
        if (!model) std::cerr << "Failed model: " << entry.path() << '\n';
        assert(model && model->meshCount>0 && model->materialCount>0);
        assert(cache.size()<=16);
        const auto bounds=GetModelBoundingBox(*model);
        assert(std::isfinite(bounds.min.x) && std::isfinite(bounds.min.y) && std::isfinite(bounds.min.z));
        assert(std::isfinite(bounds.max.x) && std::isfinite(bounds.max.y) && std::isfinite(bounds.max.z));
        assert((bounds.max.x-bounds.min.x)+(bounds.max.y-bounds.min.y)+(bounds.max.z-bounds.min.z)>0);
        assert(std::abs(bounds.min.x+bounds.max.x)<.002F && std::abs(bounds.min.z+bounds.max.z)<.002F);
        assert(std::abs(bounds.min.y)<.002F);
        for (int i=0; i<model->meshCount; ++i) assert(model->meshes[i].vertexCount>0);
        bool has_texture=false;
        for (int i=0; i<model->materialCount; ++i) {
            const auto id=model->materials[i].maps[MATERIAL_MAP_ALBEDO].texture.id;
            has_texture |= id!=0 && id!=rlGetTextureIdDefault();
        }
        if (extension==".glb" && !allow_untextured) assert(has_texture);
        textured+=has_texture;
        assert(cache.load(entry.path().string())==model); // Cache hit does not upload again.
        ++count;
    }
    cache.clear();
    assert(cache.size()==0);
    CloseWindow();
    assert(count>0 && (argc<3 || count==std::stoi(argv[2])));
    std::cout << "[PASS] Loaded " << count << " models, " << textured
        << " textured; normalized pivots and bounded GPU cache verified\n";
}
