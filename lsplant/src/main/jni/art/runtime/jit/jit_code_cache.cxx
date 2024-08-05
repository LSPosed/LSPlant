module;

#include "logging.hpp"

export module jit_code_cache;

import art_method;
import common;
import thread;
import hook_helper;

namespace lsplant::art::jit {
export class JitCodeCache {
    inline static MemberFunction<"_ZN3art3jit12JitCodeCache18MoveObsoleteMethodEPNS_9ArtMethodES3_",
                                 JitCodeCache, void(ArtMethod *, ArtMethod *)>
        MoveObsoleteMethod_;

    static void MoveObsoleteMethods(JitCodeCache *thiz) {
        auto movements = GetJitMovements();
        LOGD("Before jit cache collection, moving %zu hooked methods", movements.size());
        for (auto [target, backup] : movements) {
            if (MoveObsoleteMethod_) [[likely]]
                MoveObsoleteMethod_(thiz, target, backup);
            else {
                backup->SetData(backup->GetData());
                target->SetData(nullptr);
            }
        }
    }

    inline static MemberHooker<"_ZN3art3jit12JitCodeCache19GarbageCollectCacheEPNS_6ThreadE",
                               JitCodeCache, void(Thread *)>
        GarbageCollectCache_ = +[](JitCodeCache *thiz, Thread *self) {
            MoveObsoleteMethods(thiz);
            GarbageCollectCache_(thiz, self);
        };

    inline static MemberHooker<"_ZN3art3jit12JitCodeCache12DoCollectionEPNS_6ThreadE", JitCodeCache,
                               void(Thread *)>
        DoCollection_ = +[](JitCodeCache *thiz, Thread *self) {
            MoveObsoleteMethods(thiz);
            DoCollection_(thiz, self);
        };

public:
    static bool Init(const HookHandler &handler) {
        auto sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_O__) [[likely]] {
            if (!handler.dlsym(MoveObsoleteMethod_)) [[unlikely]] {
                return false;
            }
        }
        if (sdk_int >= __ANDROID_API_N__) [[likely]] {
            if (!handler.hook(GarbageCollectCache_, DoCollection_)) [[unlikely]] {
                return false;
            }
        }
        return true;
    }
};
}  // namespace lsplant::art::jit
