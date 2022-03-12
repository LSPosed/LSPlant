#pragma once

#include "common.hpp"
#include "runtime/art_method.hpp"

namespace lsplant::art {

class Instrumentation {
    inline static ArtMethod *MaybeUseBackupMethod(ArtMethod *art_method, const void *quick_code) {
        std::shared_lock lk(hooked_methods_lock_);
        if (auto found = hooked_methods_.find(art_method);
            found != hooked_methods_.end() && art_method->GetEntryPoint() != quick_code)
            [[unlikely]] {
            LOGD("Propagate update method code %p for hooked method %s to its backup", quick_code,
                 art_method->PrettyMethod().c_str());
            return found->second.second;
        }
        return art_method;
    }

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art15instrumentation15Instrumentation21UpdateMethodsCodeImplEPNS_9ArtMethodEPKv", void,
        UpdateMethodsCodeImpl,
        (Instrumentation * thiz, ArtMethod *art_method, const void *quick_code),
        { backup(thiz, MaybeUseBackupMethod(art_method, quick_code), quick_code); });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art15instrumentation15Instrumentation17UpdateMethodsCodeEPNS_9ArtMethodEPKv", void,
        UpdateMethodsCode, (Instrumentation * thiz, ArtMethod *art_method, const void *quick_code),
        {
            if (UpdateMethodsCodeImpl.backup) {
                UpdateMethodsCodeImpl.backup(thiz, MaybeUseBackupMethod(art_method, quick_code),
                                             quick_code);
            } else {
                backup(thiz, MaybeUseBackupMethod(art_method, quick_code), quick_code);
            }
        });
    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art15instrumentation15Instrumentation17UpdateMethodsCodeEPNS_6mirror9ArtMethodEPKvS6_b",
        void, UpdateMethodsCodeWithProtableCode,
        (Instrumentation * thiz, ArtMethod *art_method, const void *quick_code,
         const void *portable_code, bool have_portable_code),
        {
            backup(thiz, MaybeUseBackupMethod(art_method, quick_code), quick_code, portable_code,
                   have_portable_code);
        });

public:
    static bool Init(const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();
        if (sdk_int < __ANDROID_API_M__) [[unlikely]] {
            if (!HookSyms(handler, UpdateMethodsCodeWithProtableCode)) {
                return false;
            }
            return true;
        }
        if (!HookSyms(handler, UpdateMethodsCode)) {
            return false;
        }
        if (sdk_int >= __ANDROID_API_N__) [[likely]] {
            if (!HookSyms(handler, UpdateMethodsCodeImpl)) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace lsplant::art
