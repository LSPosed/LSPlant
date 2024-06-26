module;

#include "logging.hpp"
#include "utils/hook_helper.hpp"

export module jit;

import art_method;
import common;
import thread;

namespace lsplant::art::jit {
enum class CompilationKind {
    kOsr [[maybe_unused]],
    kBaseline [[maybe_unused]],
    kOptimized,
};

export class Jit {
    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art3jit3Jit27EnqueueOptimizedCompilationEPNS_9ArtMethodEPNS_6ThreadE", void,
        EnqueueOptimizedCompilation, (Jit * thiz, ArtMethod *method, Thread *self), {
            if (auto target = IsBackup(method); target) [[unlikely]] {
                LOGD("Propagate enqueue compilation: %p -> %p", method, target);
                method = target;
            }
            return backup(thiz, method, self);
        });

    CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN3art3jit3Jit14AddCompileTaskEPNS_6ThreadEPNS_9ArtMethodENS_15CompilationKindEb", void,
        AddCompileTask,
        (Jit * thiz, Thread *self, ArtMethod *method, CompilationKind compilation_kind,
         bool precompile),
        {
            if (compilation_kind == CompilationKind::kOptimized && !precompile/* && in_enqueue*/) {
                if (auto backup = IsHooked(method); backup) [[unlikely]] {
                    LOGD("Propagate compile task: %p -> %p", method, backup);
                    method = backup;
                }
            }
            return backup(thiz, self, method, compilation_kind, precompile);
        });

public:
    static bool Init(const HookHandler &handler) {
        auto sdk_int = GetAndroidApiLevel();

        if (sdk_int <= __ANDROID_API_U__) [[likely]] {
            HookSyms(handler, EnqueueOptimizedCompilation);
            HookSyms(handler, AddCompileTask);
        }
        return true;
    }
};
}  // namespace lsplant::art::jit
