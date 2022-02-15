#pragma once

#include "common.hpp"

namespace lsplant::art {

class Thread {
    struct ObjPtr {
        void *data;
    };

    CREATE_MEM_FUNC_SYMBOL_ENTRY(ObjPtr, DecodeJObject, Thread *thiz, jobject obj) {
        if (DecodeJObjectSym)
            return DecodeJObjectSym(thiz, obj);
        else
            return { .data=nullptr };
    }

    CREATE_FUNC_SYMBOL_ENTRY(Thread *, CurrentFromGdb) {
        if (CurrentFromGdbSym) [[likely]]
            return CurrentFromGdbSym();
        else
            return nullptr;
    }

public:
    static Thread *Current() {
        return CurrentFromGdb();
    }

    static bool Init(const HookHandler &handler) {
        if (!RETRIEVE_MEM_FUNC_SYMBOL(DecodeJObject,
                                      "_ZNK3art6Thread13DecodeJObjectEP8_jobject")) {
            return false;
        }
        if (!RETRIEVE_FUNC_SYMBOL(CurrentFromGdb,
                                  "_ZN3art6Thread14CurrentFromGdbEv")) {
            return false;
        }
        return true;
    }

    void *DecodeJObject(jobject obj) {
        if (DecodeJObjectSym) [[likely]] {
            return DecodeJObject(this, obj).data;
        }
        return nullptr;
    }
};
}
