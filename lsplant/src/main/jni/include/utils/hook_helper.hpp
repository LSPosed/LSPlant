#pragma once

#include <android/log.h>

#include <concepts>

#include "lsplant.hpp"
#include "type_traits.hpp"

namespace lsplant {

template <size_t N>
struct FixedString {
    consteval FixedString(const char (&str)[N]) { std::copy_n(str, N, data); }
#if defined(__LP64__)
    template <size_t M>
    consteval FixedString(const char (&)[M], const char (&str)[N]) : FixedString(str) {}
#else
    template <size_t M>
    consteval FixedString(const char (&str)[N], const char (&)[M]) : FixedString(str) {}
#endif
    char data[N] = {};
};

template <FixedString, typename>
struct Function;

template <FixedString, typename, typename>
struct MemberFunction;

template <FixedString, typename T>
struct Field {
    [[gnu::always_inline]] T *operator->() { return field_; }
    [[gnu::always_inline]] T &operator*() { return *field_; }
    [[gnu::always_inline]] operator bool() { return field_ != nullptr; }

private:
    friend struct HookHandler;
    T *field_;
};

template <FixedString, typename>
struct Hooker;

template <FixedString, typename, typename>
struct MemberHooker;

template <typename Class, typename Return, typename T, typename... Args>
    requires(std::is_same_v<T, void> || std::is_same_v<Class, T>)
inline auto memfun_cast(Return (*func)(T *, Args...)) {
    union {
        Return (Class::*f)(Args...) const;

        struct {
            decltype(func) p;
            std::ptrdiff_t adj;
        } data;
    } u{.data = {func, 0}};
    static_assert(sizeof(u.f) == sizeof(u.data), "Try different T");
    return u.f;
}

template <std::same_as<void> T, typename Return, typename... Args>
inline auto memfun_cast(Return (*func)(T *, Args...)) {
    return memfun_cast<T>(func);
}

struct HookHandler {
    HookHandler(const InitInfo &info) : info_(info) {}
    template <FixedString Sym, typename This, typename Ret, typename... Args>
    [[gnu::always_inline]] bool dlsym(MemberFunction<Sym, This, Ret(Args...)> &function,
                                      bool match_prefix = false) const {
        return function.function_ = memfun_cast<This>(
                   reinterpret_cast<Ret (*)(This *, Args...)>(dlsym<Sym>(match_prefix)));
    }

    template <FixedString Sym, typename Ret, typename... Args>
    [[gnu::always_inline]] bool dlsym(Function<Sym, Ret(Args...)> &function,
                                      bool match_prefix = false) const {
        return function.function_ = reinterpret_cast<Ret (*)(Args...)>(dlsym<Sym>(match_prefix));
    }

    template <FixedString Sym, typename T>
    [[gnu::always_inline]] bool dlsym(Field<Sym, T> &field, bool match_prefix = false) const {
        return field.field_ = reinterpret_cast<T *>(dlsym<Sym>(match_prefix));
    }

    template <FixedString Sym, typename Ret, typename... Args>
    [[gnu::always_inline]] bool hook(Hooker<Sym, Ret(Args...)> &hooker) const {
        return hooker.function_ = reinterpret_cast<Ret (*)(Args...)>(
                   hook(dlsym<Sym>(), reinterpret_cast<void *>(hooker.replace_)));
    }

    template <FixedString Sym, typename This, typename Ret, typename... Args>
    [[gnu::always_inline]] bool hook(MemberHooker<Sym, This, Ret(Args...)> &hooker) const {
        return hooker.function_ = memfun_cast<This>(reinterpret_cast<Ret (*)(This *, Args...)>(
                   hook(dlsym<Sym>(), reinterpret_cast<void *>(hooker.replace_))));
    }

    template <typename T1, typename T2, typename... U>
    [[gnu::always_inline]] bool hook(T1 &arg1, T2 &arg2, U &...args) const {
        return ((hook(arg1) || hook(arg2)) || ... || hook(args));
    }

private:
    const InitInfo &info_;

    template <FixedString Sym>
    [[gnu::always_inline]] void *dlsym(bool match_prefix = false) const {
        if (auto match = info_.art_symbol_resolver(Sym.data); match) {
            return match;
        }
        if (match_prefix && info_.art_symbol_prefix_resolver) {
            return info_.art_symbol_prefix_resolver(Sym.data);
        }
        return nullptr;
    }

    void *hook(void *original, void *replace) const {
        if (original) {
            return info_.inline_hooker(original, replace);
        }
        return nullptr;
    }
};

template <FixedString Sym, typename Ret, typename... Args>
struct Function<Sym, Ret(Args...)> {
    [[gnu::always_inline]] constexpr Ret operator()(Args... args) { return function_(args...); }
    [[gnu::always_inline]] operator bool() { return function_ != nullptr; }
    auto operator&() const { return function_; }

private:
    friend struct HookHandler;
    Ret (*function_)(Args...) = nullptr;
};

template <FixedString Sym, typename This, typename Ret, typename... Args>
struct MemberFunction<Sym, This, Ret(Args...)> {
    [[gnu::always_inline]] constexpr Ret operator()(This *thiz, Args... args) {
        return (reinterpret_cast<ThisType *>(thiz)->*function_)(args...);
    }
    [[gnu::always_inline]] operator bool() { return function_ != nullptr; }

private:
    friend struct HookHandler;
    using ThisType = std::conditional_t<std::is_same_v<This, void>, MemberFunction, This>;
    Ret (ThisType::*function_)(Args...) const = nullptr;
};

template <FixedString Sym, typename Ret, typename... Args>
struct Hooker<Sym, Ret(Args...)> : Function<Sym, Ret(Args...)> {
    [[gnu::always_inline]] constexpr Hooker(Ret (*replace)(Args...)) : replace_(replace) {};

private:
    friend struct HookHandler;
    [[maybe_unused]] Ret (*replace_)(Args...) = nullptr;
};

template <FixedString Sym, typename This, typename Ret, typename... Args>
struct MemberHooker<Sym, This, Ret(Args...)> : MemberFunction<Sym, This, Ret(Args...)> {
    [[gnu::always_inline]] constexpr MemberHooker(Ret (*replace)(This *, Args...))
        : replace_(replace) {};

private:
    friend struct HookHandler;
    [[maybe_unused]] Ret (*replace_)(This *, Args...) = nullptr;
};

}  // namespace lsplant
