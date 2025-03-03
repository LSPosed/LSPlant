module;

#include "lsplant.hpp"

export module lsplant;

export namespace lsplant {
    inline namespace v3 {
        using lsplant::v3::InitInfo;
        using lsplant::v3::Init;
        using lsplant::v3::Hook;
        using lsplant::v3::UnHook;
        using lsplant::v3::IsHooked;
        using lsplant::v3::Deoptimize;
        using lsplant::v3::GetNativeFunction;
        using lsplant::v3::MakeClassInheritable;
        using lsplant::v3::MakeDexFileTrusted;
    }
}
