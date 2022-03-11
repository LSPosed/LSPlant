#pragma once

#include "art/mirror/class.hpp"
#include "art/runtime/art_method.hpp"
#include "art/thread.hpp"
#include "common.hpp"

namespace lsplant::art {
class ClassLinker {
private:
    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, SetEntryPointsToInterpreter, ClassLinker *thiz,
                                 ArtMethod *art_method) {
        if (SetEntryPointsToInterpreterSym) [[likely]] {
            SetEntryPointsToInterpreterSym(thiz, art_method);
        }
    }

    CREATE_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker30ShouldUseInterpreterEntrypointEPNS_9ArtMethodEPKv", bool,
        ShouldUseInterpreterEntrypoint, (ArtMethod * art_method, const void *quick_code), {
            if (quick_code != nullptr && IsHooked(art_method)) [[unlikely]] {
                return false;
            }
            return backup(art_method, quick_code);
        });

    CREATE_FUNC_SYMBOL_ENTRY(void, art_quick_to_interpreter_bridge, void *) {}

    CREATE_FUNC_SYMBOL_ENTRY(void, art_quick_generic_jni_trampoline, void *) {}

    CREATE_HOOK_STUB_ENTRY("_ZN3art11interpreter29ShouldStayInSwitchInterpreterEPNS_9ArtMethodE",
                           bool, ShouldStayInSwitchInterpreter, (ArtMethod * art_method), {
                               if (IsHooked(art_method)) [[unlikely]] {
                                   return false;
                               }
                               return backup(art_method);
                           });

public:
    static bool Init(const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();

        if (sdk_int >= __ANDROID_API_N__) {
            if (!HookSyms(handler, ShouldUseInterpreterEntrypoint, ShouldStayInSwitchInterpreter))
                [[unlikely]] {
                return false;
            }
        }

        if (!RETRIEVE_MEM_FUNC_SYMBOL(
                SetEntryPointsToInterpreter,
                "_ZNK3art11ClassLinker27SetEntryPointsToInterpreterEPNS_9ArtMethodE")) {
            if (!RETRIEVE_FUNC_SYMBOL(art_quick_to_interpreter_bridge,
                                      "art_quick_to_interpreter_bridge")) {
                return false;
            }
            if (!RETRIEVE_FUNC_SYMBOL(art_quick_generic_jni_trampoline,
                                      "art_quick_generic_jni_trampoline")) {
                return false;
            }
            LOGD("art_quick_to_interpreter_bridge = %p", art_quick_to_interpreter_bridgeSym);
            LOGD("art_quick_generic_jni_trampoline = %p", art_quick_generic_jni_trampolineSym);
        }
        return true;
    }

    [[gnu::always_inline]] static bool SetEntryPointsToInterpreter(ArtMethod *art_method) {
        if (SetEntryPointsToInterpreterSym) [[likely]] {
            SetEntryPointsToInterpreter(nullptr, art_method);
            return true;
        }
        // Android 13
        if (art_quick_to_interpreter_bridgeSym && art_quick_generic_jni_trampolineSym) [[likely]] {
            if (art_method->GetAccessFlags() & ArtMethod::kAccNative) [[unlikely]] {
                art_method->SetEntryPoint(
                    reinterpret_cast<void *>(art_quick_generic_jni_trampolineSym));
            } else {
                art_method->SetEntryPoint(
                    reinterpret_cast<void *>(art_quick_to_interpreter_bridgeSym));
            }
            return true;
        }
        return false;
    }
};
}  // namespace lsplant::art
