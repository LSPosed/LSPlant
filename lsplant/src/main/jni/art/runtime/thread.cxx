module;

export module lsplant:thread;

import hook_helper;

namespace lsplant::art {
export class Thread {
    inline static auto CurrentFromGdb_ = "_ZN3art6Thread14CurrentFromGdbEv"_sym.as<Thread *()>;

public:
    static Thread *Current() {
        if (CurrentFromGdb_) [[likely]]
            return CurrentFromGdb_();
        return nullptr;
    }

    static bool Init(const HookHandler &handler) {
        if (!handler(CurrentFromGdb_)) [[unlikely]] {
            return false;
        }
        return true;
    }
};
}  // namespace lsplant::art
