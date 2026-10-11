#!/usr/bin/env python3
"""Narrow raylib 6.0 fixes: active Web contacts and correct Android ES3 context."""
from pathlib import Path
import sys

def patch_web(root):
    path=Path(root)/'src/platforms/rcore_web.c'
    source=path.read_text()
    marker='// OUTLAND: compact active contacts; Emscripten includes ended touches.'
    old='    if (eventType == EMSCRIPTEN_EVENT_TOUCHEND)\n    {\n        // Identify the EMSCRIPTEN_EVENT_TOUCHEND'
    def remove_old_release(text):
        start=text.find(old)
        if start<0:return text
        end=text.index('    return 1; // The event was consumed by the callback handler',start)
        return text[:start]+text[end:]
    if marker in source:
        updated=remove_old_release(source)
        if updated!=source:path.write_text(updated)
        return
    start=source.index('    // Register touch points count',source.index('static EM_BOOL EmscriptenTouchCallback(', source.index('// Emscripten: Called on touch input events')))
    end=source.index('    // Update mouse position when single touch detected',start)
    source=source[:start]+'''    // OUTLAND: compact active contacts; Emscripten includes ended touches.
    double canvasWidth = 1.0, canvasHeight = 1.0;
    emscripten_get_element_css_size(platform.canvasId, &canvasWidth, &canvasHeight);
    if (canvasWidth <= 0 || canvasHeight <= 0) return 1;
    CORE.Input.Touch.pointCount = 0;
    for (int i = 0; i < touchEvent->numTouches && CORE.Input.Touch.pointCount < MAX_TOUCH_POINTS; ++i)
    {
        if ((eventType == EMSCRIPTEN_EVENT_TOUCHEND || eventType == EMSCRIPTEN_EVENT_TOUCHCANCEL) && touchEvent->touches[i].isChanged) continue;
        const int slot = CORE.Input.Touch.pointCount++;
        CORE.Input.Touch.pointId[slot] = touchEvent->touches[i].identifier;
        CORE.Input.Touch.position[slot] = (Vector2){
            touchEvent->touches[i].targetX * (float)GetScreenWidth()/(float)canvasWidth,
            touchEvent->touches[i].targetY * (float)GetScreenHeight()/(float)canvasHeight};
        CORE.Input.Touch.currentTouchState[slot] = 1;
    }
    for (int i = CORE.Input.Touch.pointCount; i < MAX_TOUCH_POINTS; ++i)
    {
        CORE.Input.Touch.pointId[i] = -1;
        CORE.Input.Touch.currentTouchState[i] = 0;
    }

''' +source[end:]
    path.write_text(remove_old_release(source))
def patch(root, platform='web'):
    if platform.lower()=='web':return patch_web(root)
    if platform.lower()!='android':raise ValueError('Unsupported platform')
    path=Path(root)/'src/platforms/rcore_android.c'
    source=path.read_text()
    old='EGL_CONTEXT_CLIENT_VERSION, 2,'
    new='EGL_CONTEXT_CLIENT_VERSION, (rlGetVersion() == RL_OPENGL_ES_30)? 3 : 2, // OUTLAND: match requested graphics API'
    if new in source:return
    if source.count(old)!=1:raise ValueError('Unexpected raylib Android context API')
    path.write_text(source.replace(old,new))
if __name__=='__main__':patch(sys.argv[1],sys.argv[2] if len(sys.argv)>2 else 'web')
