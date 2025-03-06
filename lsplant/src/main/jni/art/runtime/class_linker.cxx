module;

#include <sys/types.h>

#include "logging.hpp"

export module lsplant:class_linker;

import :art_method;
import :thread;
import :common;
import :clazz;
import :handle;
import :runtime;
import hook_helper;

namespace lsplant::art {
export class ClassLinker {
private:
    inline static auto SetEntryPointsToInterpreter_ =
        "_ZNK3art11ClassLinker27SetEntryPointsToInterpreterEPNS_9ArtMethodE"_sym.as<void(ClassLinker::*)(ArtMethod *)>;

    inline static auto ShouldUseInterpreterEntrypoint_ =
        "_ZN3art11ClassLinker30ShouldUseInterpreterEntrypointEPNS_9ArtMethodEPKv"_sym.hook->*[]
        <Backup auto backup>
        (ArtMethod *art_method, const void *quick_code)static -> bool {
            if (quick_code != nullptr && IsHooked(art_method)) [[unlikely]] {
                return false;
            }
            return backup(art_method, quick_code);
        };

    inline static auto art_quick_to_interpreter_bridge_ =
            "art_quick_to_interpreter_bridge"_sym.as<void(void *)>;

    inline static auto GetOptimizedCodeFor_ =
            "_ZN3art15instrumentationL19GetOptimizedCodeForEPNS_9ArtMethodE"_sym.as<void *(ArtMethod *)>;

    inline static auto GetRuntimeQuickGenericJniStub_=
            "_ZNK3art11ClassLinker29GetRuntimeQuickGenericJniStubEv"_sym.as<void *(ClassLinker::*)()>;

    inline static art::ArtMethod *MayGetBackup(art::ArtMethod *method) {
        if (auto backup = IsHooked(method); backup) [[unlikely]] {
            method = backup;
            LOGV("propagate native method: %s", method->PrettyMethod(true).data());
        }
        return method;
    }

    inline static auto RegisterNativeThread_ =
        "_ZN3art6mirror9ArtMethod14RegisterNativeEPNS_6ThreadEPKvb"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, ArtMethod *method, Thread *thread, const void *native_method, bool is_fast) static -> void {
            return backup(thiz, MayGetBackup(method), thread, native_method, is_fast);
        };

    inline static auto UnregisterNativeThread_ =
        "_ZN3art6mirror9ArtMethod16UnregisterNativeEPNS_6ThreadE"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, ArtMethod *method, Thread *thread) static -> void {
            return backup(thiz, MayGetBackup(method), thread);
        };

    inline static auto RegisterNativeFast_ =
        "_ZN3art9ArtMethod14RegisterNativeEPKvb"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, ArtMethod *method, const void *native_method, bool is_fast) static -> void {
            return backup(thiz, MayGetBackup(method), native_method, is_fast);
        };

    inline static auto UnregisterNativeFast_ =
        "_ZN3art9ArtMethod16UnregisterNativeEv"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, ArtMethod *method) static -> void{
            return backup(thiz, MayGetBackup(method));
        };

    inline static auto RegisterNative_ =
        "_ZN3art9ArtMethod14RegisterNativeEPKv"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, ArtMethod *method, const void *native_method) static -> const void * {
            return backup(thiz, MayGetBackup(method), native_method);
        };

    inline static auto UnregisterNative_ =
        "_ZN3art9ArtMethod16UnregisterNativeEv"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, ArtMethod *method) static -> const void * {
            return backup(thiz, MayGetBackup(method));
        };

    inline static auto RegisterNativeClassLinker_ =
        "_ZN3art11ClassLinker14RegisterNativeEPNS_6ThreadEPNS_9ArtMethodEPKv"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, Thread *self, ArtMethod *method, const void *native_method) static -> const void *{
            return backup(thiz, self, MayGetBackup(method), native_method);
        };

    inline static auto UnregisterNativeClassLinker_ =
        "_ZN3art11ClassLinker16UnregisterNativeEPNS_6ThreadEPNS_9ArtMethodE"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, Thread *self, ArtMethod *method) static -> const void * {
            return backup(thiz, self, MayGetBackup(method));
        };

    static void RestoreBackup(const dex::ClassDef *class_def, art::Thread *self) {
        auto methods = mirror::Class::PopBackup(class_def, self);
        for (const auto &[art_method, old_trampoline] : methods) {
            auto new_trampoline = art_method->GetEntryPoint();
            art_method->SetEntryPoint(old_trampoline);
            auto deoptimized = IsDeoptimized(art_method);
            auto backup_method = IsHooked(art_method);
            if (backup_method) {
                // If deoptimized, the backup entrypoint should be already set to interpreter
                if (!deoptimized && new_trampoline != old_trampoline) [[unlikely]] {
                    LOGV("propagate entrypoint for orig %p backup %p", art_method, backup_method);
                    backup_method->SetEntryPoint(new_trampoline);
                }
            } else if (deoptimized) {
                if (new_trampoline != &art_quick_to_interpreter_bridge_ && !art_method->IsNative()) {
                    LOGV("re-deoptimize for %p", art_method);
                    SetEntryPointsToInterpreter(art_method);
                }
            }
        }
    }

    inline static auto FixupStaticTrampolines_ =
        "_ZN3art11ClassLinker22FixupStaticTrampolinesENS_6ObjPtrINS_6mirror5ClassEEE"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, ObjPtr<mirror::Class> mirror_class) static -> void {
            backup(thiz, mirror_class);
            RestoreBackup(mirror_class->GetClassDef(), nullptr);
        };

    inline static auto FixupStaticTrampolinesWithThread_ =
        "_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6ThreadENS_6ObjPtrINS_6mirror5ClassEEE"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, Thread *self, ObjPtr<mirror::Class> mirror_class) static -> void {
            backup(thiz, self, mirror_class);
            RestoreBackup(mirror_class->GetClassDef(), self);
        };

    inline static auto FixupStaticTrampolinesRaw_ =
        "_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6mirror5ClassE"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, mirror::Class *mirror_class)static -> void {
            backup(thiz, mirror_class);
            RestoreBackup(mirror_class->GetClassDef(), nullptr);
        };

    inline static auto AdjustThreadVisibilityCounter_ =
        ("_ZN3art11ClassLinker26VisiblyInitializedCallback29AdjustThreadVisibilityCounterEPNS_6ThreadEi"_sym |
         "_ZN3art11ClassLinker26VisiblyInitializedCallback29AdjustThreadVisibilityCounterEPNS_6ThreadEl"_sym).hook->*[]
         <MemBackup auto backup>
         (ClassLinker *thiz, Thread *self, ssize_t adjustment) static -> void {
            backup(thiz, self, adjustment);
            RestoreBackup(nullptr, self);
        };

    inline static auto MarkVisiblyInitialized_ =
        "_ZN3art11ClassLinker26VisiblyInitializedCallback22MarkVisiblyInitializedEPNS_6ThreadE"_sym.hook->*[]
        <MemBackup auto backup>
        (ClassLinker *thiz, Thread *self) static -> void {
            backup(thiz, self);
            RestoreBackup(nullptr, self);
        };

