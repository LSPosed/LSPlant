module;

#include "logging.hpp"

export module lsplant:instrumentation;

import :art_method;
import :common;
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

    inline static auto UpdateMethodsCodeToInterpreterEntryPoint_ =
        "_ZN3art15instrumentation15Instrumentation40UpdateMethodsCodeToInterpreterEntryPointEPNS_9ArtMethodE"_sym.hook->*[]
        <MemBackup auto backup>
        (Instrumentation *thiz, ArtMethod *art_method) static -> void {
            if (IsDeoptimized(art_method)) {
                LOGV("skip update entrypoint on deoptimized method %s",
                     art_method->PrettyMethod(true).c_str());
                return;
            }
            backup(thiz, MaybeUseBackupMethod(art_method, nullptr));
        };

    inline static auto InitializeMethodsCode_ =
        "_ZN3art15instrumentation15Instrumentation21InitializeMethodsCodeEPNS_9ArtMethodEPKv"_sym.hook->*[]
         <MemBackup auto backup>
         (Instrumentation *thiz, ArtMethod *art_method, const void *quick_code) static -> void {
            if (IsDeoptimized(art_method)) {
                LOGV("skip update entrypoint on deoptimized method %s",
                     art_method->PrettyMethod(true).c_str());
                return;
            }
            backup(thiz, MaybeUseBackupMethod(art_method, quick_code), quick_code);
        };

    inline static auto ReinitializeMethodsCode_ =
        "_ZN3art15instrumentation15Instrumentation23ReinitializeMethodsCodeEPNS_9ArtMethodE"_sym.hook->*[]
         <MemBackup auto backup>
         (Instrumentation *thiz, ArtMethod *art_method) static -> void {
            if (IsDeoptimized(art_method)) {
                LOGV("skip update entrypoint on deoptimized method %s",
                     art_method->PrettyMethod(true).c_str());
                return;
            }
            backup(thiz, MaybeUseBackupMethod(art_method, nullptr));
        };

public:
    static bool Init(JNIEnv *env, const HookHandler &handler) {
        if (!IsJavaDebuggable(env)) [[likely]] {
            return true;
        }
        int sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_P__) [[likely]] {
            if (!handler(ReinitializeMethodsCode_, InitializeMethodsCode_, UpdateMethodsCodeToInterpreterEntryPoint_)) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace lsplant::art
