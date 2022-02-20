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

    [[gnu::always_inline]] static void MaybeDelayHook(mirror::Class *clazz) {
        const auto *class_def = clazz->GetClassDef();
        bool should_intercept = class_def && IsPending(class_def);
        if (should_intercept) [[unlikely]] {
            LOGD("Pending hook for %p (%s)", clazz, clazz->GetDescriptor().c_str());
            OnPending(class_def);
        }
    }

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker22FixupStaticTrampolinesENS_6ObjPtrINS_6mirror5ClassEEE", void,
        FixupStaticTrampolines, (ClassLinker * thiz, mirror::Class *clazz), {
            backup(thiz, clazz);
            MaybeDelayHook(clazz);
        });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6ThreadENS_6ObjPtrINS_6mirror5ClassEEE",
        void, FixupStaticTrampolinesWithThread,
        (ClassLinker * thiz, Thread *self, mirror::Class *clazz), {
            backup(thiz, self, clazz);
            MaybeDelayHook(clazz);
        });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker20MarkClassInitializedEPNS_6ThreadENS_6HandleINS_6mirror5ClassEEE",
        void *, MarkClassInitialized, (ClassLinker * thiz, Thread *self, uint32_t *clazz_ptr), {
            void *result = backup(thiz, self, clazz_ptr);
            auto clazz = reinterpret_cast<mirror::Class *>(*clazz_ptr);
            MaybeDelayHook(clazz);
            return result;
        });

    CREATE_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker30ShouldUseInterpreterEntrypointEPNS_9ArtMethodEPKv", bool,
        ShouldUseInterpreterEntrypoint, (ArtMethod * art_method, const void *quick_code), {
            if (quick_code != nullptr && (IsHooked(art_method) || IsPending(art_method)))
                [[unlikely]] {
                return false;
            }
            return backup(art_method, quick_code);
        });

    CREATE_FUNC_SYMBOL_ENTRY(void, art_quick_to_interpreter_bridge, void *) {}

    CREATE_FUNC_SYMBOL_ENTRY(void, art_quick_generic_jni_trampoline, void *) {}

    CREATE_HOOK_STUB_ENTRY("_ZN3art11interpreter29ShouldStayInSwitchInterpreterEPNS_9ArtMethodE",
                           bool, ShouldStayInSwitchInterpreter, (ArtMethod * art_method), {
                               if (IsHooked(art_method) || IsPending(art_method)) [[unlikely]] {
                                   return false;
                               }
                               return backup(art_method);
                           });

public:
    static bool Init(const HookHandler &handler) {
        int api_level = GetAndroidApiLevel();

        if (!RETRIEVE_MEM_FUNC_SYMBOL(
                SetEntryPointsToInterpreter,
                "_ZNK3art11ClassLinker27SetEntryPointsToInterpreterEPNS_9ArtMethodE")) {
            return false;
        }

        if (!HookSyms(handler, ShouldUseInterpreterEntrypoint, ShouldStayInSwitchInterpreter)) {
            return false;
        }

        if (api_level >= __ANDROID_API_R__) {
            // In android R, FixupStaticTrampolines won't be called unless it's marking it as
            // visiblyInitialized.
            // So we miss some calls between initialized and visiblyInitialized.
            // Therefore we hook the new introduced MarkClassInitialized instead
            // This only happens on non-x86 devices
            if (!HookSyms(handler, MarkClassInitialized) ||
                !HookSyms(handler, FixupStaticTrampolinesWithThread, FixupStaticTrampolines)) {
                return false;
            }
        } else {
            if (!HookSyms(handler, FixupStaticTrampolines)) {
                return false;
            }
        }

        // MakeInitializedClassesVisiblyInitialized will cause deadlock
        // IsQuickToInterpreterBridge is inlined
        // So we use GetSavedEntryPointOfPreCompiledMethod instead

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
        return true;
    }

    [[gnu::always_inline]] static bool SetEntryPointsToInterpreter(ArtMethod *art_method) {
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
        if (SetEntryPointsToInterpreterSym) [[likely]] {
            SetEntryPointsToInterpreter(nullptr, art_method);
            return true;
        }
        return false;
    }
};
}  // namespace lsplant::art
