#pragma once

#include "common.hpp"

namespace lsplant::art::jit {
class JitCodeCache {
    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, MoveObsoleteMethod, JitCodeCache *thiz,
                                 ArtMethod *old_method, ArtMethod *new_method) {
        if (MoveObsoleteMethodSym) [[likely]]
            MoveObsoleteMethodSym(thiz, old_method, new_method);
    }

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art3jit12JitCodeCache19GarbageCollectCacheEPNS_6ThreadE", void,
                               GarbageCollectCache, (JitCodeCache * thiz, Thread *self), {
                                   LOGD("Before jit cache gc, moving hooked methods");
                                   for (auto [target, backup] : GetJitMovements()) {
                                       MoveObsoleteMethod(thiz, target, backup);
                                   }
                                   backup(thiz, self);
                               });

public:
    static bool Init(const HookHandler &handler) {
        if (!RETRIEVE_MEM_FUNC_SYMBOL(
                MoveObsoleteMethod,
                "_ZN3art3jit12JitCodeCache18MoveObsoleteMethodEPNS_9ArtMethodES3_")) {
            return false;
        }
        if (!HookSyms(handler, GarbageCollectCache)) {
            return false;
        }
        return true;
    }
};
}  // namespace lsplant::art::jit
