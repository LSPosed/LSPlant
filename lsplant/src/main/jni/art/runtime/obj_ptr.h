#pragma once

template <typename MirrorType>
class ObjPtr {
public:
    inline MirrorType* operator->() const { return Ptr(); }
    inline MirrorType* Ptr() const { return reference_; }
    inline operator MirrorType*() const { return Ptr(); }

private:
    MirrorType* reference_;
};
