module;

export module thread_list;

import hook_helper;

namespace lsplant::art::thread_list {

export class ScopedSuspendAll {
    inline static MemberFunction<"_ZN3art16ScopedSuspendAllC2EPKcb", ScopedSuspendAll,
                                 void(const char *, bool)>
        constructor_;
    inline static MemberFunction<"_ZN3art16ScopedSuspendAllD2Ev", ScopedSuspendAll, void()>
        destructor_;

    inline static Function<"_ZN3art3Dbg9SuspendVMEv", void()> SuspendVM_;
    inline static Function<"_ZN3art3Dbg8ResumeVMEv", void()> ResumeVM_;

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
        if (!handler.dlsym(constructor_) && !handler.dlsym(SuspendVM_)) [[unlikely]] {
            return false;
        }
        if (!handler.dlsym(destructor_) && !handler.dlsym(ResumeVM_)) [[unlikely]] {
            return false;
        }
        return true;
    }
};

}  // namespace lsplant::art::thread_list
