#pragma once

#include "art/thread.hpp"
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
            return GetDescriptor(thiz, storage);
        else
            return "";
    }

    CREATE_MEM_FUNC_SYMBOL_ENTRY(const dex::ClassDef *, GetClassDef, Class *thiz) {
        if (GetClassDefSym) [[likely]]
            return GetClassDefSym(thiz);
        return nullptr;
    }

public:
    static bool Init(JNIEnv *env, const HookHandler &handler) {
        if (!RETRIEVE_MEM_FUNC_SYMBOL(GetDescriptor,
                                      "_ZN3art6mirror5Class13GetDescriptorEPNSt3__112"
                                      "basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE")) {
            return false;
        }
        if (!RETRIEVE_MEM_FUNC_SYMBOL(GetClassDef, "_ZN3art6mirror5Class11GetClassDefEv")) {
            return false;
        }

        auto clazz = JNI_FindClass(env, "java/lang/Class");
        if (!clazz) {
            LOGE("Failed to find Class");
            return false;
        }

        if (class_status = JNI_GetFieldID(env, clazz, "status", "I"); !class_status) {
            LOGE("Failed to find status");
            return false;
        }

        int sdk_int = GetAndroidApiLevel();

        if (sdk_int >= __ANDROID_API_P__) {
            is_unsigned = true;
        }

        return true;
    }

    const char *GetDescriptor(std::string *storage) {
        if (GetDescriptorSym) {
            return GetDescriptor(storage);
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

    static int GetStatus(JNIEnv *env, jclass clazz) {
        int status = JNI_GetIntField(env, clazz, class_status);
        return is_unsigned ? static_cast<uint32_t>(status) >> (32 - 4) : status;
    }

    static bool IsInitialized(JNIEnv *env, jclass clazz) {
        return is_unsigned ? GetStatus(env, clazz) >= 14 : GetStatus(env, clazz) == 11;
    }

    static Class *FromReflectedClass(JNIEnv *, jclass clazz) {
        return reinterpret_cast<Class *>(art::Thread::Current()->DecodeJObject(clazz));
    }

private:
    inline static jfieldID class_status = nullptr;
    inline static bool is_unsigned = false;
};

}  // namespace mirror
}  // namespace lsplant::art
