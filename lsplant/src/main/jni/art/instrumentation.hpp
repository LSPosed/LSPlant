#pragma once

#include "common.hpp"
#include "runtime/art_method.hpp"

namespace lsplant::art {

class Instrumentation {
    CREATE_MEM_HOOK_STUB_ENTRY(
            "_ZN3art15instrumentation15Instrumentation21UpdateMethodsCodeImplEPNS_9ArtMethodEPKv",
            void, UpdateMethodsCode,
            (Instrumentation * thiz, ArtMethod * art_method, const void *quick_code), {
                if (IsHooked(art_method)) [[unlikely]] {
                    LOGD("Skip update method code for hooked method %s",
                         art_method->PrettyMethod().c_str());
                    return;
                } else {
                    backup(thiz, art_method, quick_code);
                }
            });

public:
    static bool Init(const HookHandler &handler) {
        if (!HookSyms(handler, UpdateMethodsCode)) {
            return false;
        }
        return true;
    }
};

}
