#pragma once

#include "common.hpp"

namespace lsplant::art {

class Runtime {
private:
    inline static Runtime *instance_;

    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, SetJavaDebuggable, Runtime *thiz, bool value) {
        if (SetJavaDebuggableSym) [[likely]] {
            SetJavaDebuggableSym(thiz, value);
        }
    }

public:
    inline static Runtime *Current() {
        return instance_;
    }

    void SetJavaDebuggable(bool value) {
        SetJavaDebuggable(this, value);
    }

    static bool Init(const HookHandler &handler) {
        Runtime *instance = nullptr;
        if (!RETRIEVE_FIELD_SYMBOL(instance, "_ZN3art7Runtime9instance_E")) {
            return false;
        }
        instance_ = *reinterpret_cast<Runtime **>(instance);
        LOGD("_ZN3art7Runtime9instance_E = %p", instance_);
        if (!RETRIEVE_MEM_FUNC_SYMBOL(SetJavaDebuggable, "_ZN3art7Runtime17SetJavaDebuggableEb")) {
            return false;
        }
        return false;
    }
};

}
