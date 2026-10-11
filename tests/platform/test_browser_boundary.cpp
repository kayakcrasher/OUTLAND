#include "outland/platform/Runtime.hpp"
#include <raylib.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
namespace {int width=1280,height=720;}
extern "C" {
bool ChangeDirectory(const char* path){return std::strcmp(path,"/")==0;}
int GetScreenWidth(){return width;}
int GetScreenHeight(){return height;}
void SetWindowSize(int w,int h){width=w;height=h;}
}
int main(){
    assert(outland::platform::initialize());
    assert(std::strcmp(std::getenv("OUTLAND_SAVE_DIR"),"/saves")==0);
    int local_state=7;
    for(int i=0;i<3;++i){
        outland::platform::begin_frame();
        ++local_state;
        assert(width==1248 && height==720);
        assert(outland::platform::save_requested());
        outland::platform::flush_saves();
    }
    assert(local_state==10);
    std::cout<<"Browser boundary: three yielded frames, local state, resize, saves GREEN\n";
}
