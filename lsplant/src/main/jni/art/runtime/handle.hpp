#pragma once

#include <type_traits>

#include "object_reference.hpp"
#include "value_object.hpp"

namespace lsplant::art {

namespace mirror {
class Class;
};

template <typename T>
class Handle : public ValueObject {
public:
    static_assert(std::is_same_v<T, mirror::Class>, "Expected mirror::Class");

    auto operator->() { return Get(); }

    T *Get() { return down_cast<T *>(reference_->AsMirrorPtr()); }

protected:
    StackReference<T> *reference_;
};

}  // namespace lsplant::art
