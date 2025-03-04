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

template<typename T>
concept FuncType = std::is_function_v<T> || std::is_member_function_pointer_v<T>;

template <FixedString, FuncType>
struct Function;

template <FixedString Sym, typename Ret, typename... Args>
struct Function<Sym, Ret(Args...)> {
    [[gnu::always_inline]] static Ret operator()(Args... args) {
        return inner_.function_(args...);
    }
    [[gnu::always_inline]] operator bool() { return inner_.raw_function_; }
    [[gnu::always_inline]] auto operator&() const { return inner_.function_; }
    [[gnu::always_inline]] Function &operator=(void *function) {
        inner_.raw_function_ = function;
        return *this;
    }

private:
    inline static union {
        Ret (*function_)(Args...);
        void *raw_function_ = nullptr;
    } inner_;

    static_assert(sizeof(inner_.function_) == sizeof(inner_.raw_function_));
};

template <FixedString Sym, class This, typename Ret, typename... Args>
struct Function<Sym, Ret(This::*)(Args...)> {
    [[gnu::always_inline]] static Ret operator()(This *thiz, Args... args) {
        return (reinterpret_cast<ThisType *>(thiz)->*inner_.function_)(args...);
    }
    [[gnu::always_inline]] operator bool() { return inner_.raw_function_; }
    [[gnu::always_inline]] auto operator&() const { return inner_.function_; }
    [[gnu::always_inline]] Function &operator=(void *function) {
        inner_.raw_function_ = function;
        return *this;
    }

private:
    using ThisType = std::conditional_t<std::is_same_v<This, void>, Function, This>;
    inline static union {
        Ret (ThisType::*function_)(Args...) const;

        struct {
            void *raw_function_ = nullptr;
            [[maybe_unused]] std::ptrdiff_t adj = 0;
        };
    } inner_;

    static_assert(sizeof(inner_.function_) == sizeof(inner_.raw_function_) + sizeof(inner_.adj));
};

template <FixedString, typename T>
struct Field {
    [[gnu::always_inline]] T *operator->() { return inner_.field_; }
    [[gnu::always_inline]] T &operator*() { return *inner_.field_; }
    [[gnu::always_inline]] operator bool() { return inner_.raw_field_ != nullptr; }
    [[gnu::always_inline]] Field &operator=(void *field) {
        inner_.raw_field_ = field;
        return *this;
    }
private:
    inline static union {
        void *raw_field_ = nullptr;
        T *field_;
    } inner_;

    static_assert(sizeof(inner_.field_) == sizeof(inner_.raw_field_));
};

template <FixedString, FuncType>
struct Hooker;

template <FixedString Sym, typename Ret, typename... Args>
struct Hooker<Sym, Ret(Args...)> : Function<Sym, Ret(Args...)> {
    [[gnu::always_inline]] Hooker &operator=(void *function) {
        Function<Sym, Ret(Args...)>::operator=(function);
        return *this;
    }
private:
    [[gnu::always_inline]] constexpr Hooker(Ret (*replace)(Args...)) {
        replace_ = replace;
    };
    friend struct HookHandler;
    template<FixedString S>
    friend struct Symbol;
    inline static Ret (*replace_)(Args...) = nullptr;
};

template <FixedString Sym, class This, typename Ret, typename... Args>
struct Hooker<Sym, Ret(This::*)(Args...)> : Function<Sym, Ret(This::*)(Args...)> {
    [[gnu::always_inline]] Hooker &operator=(void *function) {
        Function<Sym, Ret(This::*)(Args...)>::operator=(function);
        return *this;
    }
private:
    [[gnu::always_inline]] constexpr Hooker(Ret (*replace)(This *, Args...)) {
        replace_ = replace;
    };
    friend struct HookHandler;
    template<FixedString S>
    friend struct Symbol;
    inline static Ret (*replace_)(This *, Args...) = nullptr;
};

