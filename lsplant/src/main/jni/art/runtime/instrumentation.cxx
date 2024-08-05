module;

#include "logging.hpp"

export module instrumentation;

import art_method;
import common;
import hook_helper;

namespace lsplant::art {

export class Instrumentation {
    inline static ArtMethod *MaybeUseBackupMethod(ArtMethod *art_method, const void *quick_code) {
        if (auto backup = IsHooked(art_method); backup && art_method->GetEntryPoint() != quick_code)
            [[unlikely]] {
            LOGD("Propagate update method code %p for hooked method %p to its backup", quick_code,
                 art_method);
            return backup;
        }
        return art_method;
    }

    inline static MemberHooker<
        "_ZN3art15instrumentation15Instrumentation40UpdateMethodsCodeToInterpreterEntryPointEPNS_9ArtMethodE",
        Instrumentation, void(ArtMethod *)>
        UpdateMethodsCodeToInterpreterEntryPoint_ =
            +[](Instrumentation *thiz, ArtMethod *art_method) {
                if (IsDeoptimized(art_method)) {
                    LOGV("skip update entrypoint on deoptimized method %s",
                         art_method->PrettyMethod(true).c_str());
                    return;
                }
                UpdateMethodsCodeToInterpreterEntryPoint_(
                    thiz, MaybeUseBackupMethod(art_method, nullptr));
            };

    inline static MemberHooker<
        "_ZN3art15instrumentation15Instrumentation21InitializeMethodsCodeEPNS_9ArtMethodEPKv",
        Instrumentation, void(ArtMethod *, const void *)>
        InitializeMethodsCode_ = +[](Instrumentation *thiz, ArtMethod *art_method,
                                     const void *quick_code) {
            if (IsDeoptimized(art_method)) {
                LOGV("skip update entrypoint on deoptimized method %s",
                     art_method->PrettyMethod(true).c_str());
                return;
            }
            InitializeMethodsCode_(thiz, MaybeUseBackupMethod(art_method, quick_code), quick_code);
        };

public:
    static bool Init(JNIEnv *env, const HookHandler &handler) {
        if (!IsJavaDebuggable(env)) [[likely]] {
            return true;
        }
        int sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_P__) [[likely]] {
            if (!handler.hook(InitializeMethodsCode_, UpdateMethodsCodeToInterpreterEntryPoint_)) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace lsplant::art
