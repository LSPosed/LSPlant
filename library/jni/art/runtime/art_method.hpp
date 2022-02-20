#pragma once

#include "art/mirror/class.hpp"
#include "common.hpp"

namespace lsplant::art {

class ArtMethod {
    CREATE_MEM_FUNC_SYMBOL_ENTRY(std::string, PrettyMethod, ArtMethod *thiz, bool with_signature) {
        if (thiz == nullptr) [[unlikely]]
            return "null";
        else if (PrettyMethodSym) [[likely]]
            return PrettyMethodSym(thiz, with_signature);
        else
            return "null sym";
    }

    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, ThrowInvocationTimeError, ArtMethod *thiz) {
        if (thiz && ThrowInvocationTimeErrorSym) [[likely]]
            return ThrowInvocationTimeErrorSym(thiz);
    }

    CREATE_FUNC_SYMBOL_ENTRY(const char *, GetMethodShorty, JNIEnv *env, jmethodID mid) {
        if (GetMethodShortySym) [[likely]]
            return GetMethodShortySym(env, mid);
        return nullptr;
    }

    inline void ThrowInvocationTimeError() { ThrowInvocationTimeError(this); }

public:
    inline static const char *GetMethodShorty(JNIEnv *env, jobject method) {
        return GetMethodShorty(env, env->FromReflectedMethod(method));
    }

