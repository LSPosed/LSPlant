module;

export module lsplant:thread_list;

import hook_helper;

namespace lsplant::art::thread_list {

export class ScopedSuspendAll {
    inline static auto constructor_ =
            "_ZN3art16ScopedSuspendAllC2EPKcb"_sym.as<void(ScopedSuspendAll::*)(const char *, bool)>;

    inline static auto destructor_ =
            "_ZN3art16ScopedSuspendAllD2Ev"_sym.as<void(ScopedSuspendAll::*)()>;

    inline static auto SuspendVM_ = "_ZN3art3Dbg9SuspendVMEv"_sym.as<void()>;
    inline static auto ResumeVM_ = "_ZN3art3Dbg8ResumeVMEv"_sym.as<void()>;

public:
    ScopedSuspendAll(const char *cause, bool long_suspend) {
        if (constructor_) {
            constructor_(this, cause, long_suspend);
        } else if (SuspendVM_) {
            SuspendVM_();
        }
    }

    ~ScopedSuspendAll() {
        if (destructor_) {
            destructor_(this);
        } else if (ResumeVM_) {
            ResumeVM_();
        }
    }

    static bool Init(const HookHandler &handler) {
        if (!handler(constructor_, SuspendVM_)) [[unlikely]] {
            return false;
        }
        if (!handler(destructor_, ResumeVM_)) [[unlikely]] {
            return false;
        }
        return true;
    }
};

}  // namespace lsplant::art::thread_list
