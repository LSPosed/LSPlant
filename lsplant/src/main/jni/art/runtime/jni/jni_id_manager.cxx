module;

#include "logging.hpp"

export module lsplant:jni_id_manager;

import std;

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
    static bool Init(bool java_debuggable, const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();
        if (sdk_int < kSdkR) {
            return true;
        }
        // Whether FromReflectedMethod goes through JniIdManager::EncodeMethodId is not strictly
        // tied to the java-debuggable flag: vendor ART builds (e.g. Android 16 release builds)
        // keep that path enabled while VMRuntime.isJavaDebuggable() reports false. A backup
        // ArtMethod is a private clone that lives outside its class' method array, so encoding
        // one there computes a bogus method index and SIGSEGVs. Install the remap whenever the
        // symbol resolves instead of trusting the debuggable flag; the hook is never reached on
        // runtimes that hand out plain method pointers.
        const bool hooked_with_class = handler(EncodeGenericIdWithClass_);
        const bool hooked_without_class = handler(EncodeGenericId_);
        if (!hooked_with_class && !hooked_without_class) {
            LOGW("Failed to hook EncodeGenericId, encoding a backup method may crash the process");
            return true;
        }
        LOGD("Hooked EncodeGenericId (with_class=%d, plain=%d, debuggable=%d)", hooked_with_class,
             hooked_without_class, java_debuggable);
        return true;
    }
};

}  // namespace lsplant::art::jni
