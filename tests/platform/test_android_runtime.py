#!/usr/bin/env python3
"""Exercise actual Android extraction/save boundary with a headless AssetManager fixture."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
class AndroidRuntime(unittest.TestCase):
    def test_extract_update_cache_and_save_separation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);headers=root/'headers';(headers/'android').mkdir(parents=True)
            (headers/'raylib.h').write_text('''#pragma once
struct Color {unsigned char r,g,b,a;};
constexpr Color BLACK{0,0,0,255},RAYWHITE{245,245,245,255};
bool ChangeDirectory(const char*);double GetTime();bool IsWindowFocused();bool WindowShouldClose();
void BeginDrawing();void ClearBackground(Color);void DrawText(const char*,int,int,int,Color);void EndDrawing();
''')
            (headers/'android/asset_manager.h').write_text('''#pragma once
#include <cstddef>
struct AAssetManager;struct AAsset;
#define AASSET_MODE_BUFFER 1
#define AASSET_MODE_STREAMING 2
AAsset* AAssetManager_open(AAssetManager*,const char*,int);
const void* AAsset_getBuffer(AAsset*);long AAsset_getLength(AAsset*);
void AAsset_close(AAsset*);int AAsset_read(AAsset*,void*,std::size_t);
''')
            (headers/'android_native_app_glue.h').write_text('''#pragma once
struct AAssetManager;
struct ANativeActivity {const char* internalDataPath;AAssetManager* assetManager;};
struct android_app {ANativeActivity* activity;};
''')
            harness=r'''
#include "outland/platform/Runtime.hpp"
#include <raylib.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <algorithm>
namespace fs=std::filesystem;
struct AAssetManager {std::map<std::string,std::string> files;int opens=0;};
struct AAsset {std::string data;std::size_t offset=0;};
AAsset* AAssetManager_open(AAssetManager* manager,const char* path,int){manager->opens++;auto it=manager->files.find(path);return it==manager->files.end()?nullptr:new AAsset{it->second};}
const void* AAsset_getBuffer(AAsset* a){return a->data.data();}long AAsset_getLength(AAsset* a){return a->data.size();}
void AAsset_close(AAsset* a){delete a;}
int AAsset_read(AAsset* a,void* buffer,std::size_t size){const auto n=std::min(size,a->data.size()-a->offset);std::memcpy(buffer,a->data.data()+a->offset,n);a->offset+=n;return n;}
android_app app;extern "C" android_app* GetAndroidApp(){return &app;}
bool ChangeDirectory(const char* path){fs::current_path(path);return true;}
double GetTime(){static double time=0;return time+=.1;}bool IsWindowFocused(){return true;}bool WindowShouldClose(){return false;}
void BeginDrawing(){}void ClearBackground(Color){}void DrawText(const char*,int,int,int,Color){}void EndDrawing(){}
std::string read(const fs::path& path){std::ifstream file(path);return std::string(std::istreambuf_iterator<char>(file),{});}
int main(int argc,char** argv){
 assert(argc==2);const fs::path base=fs::absolute(argv[1]);fs::create_directories(base);
 const std::string base_string=base.string();AAssetManager manager;
 ANativeActivity activity{base_string.c_str(),&manager};app.activity=&activity;
 manager.files={{"runtime.manifest","version1\nassets/model.glb\nmaps/world.map\n"},{"assets/model.glb","production"},{"maps/world.map","world"}};
 assert(outland::platform::initialize());assert(fs::current_path()==base/"runtime");
 assert(read("assets/model.glb")=="production");assert(std::string(std::getenv("OUTLAND_SAVE_DIR"))==(base/"saves").string());
 std::ofstream(base/"saves/player.state")<<"persistent";
 const int opens=manager.opens;assert(outland::platform::initialize());assert(manager.opens==opens+1);
 manager.files["runtime.manifest"]="version2\nassets/model.glb\nmaps/world.map\n";manager.files["assets/model.glb"]="updated";
 assert(outland::platform::initialize());assert(read("assets/model.glb")=="updated");assert(read(base/"saves/player.state")=="persistent");
 manager.files["runtime.manifest"]="version3\nassets/missing.glb\n";
 assert(!outland::platform::initialize());assert(read(base/"runtime/.version")=="version2");
 manager.files["runtime.manifest"]="version4\n../escape\n";
 assert(!outland::platform::initialize());assert(!fs::exists(base/"escape"));assert(read(base/"saves/player.state")=="persistent");
}
'''
            (root/'runtime_test.cpp').write_text(harness)
            flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.getenv('OUTLAND_PLATFORM_SANITIZERS') else []
            subprocess.run(['c++','-std=c++20','-D__ANDROID__',*flags,'-I'+str(headers),'-I'+str(ROOT/'include'),str(ROOT/'src/platform/Runtime.cpp'),str(root/'runtime_test.cpp'),'-o',str(root/'runtime_test')],check=True)
            subprocess.run([str(root/'runtime_test'),str(root/'private-files')],check=True)
if __name__=='__main__':unittest.main()
