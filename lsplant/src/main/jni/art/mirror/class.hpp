#pragma once

#include "common.hpp"

namespace lsplant::art {

namespace dex {
class ClassDef {};
}  // namespace dex

namespace mirror {

class Class {
private:
    CREATE_MEM_FUNC_SYMBOL_ENTRY(const char *, GetDescriptor, Class *thiz, std::string *storage) {
        if (GetDescriptorSym) [[likely]]
            return GetDescriptorSym(thiz, storage);
        else
            return "";
    }

    CREATE_MEM_FUNC_SYMBOL_ENTRY(const dex::ClassDef *, GetClassDef, Class *thiz) {
        if (GetClassDefSym) [[likely]]
            return GetClassDefSym(thiz);
        return nullptr;
    }

public:
    static bool Init(const HookHandler &handler) {
        if (!RETRIEVE_MEM_FUNC_SYMBOL(GetDescriptor,
                                      "_ZN3art6mirror5Class13GetDescriptorEPNSt3__112"
                                      "basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE")) {
            return false;
        }
        if (!RETRIEVE_MEM_FUNC_SYMBOL(GetClassDef, "_ZN3art6mirror5Class11GetClassDefEv")) {
            return false;
        }
        return true;
    }

    const char *GetDescriptor(std::string *storage) {
        if (GetDescriptorSym) {
            return GetDescriptor(this, storage);
        }
        return "";
    }

    std::string GetDescriptor() {
        std::string storage;
        return GetDescriptor(&storage);
    }

    const dex::ClassDef *GetClassDef() {
        if (GetClassDefSym) return GetClassDef(this);
        return nullptr;
    }
};

}  // namespace mirror
}  // namespace lsplant::art
