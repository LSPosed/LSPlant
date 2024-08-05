module;

#include "logging.hpp"

export module jit;

import art_method;
import common;
import thread;
import hook_helper;

namespace lsplant::art::jit {
enum class CompilationKind {
    kOsr [[maybe_unused]],
    kBaseline [[maybe_unused]],
    kOptimized,
};

export class Jit {
    inline static MemberHooker<
        "_ZN3art3jit3Jit27EnqueueOptimizedCompilationEPNS_9ArtMethodEPNS_6ThreadE", Jit,
        void(ArtMethod *, Thread *)>
        EnqueueOptimizedCompilation_ = +[](Jit *thiz, ArtMethod *method, Thread *self) {
            if (auto target = IsBackup(method); target) [[unlikely]] {
                LOGD("Propagate enqueue compilation: %p -> %p", method, target);
                method = target;
            }
            return EnqueueOptimizedCompilation_(thiz, method, self);
        };

    inline static MemberHooker<
        "_ZN3art3jit3Jit14AddCompileTaskEPNS_6ThreadEPNS_9ArtMethodENS_15CompilationKindEb", Jit,
        void(Thread *, ArtMethod *, CompilationKind, bool)>
        AddCompileTask_ = +[](Jit *thiz, Thread *self, ArtMethod *method,
                              CompilationKind compilation_kind, bool precompile) {
            if (compilation_kind == CompilationKind::kOptimized && !precompile) {
                if (auto backup = IsHooked(method); backup) [[unlikely]] {
                    LOGD("Propagate compile task: %p -> %p", method, backup);
                    method = backup;
                }
            }
            return AddCompileTask_(thiz, self, method, compilation_kind, precompile);
        };

public:
    static bool Init(const HookHandler &handler) {
        auto sdk_int = GetAndroidApiLevel();

        if (sdk_int <= __ANDROID_API_U__) [[likely]] {
            handler.hook(EnqueueOptimizedCompilation_);
            handler.hook(AddCompileTask_);
        }
        return true;
    }
};
}  // namespace lsplant::art::jit