struct HookHandler {
    HookHandler(const InitInfo &info) : info_(info) {}
    template <FixedString Sym, typename This, typename Ret, typename... Args>
    [[gnu::always_inline]] bool dlsym(Function<Sym, Ret(This::*)(Args...)> &function,
                                      bool match_prefix = false) const {
        return function = dlsym<Sym>(match_prefix);
    }

    template <FixedString Sym, typename Ret, typename... Args>
    [[gnu::always_inline]] bool dlsym(Function<Sym, Ret(Args...)> &function,
                                      bool match_prefix = false) const {
        return function = dlsym<Sym>(match_prefix);
    }

    template <FixedString Sym, typename T>
    [[gnu::always_inline]] bool dlsym(Field<Sym, T> &field, bool match_prefix = false) const {
        return field = dlsym<Sym>(match_prefix);
    }

    template <FixedString Sym, typename Ret, typename... Args>
    [[gnu::always_inline]] bool hook(Hooker<Sym, Ret(Args...)> &hooker) const {
        return hooker = hook(dlsym<Sym>(), reinterpret_cast<void *>(hooker.replace_));
    }

    template <FixedString Sym, typename This, typename Ret, typename... Args>
    [[gnu::always_inline]] bool hook(Hooker<Sym, Ret(This::*)(Args...)> &hooker) const {
        return hooker = hook(dlsym<Sym>(), reinterpret_cast<void *>(hooker.replace_));
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


struct Dummy;

template<typename F>
concept Backup = std::is_function_v<std::remove_pointer_t<F>> || requires(F&& f) { { f(std::declval<Dummy*>()) } -> std::same_as<Dummy*>; };

template<typename F>
concept MemBackup = std::is_function_v<std::remove_pointer_t<F>> || requires(F&& f) { { f(std::declval<Dummy*>()) } -> std::same_as<Dummy**>; };

template <typename T>
T return_t();

// for ndk 29+, use decltype(F::template operator()<&decltype([] static {})::operator()>)
// and remove Dummy, set MemBackup as std::is_member_function_pointer_v<F>
template<typename F, bool mem>
using Signature = decltype(F::template operator()<[](auto...a) static {
    using D = std::conditional_t<mem, Dummy**, Dummy*>;
    if constexpr ((false || ... || std::is_same_v<decltype(a), Dummy*>)) {
        return return_t<D>();
    } else {
        return return_t<decltype(F::template operator()<[](auto...) -> D {}>(std::declval<decltype(a)>()...))>();
    }
}>);

template<FixedString S>
struct Symbol {
    template<typename T>
    inline static decltype([]{
        if constexpr (FuncType<T>) {
            return Function<S, T>{};
        } else {
            return Field<S, T>{};
        }
    }()) as{};

    [[no_unique_address]] struct Hook {
        template<typename F>
        requires(requires { std::declval<Signature<F, false>>(); })
        auto operator->*(F&&) const {
            using HookerType = Hooker<S, Signature<F, false>>;
            return HookerType{static_cast<decltype(HookerType::replace_)>(&F::template operator()<HookerType::operator()>)};
        };
        template<typename F>
        requires(requires { std::declval<Signature<F, true>>(); })
        auto operator->*(F&&) const {
            constexpr auto c = []<class This, typename Ret, typename... Args>(Ret(*f)(This*, Args...)) -> Ret(This::*)(Args...) {
                return {};
            };
            using HookerType = Hooker<S, decltype(c.template operator()(std::declval<Signature<F, true>>()))>;
            return HookerType{static_cast<decltype(HookerType::replace_)>(&F::template operator()<HookerType::operator()>)};
        };

    } hook;
};

template <FixedString S> constexpr Symbol<S> operator""_sym() {
    return {};
}

template<FixedString S, FixedString P>
consteval auto operator|([[maybe_unused]] Symbol<S> a, [[maybe_unused]] Symbol<P> b) {
#if defined(__LP64__)
    return b;
#else
    return a;
#endif
}

void test() {
    "test"_sym.hook->*[]<MemBackup auto backup>(std::string *x) static -> char {
        return backup(x);
    };
}
}  // namespace lsplant
