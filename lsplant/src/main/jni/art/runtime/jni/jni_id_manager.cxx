module;

#include "logging.hpp"

export module jni_id_manager;

import art_method;
import common;
import handle;
import hook_helper;

namespace lsplant::art::jni {

export class JniIdManager {
private:
    inline static MemberHooker<
        "_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEmNS_16ReflectiveHandleIT_EE",
        JniIdManager, uintptr_t(ReflectiveHandle<ArtMethod>)>
        EncodeGenericId_ =
            +[](JniIdManager *thiz, ReflectiveHandle<ArtMethod> method) -> uintptr_t {
        if (auto target = IsBackup(method.Get()); target) {
            LOGD("get generic id for %s", method.Get()->PrettyMethod().c_str());
            method.Set(target);
        }
        return EncodeGenericId_(thiz, method);
    };

public:
    static bool Init(JNIEnv *env, const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_R__) {
            if (IsJavaDebuggable(env) && !handler.hook(EncodeGenericId_)) {
                LOGW("Failed to hook EncodeGenericId, attaching debugger may crash the process");
            }
        }
        return true;
    }
};

}  // namespace lsplant::art::jni
