#include "outland/platform/Haptics.hpp"
#include <algorithm>
#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include <jni.h>
extern "C" android_app* GetAndroidApp();
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, browser_vibrate, (int ms), {
    try { if (navigator.vibrate) navigator.vibrate(ms); } catch (e) {}
});
#endif

namespace outland::platform {

void vibrate(int milliseconds, float strength) {
    milliseconds = std::clamp(milliseconds, 1, 1000);
    strength = std::clamp(strength, 0.0F, 1.0F);
#ifdef __ANDROID__
    // GameActivity.vibrate(int ms, int amplitude) drives the Vibrator from Java.
    android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return;
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    static jmethodID method = nullptr;
    if (!method) {
        jclass type = env->GetObjectClass(app->activity->clazz);
        method = env->GetMethodID(type, "vibrate", "(II)V");
        env->DeleteLocalRef(type);
        if (env->ExceptionCheck()) { env->ExceptionClear(); method = nullptr; }
    }
    if (method) {
        env->CallVoidMethod(app->activity->clazz, method, static_cast<jint>(milliseconds),
                            static_cast<jint>(std::clamp(static_cast<int>(strength * 255), 1, 255)));
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (attached) vm->DetachCurrentThread();
#elif defined(__EMSCRIPTEN__)
    // No force control in browsers: a weaker buzz is a shorter one.
    browser_vibrate(std::max(1, static_cast<int>(milliseconds * (.45F + .55F * strength))));
#else
    (void)milliseconds;
    (void)strength;
#endif
}

} // namespace outland::platform