    void SetNonCompilable() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccCompileDontBother;
        access_flags &= ~kAccPreCompiled;
        SetAccessFlags(access_flags);
    }

    void SetNonIntrinsic() {
        auto access_flags = GetAccessFlags();
        access_flags &= ~kAccFastInterpreterToInterpreterInvoke;
        SetAccessFlags(access_flags);
    }

    void SetPrivate() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccPrivate;
        access_flags &= ~kAccProtected;
        access_flags &= ~kAccPublic;
        SetAccessFlags(access_flags);
    }

    void SetPublic() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccPublic;
        access_flags &= ~kAccProtected;
        access_flags &= ~kAccPrivate;
        SetAccessFlags(access_flags);
    }

    void SetProtected() {
        auto access_flags = GetAccessFlags();
        access_flags |= kAccProtected;
        access_flags &= ~kAccPrivate;
        access_flags &= ~kAccPublic;
        SetAccessFlags(access_flags);
    }

    void SetNonFinal() {
        auto access_flags = GetAccessFlags();
        access_flags &= ~kAccFinal;
        SetAccessFlags(access_flags);
    }

    bool IsPrivate() { return GetAccessFlags() & kAccPrivate; }
    bool IsProtected() { return GetAccessFlags() & kAccProtected; }
    bool IsPublic() { return GetAccessFlags() & kAccPublic; }
    bool IsFinal() { return GetAccessFlags() & kAccFinal; }
    bool IsStatic() { return GetAccessFlags() & kAccStatic; }
    bool IsNative() { return GetAccessFlags() & kAccNative; }

    void CopyFrom(const ArtMethod *other) { memcpy(this, other, art_method_size); }

    void SetEntryPoint(void *entry_point) {
        *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + entry_point_offset) =
            entry_point;
    }

    void *GetEntryPoint() {
        return *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + entry_point_offset);
    }

    void *GetData() {
        return *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + data_offset);
    }

    uint32_t GetAccessFlags() {
        return (reinterpret_cast<const std::atomic<uint32_t> *>(reinterpret_cast<uintptr_t>(this) +
                                                                access_flags_offset))
            ->load(std::memory_order_relaxed);
    }

    void SetAccessFlags(uint32_t flags) {
        return (reinterpret_cast<std::atomic<uint32_t> *>(reinterpret_cast<uintptr_t>(this) +
                                                          access_flags_offset))
            ->store(flags, std::memory_order_relaxed);
    }

    std::string PrettyMethod(bool with_signature = true) {
        return PrettyMethod(this, with_signature);
    }

    static art::ArtMethod *FromReflectedMethod(JNIEnv *env, jobject method) {
        return reinterpret_cast<art::ArtMethod *>(JNI_GetLongField(env, method, art_method_field));
    }

    static bool Init(JNIEnv *env, const HookHandler handler) {
        auto executable = JNI_FindClass(env, "java/lang/reflect/Executable");
        if (!executable) {
            LOGE("Failed to found Executable");
            return false;
        }

        if (art_method_field = JNI_GetFieldID(env, executable, "artMethod", "J");
            !art_method_field) {
            LOGE("Failed to find artMethod field");
            return false;
        }

        auto throwable = JNI_FindClass(env, "java/lang/Throwable");
        if (!throwable) {
            LOGE("Failed to found Executable");
            return false;
        }
        auto clazz = JNI_FindClass(env, "java/lang/Class");
        static_assert(std::is_same_v<decltype(clazz)::BaseType, jclass>);
        jmethodID get_declared_constructors = JNI_GetMethodID(env, clazz, "getDeclaredConstructors",
                                                              "()[Ljava/lang/reflect/Constructor;");
        const auto constructors =
            JNI_Cast<jobjectArray>(JNI_CallObjectMethod(env, throwable, get_declared_constructors));
        if (constructors.size() < 2) {
            LOGE("Throwable has less than 2 constructors");
            return false;
        }
        auto &first_ctor = constructors[0];
        auto &second_ctor = constructors[1];
        auto *first = FromReflectedMethod(env, first_ctor.get());
        auto *second = FromReflectedMethod(env, second_ctor.get());
        art_method_size = reinterpret_cast<uintptr_t>(second) - reinterpret_cast<uintptr_t>(first);
        LOGD("ArtMethod size: %zu", art_method_size);

        if (RoundUpTo(4 * 4 + 2 * 2, kPointerSize) + kPointerSize * 3 < art_method_size) {
            LOGW("ArtMethod size exceeds maximum assume. There may be something wrong.");
        }

        entry_point_offset = art_method_size - sizeof(void *);
        LOGD("ArtMethod::entrypoint offset: %zu", entry_point_offset);

        data_offset = entry_point_offset - sizeof(void *);
        LOGD("ArtMethod::data offset: %zu", data_offset);

        if (auto access_flags_field = JNI_GetFieldID(env, executable, "accessFlags", "I");
            access_flags_field) {
            uint32_t real_flags = JNI_GetIntField(env, first_ctor, access_flags_field);
            for (size_t i = 0; i < art_method_size; i += sizeof(uint32_t)) {
                if (*reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(first) + i) ==
                    real_flags) {
                    access_flags_offset = i;
                    LOGD("ArtMethod::access_flags offset: %zu", access_flags_offset);
                    break;
                }
            }
        }
        if (access_flags_offset == 0) {
            LOGW("Failed to find accessFlags field. Fallback to 4.");
            access_flags_offset = 4U;
        }
        auto sdk_int = GetAndroidApiLevel();

        if (sdk_int < __ANDROID_API_R__) kAccPreCompiled = 0;
        if (sdk_int < __ANDROID_API_Q__) kAccFastInterpreterToInterpreterInvoke = 0;

        if (!RETRIEVE_FUNC_SYMBOL(GetMethodShorty,
                                  "_ZN3artL15GetMethodShortyEP7_JNIEnvP10_jmethodID")) {
            LOGE("Failed to find GetMethodShorty");
            return false;
        }

        if (sdk_int == __ANDROID_API_O__) [[unlikely]] {
            auto abstract_method_error = JNI_FindClass(env, "java/lang/AbstractMethodError");
            if (!abstract_method_error) {
                LOGE("Failed to find AbstractMethodError");
                return false;
            }
            auto executable_get_name =
                JNI_GetMethodID(env, executable, "getName", "()Ljava/lang/String;");
            if (!executable_get_name) {
                LOGE("Failed to find Executable.getName");
                return false;
            }
            auto abstract_method = FromReflectedMethod(
                env, JNI_ToReflectedMethod(env, executable, executable_get_name, false));
            uint32_t access_flags = abstract_method->GetAccessFlags();
            abstract_method->SetAccessFlags(access_flags | kAccDefaultConflict);
            abstract_method->ThrowInvocationTimeError();
            abstract_method->SetAccessFlags(access_flags);
            if (auto exception = env->ExceptionOccurred();
                env->ExceptionClear(),
                (!exception || JNI_IsInstanceOf(env, exception, abstract_method_error))) {
                kAccCompileDontBother = kAccDefaultConflict;
            }
        }

        return true;
    }

    static size_t GetEntryPointOffset() { return entry_point_offset; }

    constexpr static uint32_t kAccPublic = 0x0001;     // class, field, method, ic
    constexpr static uint32_t kAccPrivate = 0x0002;    // field, method, ic
    constexpr static uint32_t kAccProtected = 0x0004;  // field, method, ic
    constexpr static uint32_t kAccStatic = 0x0008;     // field, method, ic
    constexpr static uint32_t kAccNative = 0x0100;     // method
    constexpr static uint32_t kAccFinal = 0x0010;      // class, field, method, ic

private:
    inline static jfieldID art_method_field = nullptr;
    inline static size_t art_method_size = 0;
    inline static size_t entry_point_offset = 0;
    inline static size_t data_offset = 0;
    inline static size_t access_flags_offset = 0;
    inline static uint32_t kAccFastInterpreterToInterpreterInvoke = 0x40000000;
    inline static uint32_t kAccPreCompiled = 0x00200000;
    inline static uint32_t kAccCompileDontBother = 0x02000000;
    inline static uint32_t kAccDefaultConflict = 0x01000000;
};

}  // namespace lsplant::art
