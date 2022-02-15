#pragma once

#include "common.hpp"

namespace lsplant::art::thread_list {

class ScopedSuspendAll {
    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, constructor, ScopedSuspendAll *thiz, const char *cause,
                                 bool long_suspend) {
        if (thiz == nullptr) [[unlikely]] return;
        if (constructorSym) [[likely]]
            return constructorSym(thiz, cause, long_suspend);
    }

    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, destructor, ScopedSuspendAll *thiz) {
        if (thiz == nullptr) [[unlikely]] return;
        if (destructorSym) [[likely]]
            return destructorSym(thiz);
    }

public:
    ScopedSuspendAll(const char *cause, bool long_suspend) {
        constructor(this, cause, long_suspend);
    }

    ~ScopedSuspendAll() {
        destructor(this);
    }

    static bool Init(const HookHandler &handler) {
        if (!RETRIEVE_MEM_FUNC_SYMBOL(constructor, "_ZN3art16ScopedSuspendAllC2EPKcb")) {
            return false;
        }
        if (!RETRIEVE_MEM_FUNC_SYMBOL(destructor, "_ZN3art16ScopedSuspendAllD2Ev")) {
            return false;
        }
        return true;
    }
};

}

