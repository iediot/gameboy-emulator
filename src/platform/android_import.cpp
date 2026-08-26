//
// android bridge to the storage access framework so roms can be imported into the
// writable folder, the whole file compiles to nothing off android
//

#include "platform.h"
#if GB_ANDROID

#include <jni.h>
#include <atomic>
#include <SDL.h>

static std::atomic<bool> g_import_done{false};

extern "C" JNIEXPORT void JNICALL
Java_com_iediot_gbemu_GBImport_nativeImportDone(JNIEnv*, jclass) {
    g_import_done.store(true);
}

extern "C" void gb_present_document_picker(const char* destDir) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;

    jclass cls = env->FindClass("com/iediot/gbemu/GBImport");
    if (cls) {
        jmethodID mid = env->GetStaticMethodID(cls, "present",
                                               "(Landroid/app/Activity;Ljava/lang/String;)V");
        if (mid) {
            jstring dest = env->NewStringUTF(destDir);
            env->CallStaticVoidMethod(cls, mid, activity, dest);
            env->DeleteLocalRef(dest);
        }
        env->DeleteLocalRef(cls);
    }
    env->DeleteLocalRef(activity);
}

extern "C" bool gb_take_import_done() {
    return g_import_done.exchange(false);
}


// Configuration.uiMode & UI_MODE_NIGHT_MASK == UI_MODE_NIGHT_YES
bool gb_system_dark() {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity)
        return true;

    bool dark = true;
    jclass ctx = env->GetObjectClass(activity);
    jmethodID getRes = env->GetMethodID(ctx, "getResources", "()Landroid/content/res/Resources;");
    if (getRes) {
        jobject res = env->CallObjectMethod(activity, getRes);
        if (res) {
            jclass rcls = env->GetObjectClass(res);
            jmethodID getCfg = env->GetMethodID(rcls, "getConfiguration",
                                                "()Landroid/content/res/Configuration;");
            if (getCfg) {
                jobject cfg = env->CallObjectMethod(res, getCfg);
                if (cfg) {
                    jclass ccls = env->GetObjectClass(cfg);
                    jfieldID f = env->GetFieldID(ccls, "uiMode", "I");
                    if (f)
                        dark = (env->GetIntField(cfg, f) & 0x30) == 0x20;
                    env->DeleteLocalRef(cfg);
                    env->DeleteLocalRef(ccls);
                }
            }
            env->DeleteLocalRef(rcls);
            env->DeleteLocalRef(res);
        }
    }
    env->DeleteLocalRef(ctx);
    env->DeleteLocalRef(activity);
    return dark;
}

#endif // GB_ANDROID