public:
    static bool Init(JNIEnv *env, const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();

        if (sdk_int >= __ANDROID_API_N__ && sdk_int < __ANDROID_API_T__) {
            handler(ShouldUseInterpreterEntrypoint_);
        }

        if (!handler(FixupStaticTrampolinesWithThread_, FixupStaticTrampolines_,
                          FixupStaticTrampolinesRaw_)) {
            return false;
        }

        if (!handler(RegisterNativeClassLinker_, RegisterNative_, RegisterNativeFast_,
                          RegisterNativeThread_) ||
            !handler(UnregisterNativeClassLinker_, UnregisterNative_, UnregisterNativeFast_,
                          UnregisterNativeThread_)) {
            return false;
        }

        if (sdk_int >= __ANDROID_API_R__) {
            if constexpr (kArch != Arch::kX86 && kArch != Arch::kX86_64) {
                // fixup static trampoline may have been inlined
                handler(AdjustThreadVisibilityCounter_, MarkVisiblyInitialized_);
            }
        }

        if (!handler(SetEntryPointsToInterpreter_)) [[likely]] {
            if (handler(GetOptimizedCodeFor_, true)) [[likely]] {
                auto obj = JNI_FindClass(env, "java/lang/Object");
                if (!obj) {
                    return false;
                }
                auto method = JNI_GetMethodID(env, obj, "equals", "(Ljava/lang/Object;)Z");
                if (!method) {
                    return false;
                }
                auto dummy = ArtMethod::FromReflectedMethod(
                        env, JNI_ToReflectedMethod(env, obj, method, false).get())->Clone();
                JavaDebuggableGuard guard;
                // just in case
                dummy->SetNonNative();
                art_quick_to_interpreter_bridge_ = GetOptimizedCodeFor_(dummy.get());
            } else if (!handler(art_quick_to_interpreter_bridge_)) [[unlikely]] {
                return false;
            }
        }
        LOGD("art_quick_to_interpreter_bridge = %p", &art_quick_to_interpreter_bridge_);
        return true;
    }

    [[gnu::always_inline]] static bool SetEntryPointsToInterpreter(ArtMethod *art_method) {
        if (art_method->IsNative()) {
            return false;
        }
        if (SetEntryPointsToInterpreter_) [[likely]] {
            SetEntryPointsToInterpreter_(nullptr, art_method);
            return true;
        }
        // Android 13
        if (art_quick_to_interpreter_bridge_) [[likely]] {
            LOGV("deoptimize method %s from %p to %p", art_method->PrettyMethod(true).data(),
                 art_method->GetEntryPoint(), &art_quick_to_interpreter_bridge_);
            art_method->SetEntryPoint(
                reinterpret_cast<void *>(&art_quick_to_interpreter_bridge_));
            return true;
        }
        return false;
    }
};
}  // namespace lsplant::art
