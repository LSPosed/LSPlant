module;

#include "logging.hpp"

export module lsplant:jni_id_manager;

import :art_method;
import :common;
import :clazz;
import :handle;
import hook_helper;

namespace lsplant::art::jni {

export class JniIdManager {
private:
    inline static auto EncodeGenericId_ =
        ("_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEjNS_16ReflectiveHandleIT_EE"_sym |
         "_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEmNS_16ReflectiveHandleIT_EE"_sym)
            .hook
            ->*[]<MemBackup auto backup>(JniIdManager *thiz,
                                         ReflectiveHandle<ArtMethod> method) static -> uintptr_t {
        if (auto *target = IsBackup(method.Get()); target) {
            LOGD("get generic id for %s", method.Get()->PrettyMethod().c_str());
            method.Set(target);
        }
        return backup(thiz, method);
    };

    inline static auto EncodeGenericIdWithClass_ =
        ("_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEjNS_6HandleINS_6mirror5ClassEEENS_16ReflectiveHandleIT_EE"_sym |
         "_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEmNS_6HandleINS_6mirror5ClassEEENS_16ReflectiveHandleIT_EE"_sym)
            .hook
            ->*[]<MemBackup auto backup>(JniIdManager *thiz, Handle<mirror::Class> klass,
                                         ReflectiveHandle<ArtMethod> method) static -> uintptr_t {
        if (auto *target = IsBackup(method.Get()); target) {
            LOGD("get generic id for %s", method.Get()->PrettyMethod().c_str());
            method.Set(target);
        }
        return backup(thiz, klass, method);
    };

public:
    static bool Init(JNIEnv *env, const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();
        if (sdk_int >= kSdkR) {
            if (IsJavaDebuggable(env) && !handler(EncodeGenericIdWithClass_, EncodeGenericId_)) {
                LOGW("Failed to hook EncodeGenericId, attaching debugger may crash the process");
            }
        }
        return true;
    }
};

}  // namespace lsplant::art::jni
