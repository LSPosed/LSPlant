module;

#include "logging.hpp"

export module lsplant:jit;

import :art_method;
import :common;
import :thread;
import hook_helper;

namespace lsplant::art::jit {
enum class CompilationKind {
    kOsr [[maybe_unused]],
    kBaseline [[maybe_unused]],
    kOptimized,
};

export class Jit {
    inline static auto EnqueueOptimizedCompilation_ =
        "_ZN3art3jit3Jit27EnqueueOptimizedCompilationEPNS_9ArtMethodEPNS_6ThreadE"_sym.hook->*[]
        <MemBackup auto backup>
        (Jit *thiz, ArtMethod *method, Thread *self) static -> void {
            if (auto target = IsBackup(method); target) [[unlikely]] {
                LOGD("Propagate enqueue compilation: %p -> %p", method, target);
                method = target;
            }
            return backup(thiz, method, self);
        };

    inline static auto AddCompileTask_ =
        "_ZN3art3jit3Jit14AddCompileTaskEPNS_6ThreadEPNS_9ArtMethodENS_15CompilationKindEb"_sym.hook->*[]
        <MemBackup auto backup>
        (Jit *thiz, Thread *self, ArtMethod *method, CompilationKind compilation_kind, bool precompile) static -> void {
            if (compilation_kind == CompilationKind::kOptimized && !precompile) {
                if (auto b = IsHooked(method); b) [[unlikely]] {
                    LOGD("Propagate compile task: %p -> %p", method, b);
                    method = b;
                }
            }
            return backup(thiz, self, method, compilation_kind, precompile);
        };

public:
    static bool Init(const HookHandler &handler) {
        auto sdk_int = GetAndroidApiLevel();

        if (sdk_int <= __ANDROID_API_U__) [[likely]] {
            handler(EnqueueOptimizedCompilation_);
            handler(AddCompileTask_);
        }
        return true;
    }
};
}  // namespace lsplant::art::jit
