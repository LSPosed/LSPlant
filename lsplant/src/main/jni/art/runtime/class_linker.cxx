module;

#include <sys/types.h>

#include "logging.hpp"

export module class_linker;

import art_method;
import thread;
import common;
import clazz;
import handle;
import hook_helper;

namespace lsplant::art {
export class ClassLinker {
private:
    inline static MemberFunction<
        "_ZNK3art11ClassLinker27SetEntryPointsToInterpreterEPNS_9ArtMethodE", ClassLinker,
        void(ArtMethod *)>
        SetEntryPointsToInterpreter_;

    inline static Hooker<"_ZN3art11ClassLinker30ShouldUseInterpreterEntrypointEPNS_9ArtMethodEPKv",
                         bool(ArtMethod *, const void *)>
        ShouldUseInterpreterEntrypoint_ = +[](ArtMethod *art_method, const void *quick_code) {
            if (quick_code != nullptr && IsHooked(art_method)) [[unlikely]] {
                return false;
            }
            return ShouldUseInterpreterEntrypoint_(art_method, quick_code);
        };

    inline static Function<"art_quick_to_interpreter_bridge", void(void *)>
        art_quick_to_interpreter_bridge_;
    inline static Function<"art_quick_generic_jni_trampoline", void(void *)>
        art_quick_generic_jni_trampoline_;

    inline static art::ArtMethod *MayGetBackup(art::ArtMethod *method) {
        if (auto backup = IsHooked(method); backup) [[unlikely]] {
            method = backup;
            LOGV("propagate native method: %s", method->PrettyMethod(true).data());
        }
        return method;
    }

    inline static MemberHooker<"_ZN3art6mirror9ArtMethod14RegisterNativeEPNS_6ThreadEPKvb",
                               ClassLinker, void(ArtMethod *, Thread *, const void *, bool)>
        RegisterNativeThread_ = +[](ClassLinker *thiz, ArtMethod *method, Thread *thread,
                                    const void *native_method, bool is_fast) {
            return RegisterNativeThread_(thiz, MayGetBackup(method), thread, native_method,
                                         is_fast);
        };

    inline static MemberHooker<"_ZN3art6mirror9ArtMethod16UnregisterNativeEPNS_6ThreadE",
                               ClassLinker, void(ArtMethod *, Thread *)>
        UnregisterNativeThread_ = +[](ClassLinker *thiz, ArtMethod *method, Thread *thread) {
            return UnregisterNativeThread_(thiz, MayGetBackup(method), thread);
        };

    inline static MemberHooker<"_ZN3art9ArtMethod14RegisterNativeEPKvb", ClassLinker,
                               void(ArtMethod *, const void *, bool)>
        RegisterNativeFast_ =
            +[](ClassLinker *thiz, ArtMethod *method, const void *native_method, bool is_fast) {
                return RegisterNativeFast_(thiz, MayGetBackup(method), native_method, is_fast);
            };

    inline static MemberHooker<"_ZN3art9ArtMethod16UnregisterNativeEv", ClassLinker,
                               void(ArtMethod *)>
        UnregisterNativeFast_ = +[](ClassLinker *thiz, ArtMethod *method) {
            return UnregisterNativeFast_(thiz, MayGetBackup(method));
        };

    inline static MemberHooker<"_ZN3art9ArtMethod14RegisterNativeEPKv", ClassLinker,
                               const void *(ArtMethod *, const void *)>
        RegisterNative_ = +[](ClassLinker *thiz, ArtMethod *method, const void *native_method) {
            return RegisterNative_(thiz, MayGetBackup(method), native_method);
        };

    inline static MemberHooker<"_ZN3art9ArtMethod16UnregisterNativeEv", ClassLinker,
                               const void *(ArtMethod *)>
        UnregisterNative_ = +[](ClassLinker *thiz, ArtMethod *method) {
            return UnregisterNative_(thiz, MayGetBackup(method));
        };

    inline static MemberHooker<
        "_ZN3art11ClassLinker14RegisterNativeEPNS_6ThreadEPNS_9ArtMethodEPKv", ClassLinker,
        const void *(Thread *, ArtMethod *, const void *)>
        RegisterNativeClassLinker_ =
            +[](ClassLinker *thiz, Thread *self, ArtMethod *method, const void *native_method) {
                return RegisterNativeClassLinker_(thiz, self, MayGetBackup(method), native_method);
            };

