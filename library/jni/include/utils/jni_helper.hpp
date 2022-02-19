#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma once

#include <jni.h>

#include <string>
#include <string_view>
#include <android/log.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                                         \
    TypeName(const TypeName &) = delete;                                                           \
    void operator=(const TypeName &) = delete

namespace lsplant {
template <class, template <class, class...> class>
struct is_instance : public std::false_type {};

template <class... Ts, template <class, class...> class U>
struct is_instance<U<Ts...>, U> : public std::true_type {};

template <class T, template <class, class...> class U>
inline constexpr bool is_instance_v = is_instance<T, U>::value;

template <typename T>
concept JObject = std::is_base_of_v<std::remove_pointer_t<_jobject>, std::remove_pointer_t<T>>;

template <JObject T>
class ScopedLocalRef {
public:
    using BaseType [[maybe_unused]] = T;

    ScopedLocalRef(JNIEnv *env, T localRef) : env_(env), local_ref_(localRef) {}

    ScopedLocalRef(ScopedLocalRef &&s) noexcept : env_(s.env_), local_ref_(s.release()) {}

    template <JObject U>
    ScopedLocalRef(ScopedLocalRef<U> &&s) noexcept : env_(s.env_), local_ref_((T)s.release()) {}

    explicit ScopedLocalRef(JNIEnv *env) noexcept : env_(env), local_ref_(nullptr) {}

    ~ScopedLocalRef() { reset(); }

    void reset(T ptr = nullptr) {
        if (ptr != local_ref_) {
            if (local_ref_ != nullptr) {
                env_->DeleteLocalRef(local_ref_);
            }
            local_ref_ = ptr;
        }
    }

    [[nodiscard]] T release() {
        T localRef = local_ref_;
        local_ref_ = nullptr;
        return localRef;
    }

    T get() const { return local_ref_; }

    operator T() const { return local_ref_; }

    // We do not expose an empty constructor as it can easily lead to errors
    // using common idioms, e.g.:
    //   ScopedLocalRef<...> ref;
    //   ref.reset(...);
    // Move assignment operator.
    ScopedLocalRef &operator=(ScopedLocalRef &&s) noexcept {
        reset(s.release());
        env_ = s.env_;
        return *this;
    }

    operator bool() const { return local_ref_; }

    template <JObject U>
    friend class ScopedLocalRef;

    friend class JUTFString;

private:
    JNIEnv *env_;
    T local_ref_;
    DISALLOW_COPY_AND_ASSIGN(ScopedLocalRef);
};

class JNIScopeFrame {
    JNIEnv *env_;

public:
    JNIScopeFrame(JNIEnv *env, jint size) : env_(env) { env_->PushLocalFrame(size); }

    ~JNIScopeFrame() { env_->PopLocalFrame(nullptr); }
};

template <typename T, typename U>
concept ScopeOrRaw = std::is_convertible_v<T, U> ||
    (is_instance_v<std::decay_t<T>, ScopedLocalRef>
         &&std::is_convertible_v<typename std::decay_t<T>::BaseType, U>);

template <typename T>
concept ScopeOrClass = ScopeOrRaw<T, jclass>;

template <typename T>
concept ScopeOrObject = ScopeOrRaw<T, jobject>;

inline ScopedLocalRef<jstring> ClearException(JNIEnv *env) {
    if (auto exception = env->ExceptionOccurred()) {
        env->ExceptionClear();
        static jclass log = (jclass)env->NewGlobalRef(env->FindClass("android/util/Log"));
        static jmethodID toString = env->GetStaticMethodID(
            log, "getStackTraceString", "(Ljava/lang/Throwable;)Ljava/lang/String;");
        auto str = (jstring)env->CallStaticObjectMethod(log, toString, exception);
        env->DeleteLocalRef(exception);
        return {env, str};
    }
    return {env, nullptr};
}

template <typename T>
[[maybe_unused]] inline auto UnwrapScope(T &&x) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>)
        return x.data();
    else if constexpr (is_instance_v<std::decay_t<T>, ScopedLocalRef>)
        return x.get();
    else
        return std::forward<T>(x);
}

template <typename T>
[[maybe_unused]] inline auto WrapScope(JNIEnv *env, T &&x) {
    if constexpr (std::is_convertible_v<T, _jobject *>) {
        return ScopedLocalRef(env, std::forward<T>(x));
    } else
        return x;
}

template <typename... T, size_t... I>
[[maybe_unused]] inline auto WrapScope(JNIEnv *env, std::tuple<T...> &&x,
                                       std::index_sequence<I...>) {
    return std::make_tuple(WrapScope(env, std::forward<T>(std::get<I>(x)))...);
}

template <typename... T>
[[maybe_unused]] inline auto WrapScope(JNIEnv *env, std::tuple<T...> &&x) {
    return WrapScope(env, std::forward<std::tuple<T...>>(x),
                     std::make_index_sequence<sizeof...(T)>());
}

