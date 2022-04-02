#pragma once

#include "art/runtime/art_method.hpp"
#include "art/runtime/reflective_handle.hpp"
#include "common.hpp"

// _ZN3art3jni12JniIdManager14EncodeMethodIdEPNS_9ArtMethodE
namespace lsplant::art::jni {

class JniIdManager {
private:
    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEmNS_16ReflectiveHandleIT_EE",
        uintptr_t, EncodeGenericId, (JniIdManager * thiz, ReflectiveHandle<ArtMethod> method), {
            if (auto target = IsBackup(method.Get()); target) {
                LOGD("get generic id for %s", method.Get()->PrettyMethod().c_str());
                method.Set(target);
            }
            return backup(thiz, method);
        });

public:
    static bool Init(JNIEnv *env, const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_R__) {
            auto runtime_class = JNI_FindClass(env, "dalvik/system/VMRuntime");
            if (!runtime_class) {
                LOGE("Failed to find VMRuntime");
                return false;
            }
            auto get_runtime_method = JNI_GetStaticMethodID(env, runtime_class, "getRuntime",
                                                            "()Ldalvik/system/VMRuntime;");
            if (!get_runtime_method) {
                LOGE("Failed to find VMRuntime.getRuntime()");
                return false;
            }
            auto is_debuggable_method =
                JNI_GetMethodID(env, runtime_class, "isJavaDebuggable", "()Z");
            if (!is_debuggable_method) {
                LOGE("Failed to find VMRuntime.isJavaDebuggable()");
                return false;
            }
            auto runtime = JNI_CallStaticObjectMethod(env, runtime_class, get_runtime_method);
            if (!runtime) {
                LOGE("Failed to get VMRuntime");
                return false;
            }
            auto is_debuggable = JNI_CallBooleanMethod(env, runtime, is_debuggable_method);

            LOGD("java runtime debuggable %s", is_debuggable ? "true" : "false");

            if (is_debuggable && !HookSyms(handler, EncodeGenericId)) {
                LOGW("Failed to hook EncodeGenericId, attaching debugger may crash the process");
            }
        }
        return true;
    }
};

}  // namespace lsplant::art::jni
