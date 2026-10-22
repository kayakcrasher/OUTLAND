#!/usr/bin/env python3
"""Compile the raylib patch's active-contact adapter and exercise real multitouch release cases."""
import importlib.util,subprocess,tempfile,unittest,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('patch',ROOT/'tools/patch_raylib_platform.py')
patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
class WebContacts(unittest.TestCase):
    def test_contacts_release_reorder_cancel_and_css_scale(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'src/platforms').mkdir(parents=True)
            source=root/'src/platforms/rcore_web.c'
            source.write_text('// Emscripten: Called on touch input events\nstatic EM_BOOL EmscriptenTouchCallback(int eventType) {\n    // Register touch points count\n    // upstream replaced body\n    // Update mouse position when single touch detected\n    if (eventType == EMSCRIPTEN_EVENT_TOUCHEND)\n    {\n        // Identify the EMSCRIPTEN_EVENT_TOUCHEND\n        CORE.Input.Touch.pointCount--;\n    }\n    return 1; // The event was consumed by the callback handler\n}\n'+patch.HIDPI_OLD)
            patch.patch(root);first=source.read_text();self.assertNotIn('// Identify the EMSCRIPTEN_EVENT_TOUCHEND',first);patch.patch(root);self.assertEqual(first,source.read_text())
            self.assertIn(patch.HIDPI_MARKER,first);self.assertNotIn(patch.HIDPI_OLD,first)
            body=first[first.index('    // OUTLAND:'):first.index('    // Update mouse')]
            harness='''#include <assert.h>
#define MAX_TOUCH_POINTS 8
#define EMSCRIPTEN_EVENT_TOUCHEND 2
#define EMSCRIPTEN_EVENT_TOUCHCANCEL 3
typedef struct {float x,y;} Vector2;
typedef struct {int identifier,isChanged;float targetX,targetY;} Touch;
typedef struct {int numTouches;Touch touches[8];} Event;
struct {struct {struct {int pointCount,pointId[8],currentTouchState[8];Vector2 position[8];} Touch;} Input;} CORE;
struct {char* canvasId;} platform;
void emscripten_get_element_css_size(char* id,double* w,double* h){*w=640;*h=360;}
int GetScreenWidth(){return 1280;}int GetScreenHeight(){return 720;}
int callback(int eventType,Event* touchEvent){BODY return 0;}
int main(){
 Event e={2,{{11,1,100,50},{22,1,500,200}}};callback(1,&e);
 assert(CORE.Input.Touch.pointCount==2 && CORE.Input.Touch.position[1].x==1000);
 e.touches[0].isChanged=1;e.touches[1].isChanged=0;callback(2,&e);
 assert(CORE.Input.Touch.pointCount==1 && CORE.Input.Touch.pointId[0]==22 && CORE.Input.Touch.pointId[1]==-1);
 e.numTouches=1;e.touches[0]=(Touch){22,1,500,200};callback(3,&e);
 assert(CORE.Input.Touch.pointCount==0 && !CORE.Input.Touch.currentTouchState[0]);
 e.numTouches=2;e.touches[0]=(Touch){33,0,100,50};e.touches[1]=(Touch){44,1,500,200};callback(2,&e);
 assert(CORE.Input.Touch.pointCount==1 && CORE.Input.Touch.pointId[0]==33);
 e.numTouches=0;callback(2,&e);assert(CORE.Input.Touch.pointCount==0);
}
'''.replace('BODY',body)
            (root/'contacts.c').write_text(harness)
            subprocess.run(['cc',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.getenv('OUTLAND_PLATFORM_SANITIZERS') else []),str(root/'contacts.c'),'-o',str(root/'contacts')],check=True)
            subprocess.run([str(root/'contacts')],check=True)
    def test_android_es3_context_matches_graphics_api(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'src/platforms').mkdir(parents=True)
            source=root/'src/platforms/rcore_android.c'
            source.write_text('EGL_CONTEXT_CLIENT_VERSION, 2,')
            patch.patch(root,'Android');first=source.read_text()
            patch.patch(root,'Android');self.assertEqual(first,source.read_text())
            expression=first.split(',')[1].strip()
            harness='#include <assert.h>\n#define RL_OPENGL_ES_30 3\nint version;int rlGetVersion(){return version;}\nint main(){version=3;assert(('+expression+')==3);version=2;assert(('+expression+')==2);}'
            (root/'context.c').write_text(harness)
            subprocess.run(['cc',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.getenv('OUTLAND_PLATFORM_SANITIZERS') else []),str(root/'context.c'),'-o',str(root/'context')],check=True)
            subprocess.run([str(root/'context')],check=True)
if __name__=='__main__':unittest.main()
