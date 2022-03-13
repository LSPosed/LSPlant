#pragma once

#include "art/runtime/art_method.hpp"
#include "art/runtime/obj_ptr.h"
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

    static auto GetBackupMethods(mirror::Class *mirror_class) {
        std::list<std::tuple<art::ArtMethod *, void *>> out;
        auto class_def = mirror_class->GetClassDef();
        if (!class_def) return out;
        std::shared_lock lk(hooked_classes_lock_);
        if (auto found = hooked_classes_.find(class_def); found != hooked_classes_.end()) {
            LOGD("Before fixup %s, backup hooked methods' trampoline",
                 mirror_class->GetDescriptor().c_str());
            for (auto method : found->second) {
                out.emplace_back(method, method->GetEntryPoint());
            }
        }
        return out;
    }

    static void FixTrampoline(const std::list<std::tuple<art::ArtMethod *, void *>> &methods) {
        std::shared_lock lk(hooked_methods_lock_);
        for (const auto &[art_method, old_trampoline] : methods) {
            if (auto found = hooked_methods_.find(art_method); found != hooked_methods_.end()) {
                if (auto new_trampoline = art_method->GetEntryPoint();
                    new_trampoline != old_trampoline) {
                    found->second.second->SetEntryPoint(new_trampoline);
                    art_method->SetEntryPoint(old_trampoline);
                }
            }
        }
    }

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker22FixupStaticTrampolinesENS_6ObjPtrINS_6mirror5ClassEEE", void,
        FixupStaticTrampolines, (ClassLinker * thiz, ObjPtr<mirror::Class> mirror_class), {
            auto backup_methods = GetBackupMethods(mirror_class);
            backup(thiz, mirror_class);
            FixTrampoline(backup_methods);
        });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6ThreadENS_6ObjPtrINS_6mirror5ClassEEE",
        void, FixupStaticTrampolinesWithThread,
        (ClassLinker * thiz, art::Thread *self, ObjPtr<mirror::Class> mirror_class), {
            auto backup_methods = GetBackupMethods(mirror_class);
            backup(thiz, self, mirror_class);
            FixTrampoline(backup_methods);
        });

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6mirror5ClassE",
                               void, FixupStaticTrampolinesRaw,
                               (ClassLinker * thiz, mirror::Class *mirror_class), {
                                   auto backup_methods = GetBackupMethods(mirror_class);
                                   backup(thiz, mirror_class);
                                   FixTrampoline(backup_methods);
                               });

public:
    static bool Init(const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();

        if (sdk_int >= __ANDROID_API_N__) [[likely]] {
            if (!HookSyms(handler, ShouldUseInterpreterEntrypoint, ShouldStayInSwitchInterpreter))
                [[unlikely]] {
                return false;
            }
        }

        if (!HookSyms(handler, FixupStaticTrampolinesWithThread, FixupStaticTrampolines,
                      FixupStaticTrampolinesRaw)) {
            return false;
        }

        if (!RETRIEVE_MEM_FUNC_SYMBOL(
                SetEntryPointsToInterpreter,
                "_ZNK3art11ClassLinker27SetEntryPointsToInterpreterEPNS_9ArtMethodE"))
            [[unlikely]] {
            if (!RETRIEVE_FUNC_SYMBOL(art_quick_to_interpreter_bridge,
                                      "art_quick_to_interpreter_bridge")) [[unlikely]] {
                return false;
            }
            if (!RETRIEVE_FUNC_SYMBOL(art_quick_generic_jni_trampoline,
                                      "art_quick_generic_jni_trampoline")) [[unlikely]] {
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