inline auto JNI_NewStringUTF(JNIEnv *env, std::string_view sv) {
    return ScopedLocalRef(env, env->NewStringUTF(sv.data()));
}

class JUTFString {
public:
    inline JUTFString(JNIEnv *env, jstring jstr) : JUTFString(env, jstr, nullptr) {}

    inline JUTFString(const ScopedLocalRef<jstring> &jstr)
        : JUTFString(jstr.env_, jstr.local_ref_, nullptr) {}

    inline JUTFString(JNIEnv *env, jstring jstr, const char *default_cstr)
        : env_(env), jstr_(jstr) {
        if (env_ && jstr_)
            cstr_ = env_->GetStringUTFChars(jstr, nullptr);
        else
            cstr_ = default_cstr;
    }

    inline operator const char *() const { return cstr_; }

    inline operator const std::string() const { return cstr_; }

    inline operator const bool() const { return cstr_ != nullptr; }

    inline auto get() const { return cstr_; }

    inline ~JUTFString() {
        if (env_ && jstr_) env_->ReleaseStringUTFChars(jstr_, cstr_);
    }

    JUTFString(JUTFString &&other)
        : env_(std::move(other.env_)),
          jstr_(std::move(other.jstr_)),
          cstr_(std::move(other.cstr_)) {
        other.cstr_ = nullptr;
    }

    JUTFString &operator=(JUTFString &&other) {
        if (&other != this) {
            env_ = std::move(other.env_);
            jstr_ = std::move(other.jstr_);
            cstr_ = std::move(other.cstr_);
            other.cstr_ = nullptr;
        }
        return *this;
    }

private:
    JNIEnv *env_;
    jstring jstr_;
    const char *cstr_;

    JUTFString(const JUTFString &) = delete;

    JUTFString &operator=(const JUTFString &) = delete;
};

template <typename Func, typename... Args>
requires(std::is_function_v<Func>)
    [[maybe_unused]] inline auto JNI_SafeInvoke(JNIEnv *env, Func JNIEnv::*f, Args &&...args) {
    struct finally {
        finally(JNIEnv *env) : env_(env) {}

        ~finally() {
            if (auto exception = ClearException(env_)) {
                __android_log_print(ANDROID_LOG_ERROR,
#ifdef LOG_TAG
                                    LOG_TAG,
#else
                                    "JNIHelper",
#endif
                                    "%s", JUTFString(env_, exception.get()).get());
            }
        }

        JNIEnv *env_;
    } _(env);

    if constexpr (!std::is_same_v<void,
                                  std::invoke_result_t<Func, decltype(UnwrapScope(
                                                                 std::forward<Args>(args)))...>>)
        return WrapScope(env, (env->*f)(UnwrapScope(std::forward<Args>(args))...));
    else
        (env->*f)(UnwrapScope(std::forward<Args>(args))...);
}

[[maybe_unused]] inline auto JNI_FindClass(JNIEnv *env, std::string_view name) {
    return JNI_SafeInvoke(env, &JNIEnv::FindClass, name);
}

template <ScopeOrObject Object>
[[maybe_unused]] inline auto JNI_GetObjectClass(JNIEnv *env, const Object &obj) {
    return JNI_SafeInvoke(env, &JNIEnv::GetObjectClass, obj);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_GetFieldID(JNIEnv *env, const Class &clazz, std::string_view name,
                                            std::string_view sig) {
    return JNI_SafeInvoke(env, &JNIEnv::GetFieldID, clazz, name, sig);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_ToReflectedMethod(JNIEnv *env, const Class &clazz,
                                                   jmethodID method, jboolean isStatic) {
    return JNI_SafeInvoke(env, &JNIEnv::ToReflectedMethod, clazz, method, isStatic);
}

template <ScopeOrObject Object>
[[maybe_unused]] inline auto JNI_GetObjectField(JNIEnv *env, const Object &obj, jfieldID fieldId) {
    return JNI_SafeInvoke(env, &JNIEnv::GetObjectField, obj, fieldId);
}

template <ScopeOrObject Object>
[[maybe_unused]] inline auto JNI_GetLongField(JNIEnv *env, const Object &obj, jfieldID fieldId) {
    return JNI_SafeInvoke(env, &JNIEnv::GetLongField, obj, fieldId);
}

template <ScopeOrObject Object>
[[maybe_unused]] inline auto JNI_GetIntField(JNIEnv *env, const Object &obj, jfieldID fieldId) {
    return JNI_SafeInvoke(env, &JNIEnv::GetIntField, obj, fieldId);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_GetMethodID(JNIEnv *env, const Class &clazz, std::string_view name,
                                             std::string_view sig) {
    return JNI_SafeInvoke(env, &JNIEnv::GetMethodID, clazz, name, sig);
}

template <ScopeOrObject Object, typename... Args>
[[maybe_unused]] inline auto JNI_CallObjectMethod(JNIEnv *env, const Object &obj, jmethodID method,
                                                  Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallObjectMethod, obj, method, std::forward<Args>(args)...);
}

template <ScopeOrObject Object, typename... Args>
[[maybe_unused]] inline auto JNI_CallIntMethod(JNIEnv *env, const Object &obj, jmethodID method,
                                               Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallIntMethod, obj, method, std::forward<Args>(args)...);
}

template <ScopeOrObject Object, typename... Args>
[[maybe_unused]] inline auto JNI_CallLongMethod(JNIEnv *env, const Object &obj, Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallLongMethod, obj, std::forward<Args>(args)...);
}