    inline static MemberHooker<"_ZN3art11ClassLinker16UnregisterNativeEPNS_6ThreadEPNS_9ArtMethodE",
                               ClassLinker, const void *(Thread *, ArtMethod *)>
        UnregisterNativeClassLinker_ = +[](ClassLinker *thiz, Thread *self, ArtMethod *method) {
            return UnregisterNativeClassLinker_(thiz, self, MayGetBackup(method));
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
                if (new_trampoline != &art_quick_to_interpreter_bridge_ &&
                    new_trampoline != &art_quick_generic_jni_trampoline_) {
                    LOGV("re-deoptimize for %p", art_method);
                    SetEntryPointsToInterpreter(art_method);
                }
            }
        }
    }

    inline static MemberHooker<
        "_ZN3art11ClassLinker22FixupStaticTrampolinesENS_6ObjPtrINS_6mirror5ClassEEE", ClassLinker,
        void(ObjPtr<mirror::Class>)>
        FixupStaticTrampolines_ = +[](ClassLinker *thiz, ObjPtr<mirror::Class> mirror_class) {
            FixupStaticTrampolines_(thiz, mirror_class);
            RestoreBackup(mirror_class->GetClassDef(), nullptr);
        };

    inline static MemberHooker<
        "_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6ThreadENS_6ObjPtrINS_6mirror5ClassEEE",
        ClassLinker, void(Thread *, ObjPtr<mirror::Class>)>
        FixupStaticTrampolinesWithThread_ =
            +[](ClassLinker *thiz, Thread *self, ObjPtr<mirror::Class> mirror_class) {
                FixupStaticTrampolinesWithThread_(thiz, self, mirror_class);
                RestoreBackup(mirror_class->GetClassDef(), self);
            };

    inline static MemberHooker<"_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6mirror5ClassE",
                               ClassLinker, void(mirror::Class *)>
        FixupStaticTrampolinesRaw_ = +[](ClassLinker *thiz, mirror::Class *mirror_class) {
            FixupStaticTrampolinesRaw_(thiz, mirror_class);
            RestoreBackup(mirror_class->GetClassDef(), nullptr);
        };

    inline static MemberHooker<
        {"_ZN3art11ClassLinker26VisiblyInitializedCallback29AdjustThreadVisibilityCounterEPNS_6ThreadEi",
         "_ZN3art11ClassLinker26VisiblyInitializedCallback29AdjustThreadVisibilityCounterEPNS_6ThreadEl"},
        ClassLinker, void(Thread *, ssize_t)>
        AdjustThreadVisibilityCounter_ = +[](ClassLinker *thiz, Thread *self, ssize_t adjustment) {
            AdjustThreadVisibilityCounter_(thiz, self, adjustment);
            RestoreBackup(nullptr, self);
        };

    inline static MemberHooker<
        "_ZN3art11ClassLinker26VisiblyInitializedCallback22MarkVisiblyInitializedEPNS_6ThreadE",
        ClassLinker, void(Thread *)>
        MarkVisiblyInitialized_ = +[](ClassLinker *thiz, Thread *self) {
            MarkVisiblyInitialized_(thiz, self);
            RestoreBackup(nullptr, self);
        };

public:
    static bool Init(const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();

        if (sdk_int >= __ANDROID_API_N__ && sdk_int < __ANDROID_API_T__) {
            handler.hook(ShouldUseInterpreterEntrypoint_);
        }

        if (!handler.hook(FixupStaticTrampolinesWithThread_, FixupStaticTrampolines_,
                          FixupStaticTrampolinesRaw_)) {
            return false;
        }

        if (!handler.hook(RegisterNativeClassLinker_, RegisterNative_, RegisterNativeFast_,
                          RegisterNativeThread_) ||
            !handler.hook(UnregisterNativeClassLinker_, UnregisterNative_, UnregisterNativeFast_,
                          UnregisterNativeThread_)) {
            return false;
        }

        if (sdk_int >= __ANDROID_API_R__) {
            if constexpr (kArch != Arch::kX86 && kArch != Arch::kX86_64) {
                // fixup static trampoline may have been inlined
                handler.hook(AdjustThreadVisibilityCounter_, MarkVisiblyInitialized_);
            }
        }

        if (!handler.dlsym(SetEntryPointsToInterpreter_)) [[unlikely]] {
            if (!handler.dlsym(art_quick_to_interpreter_bridge_)) [[unlikely]] {
                return false;
            }
            if (!handler.dlsym(art_quick_generic_jni_trampoline_)) [[unlikely]] {
                return false;
            }
            LOGD("art_quick_to_interpreter_bridge = %p", &art_quick_to_interpreter_bridge_);
            LOGD("art_quick_generic_jni_trampoline = %p", &art_quick_generic_jni_trampoline_);
        }
        return true;
    }

    [[gnu::always_inline]] static bool SetEntryPointsToInterpreter(ArtMethod *art_method) {
        if (SetEntryPointsToInterpreter_) [[likely]] {
            SetEntryPointsToInterpreter_(nullptr, art_method);
            return true;
        }
        // Android 13
        if (art_quick_to_interpreter_bridge_ && art_quick_generic_jni_trampoline_) [[likely]] {
            if (art_method->GetAccessFlags() & ArtMethod::kAccNative) [[unlikely]] {
                LOGV("deoptimize native method %s from %p to %p",
                     art_method->PrettyMethod(true).data(), art_method->GetEntryPoint(),
                     &art_quick_generic_jni_trampoline_);
                art_method->SetEntryPoint(
                    reinterpret_cast<void *>(&art_quick_generic_jni_trampoline_));
            } else {
                LOGV("deoptimize method %s from %p to %p", art_method->PrettyMethod(true).data(),
                     art_method->GetEntryPoint(), &art_quick_to_interpreter_bridge_);
                art_method->SetEntryPoint(
                    reinterpret_cast<void *>(&art_quick_to_interpreter_bridge_));
            }
            return true;
        }
        return false;
    }
};
}  // namespace lsplant::art
