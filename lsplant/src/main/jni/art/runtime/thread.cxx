module;

export module thread;

import hook_helper;

namespace lsplant::art {
export class Thread {
    inline static Function<"_ZN3art6Thread14CurrentFromGdbEv", Thread *()> CurrentFromGdb_;

public:
    static Thread *Current() {
        if (CurrentFromGdb_) [[likely]]
            return CurrentFromGdb_();
        return nullptr;
    }

    static bool Init(const HookHandler &handler) {
        if (!handler.dlsym(CurrentFromGdb_)) [[unlikely]] {
            return false;
        }
        return true;
    }
};
}  // namespace lsplant::art