template <ScopeOrObject Object, typename... Args>
[[maybe_unused]] inline auto JNI_CallVoidMethod(JNIEnv *env, const Object &obj, Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallVoidMethod, obj, std::forward<Args>(args)...);
}

template <ScopeOrObject Object, typename... Args>
[[maybe_unused]] inline auto JNI_CallBooleanMethod(JNIEnv *env, const Object &obj, Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallBooleanMethod, obj, std::forward<Args>(args)...);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_GetStaticFieldID(JNIEnv *env, const Class &clazz,
                                                  std::string_view name, std::string_view sig) {
    return JNI_SafeInvoke(env, &JNIEnv::GetStaticFieldID, clazz, name, sig);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_GetStaticObjectField(JNIEnv *env, const Class &clazz,
                                                      jfieldID fieldId) {
    return JNI_SafeInvoke(env, &JNIEnv::GetStaticObjectField, clazz, fieldId);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_GetStaticIntField(JNIEnv *env, const Class &clazz,
                                                   jfieldID fieldId) {
    return JNI_SafeInvoke(env, &JNIEnv::GetStaticIntField, clazz, fieldId);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_GetStaticMethodID(JNIEnv *env, const Class &clazz,
                                                   std::string_view name, std::string_view sig) {
    return JNI_SafeInvoke(env, &JNIEnv::GetStaticMethodID, clazz, name, sig);
}

template <ScopeOrClass Class, typename... Args>
[[maybe_unused]] inline auto JNI_CallStaticVoidMethod(JNIEnv *env, const Class &clazz,
                                                      Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallStaticVoidMethod, clazz, std::forward<Args>(args)...);
}

template <ScopeOrClass Class, typename... Args>
[[maybe_unused]] inline auto JNI_CallStaticObjectMethod(JNIEnv *env, const Class &clazz,
                                                        Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallStaticObjectMethod, clazz, std::forward<Args>(args)...);
}

template <ScopeOrClass Class, typename... Args>
[[maybe_unused]] inline auto JNI_CallStaticIntMethod(JNIEnv *env, const Class &clazz,
                                                     Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallStaticIntMethod, clazz, std::forward<Args>(args)...);
}

template <ScopeOrClass Class, typename... Args>
[[maybe_unused]] inline auto JNI_CallStaticBooleanMethod(JNIEnv *env, const Class &clazz,
                                                         Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::CallStaticBooleanMethod, clazz,
                          std::forward<Args>(args)...);
}

template <ScopeOrRaw<jarray> Array>
[[maybe_unused]] inline auto JNI_GetArrayLength(JNIEnv *env, const Array &array) {
    return JNI_SafeInvoke(env, &JNIEnv::GetArrayLength, array);
}

template <ScopeOrRaw<jobjectArray> Array>
[[maybe_unused]] inline auto JNI_GetObjectArrayElement(JNIEnv *env, const Array &array, jsize idx) {
    return JNI_SafeInvoke(env, &JNIEnv::GetObjectArrayElement, array, idx);
}

template <ScopeOrClass Class, typename... Args>
[[maybe_unused]] inline auto JNI_NewObject(JNIEnv *env, const Class &clazz, Args &&...args) {
    return JNI_SafeInvoke(env, &JNIEnv::NewObject, clazz, std::forward<Args>(args)...);
}

template <ScopeOrClass Class>
[[maybe_unused]] inline auto JNI_RegisterNatives(JNIEnv *env, const Class &clazz,
                                                 const JNINativeMethod *methods, jint size) {
    return JNI_SafeInvoke(env, &JNIEnv::RegisterNatives, clazz, methods, size);
}

template <ScopeOrObject Object>
[[maybe_unused]] inline auto JNI_NewGlobalRef(JNIEnv *env, Object &&x) {
    return (decltype(UnwrapScope(std::forward<Object>(x))))env->NewGlobalRef(
        UnwrapScope(std::forward<Object>(x)));
}

template <typename U, typename T>
[[maybe_unused]] inline auto JNI_Cast(ScopedLocalRef<T> &&x) requires(
    std::is_convertible_v<T, _jobject *>) {
    return ScopedLocalRef<U>(std::move(x));
}

}  // namespace lsplant

#undef DISALLOW_COPY_AND_ASSIGN

#pragma clang diagnostic pop
