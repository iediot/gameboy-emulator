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

#endif // GB_ANDROID
