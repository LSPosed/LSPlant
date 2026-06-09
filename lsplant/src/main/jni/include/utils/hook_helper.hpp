#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "lsplant.hpp"
#include "type_traits.hpp"

namespace lsplant {

template <size_t N, char... Cs>
struct FixedString {
    static constexpr auto data = [] consteval {
        static constexpr auto kString = std::array{Cs...};
        return std::string_view{kString.data(), N};
    }();
};

template <typename T>
concept FuncType = std::is_function_v<T> || std::is_member_function_pointer_v<T>;

template <FixedString, FuncType>
struct Function;

template <FixedString Sym, typename Ret, typename... Args>
struct Function<Sym, Ret(Args...)> {
    [[gnu::always_inline]] static Ret operator()(Args... args) {
        return inner_.function_(std::forward<Args>(args)...);
    }
    [[gnu::always_inline]] operator bool() const { return inner_.raw_function_ != nullptr; }
    [[gnu::always_inline]] auto operator&() const { return inner_.function_; }
    [[gnu::always_inline]] auto &operator=(void *function) const {
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

template <class Derived, typename This, typename Ret, typename... Args>
struct BaseMemberFunction {
    [[gnu::always_inline]] static Ret operator()(This *thiz, Args... args) {
        return (reinterpret_cast<ThisType *>(thiz)->*Derived::inner_.function_)(
            std::forward<Args>(args)...);
    }
    [[gnu::always_inline]] operator bool() const {
        return Derived::inner_.raw_function_ != nullptr;
    }
    [[gnu::always_inline]] auto operator&() const {
        return reinterpret_cast<Ret (*)(This *, Args...)>(Derived::inner_.raw_function_);
    }

protected:
    [[gnu::always_inline]] auto &operator=(void *function) const {
        Derived::inner_.raw_function_ = function;
        return *this;
    }

    using ThisType = std::conditional_t<std::is_void_v<This>, Derived, This>;
    union InnerType {
        Ret (ThisType::*function_)(Args...) const;

        struct {
            void *raw_function_ = nullptr;
            [[maybe_unused]] std::ptrdiff_t adj = 0;
        };
    };

    static_assert(sizeof(InnerType::function_) ==
                  sizeof(InnerType::raw_function_) + sizeof(InnerType::adj));
};

template <FixedString Sym, class This, typename Ret, typename... Args>
struct Function<Sym, Ret (This::*)(Args...)>
    : BaseMemberFunction<Function<Sym, Ret (This::*)(Args...)>, This, Ret, Args...> {
    [[gnu::always_inline]] auto &operator=(void *function) const {
        Function::BaseMemberFunction::operator=(function);
        return *this;
    }

private:
    inline static Function::BaseMemberFunction::InnerType inner_;
    friend struct Function::BaseMemberFunction;
};

template <FixedString Sym, class This, typename Ret, typename... Args>
struct Function<Sym, Ret (This::*const)(Args...)>
    : BaseMemberFunction<Function<Sym, Ret (This::*const)(Args...)>, const This, Ret, Args...> {
    [[gnu::always_inline]] auto &operator=(void *function) const {
        Function::BaseMemberFunction::operator=(function);
        return *this;
    }

private:
    inline static Function::BaseMemberFunction::InnerType inner_;
    friend struct Function::BaseMemberFunction;
};

template <FixedString, typename T>
struct Field {
    [[gnu::always_inline]] T *operator->() const { return inner_.field_; }
    [[gnu::always_inline]] T &operator*() const { return *inner_.field_; }
    [[gnu::always_inline]] operator bool() const { return inner_.raw_field_ != nullptr; }
    [[gnu::always_inline]] auto operator&() const { return inner_.field_; }
    [[gnu::always_inline]] auto &operator=(void *field) const {
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
    [[gnu::always_inline]] auto &operator=(void *function) const {
        Hooker::Function::operator=(function);
        return *this;
    }

private:
    consteval Hooker(Ret (*replace)(Args...)) : replace_{replace} {};

    inline static void *address_ = nullptr;

    Ret (*replace_)(Args...);

    friend struct HookHandler;
    template <FixedString S>
    friend struct Symbol;
};

template <FixedString Sym, class This, typename Ret, typename... Args>
struct Hooker<Sym, Ret (This::*)(Args...)> : Function<Sym, Ret (This::*)(Args...)> {
    [[gnu::always_inline]] auto &operator=(void *function) const {
        Hooker::Function::operator=(function);
        return *this;
    }

private:
    consteval Hooker(Ret (*replace)(This *, Args...)) : replace_{replace} {};

    inline static void *address_ = nullptr;

    Ret (*replace_)(This *, Args...);

    friend struct HookHandler;
    template <FixedString S>
    friend struct Symbol;
};

struct HookHandler {
    HookHandler(const InitInfo &info) : info_(info) {}

    template <typename T>
    [[gnu::always_inline]] bool operator()(T &&arg) const {
        return handle(std::forward<T>(arg), false);
    }

    template <typename T1, typename T2, typename... U>
    [[gnu::always_inline]] bool operator()(T1 &&arg1, T2 &&arg2, U &&...args) const {
        if constexpr (std::is_same_v<T2, bool>) {
            return handle(std::forward<T1>(arg1), std::forward<T2>(arg2)) ||
                   this->operator()(std::forward<U>(args)...);
        } else {
            return handle(std::forward<T1>(arg1), false) ||
                   this->operator()(std::forward<T2>(arg2), std::forward<U>(args)...);
        }
    }

    template <FixedString Sym, typename... Us, template <FixedString, typename...> typename T>
        requires(requires { T<Sym, Us...>::replace_; })
    [[gnu::always_inline]] bool unhook(const T<Sym, Us...> &hooker) const {
        if (hooker.address_ && info_.inline_unhooker && info_.inline_unhooker(hooker.address_)) {
            hooker = nullptr;
            hooker.address_ = nullptr;
            return true;
        }
        return false;
    }

private:
    [[gnu::always_inline]] constexpr bool operator()() const { return false; }

    template <FixedString Sym, typename... Us, template <FixedString, typename...> typename T>
        requires(!requires { T<Sym, Us...>::replace_; })
    [[gnu::always_inline]] bool handle(const T<Sym, Us...> &target, bool match_prefix) const {
        return target = dlsym<Sym>(match_prefix);
    }

    template <FixedString Sym, typename... Us, template <FixedString, typename...> typename T>
        requires(requires { T<Sym, Us...>::replace_; })
    [[gnu::always_inline]] bool handle(const T<Sym, Us...> &hooker, bool match_prefix) const {
        return hooker = hook(hooker.address_ = dlsym<Sym>(match_prefix),
                             reinterpret_cast<void *>(hooker.replace_));
    }

    template <FixedString Sym>
    [[gnu::always_inline, nodiscard]] void *dlsym(bool match_prefix = false) const {
        if (auto match = info_.art_symbol_resolver(Sym.data)) {
            return match;
        }
        if (match_prefix && info_.art_symbol_prefix_resolver) [[likely]] {
            return info_.art_symbol_prefix_resolver(Sym.data);
        }
        return nullptr;
    }

    [[gnu::always_inline, nodiscard]] void *hook(void *original, void *replace) const {
        if (original) [[likely]] {
            return info_.inline_hooker(original, replace);
        }
        return nullptr;
    }

    const InitInfo &info_;
};

template <typename F>
concept Backup = std::is_function_v<std::remove_pointer_t<F>>;

template <typename F>
concept MemBackup = std::is_member_function_pointer_v<std::remove_pointer_t<F>> || Backup<F>;

template <FixedString S>
struct Symbol {
    template <typename T>
    inline static decltype([] {
        if constexpr (FuncType<T>) {
            return Function<S, T>{};
        } else {
            return Field<S, T>{};
        }
    }()) as{};

    [[no_unique_address]] struct Hook {
        template <typename F>
        consteval auto operator->*(F && /*unused*/) const {
            using Signature = decltype(F::template operator()<&decltype([] static {})::operator()>);
            if constexpr (requires { F::template operator()<&decltype([] {})::operator()>; }) {
                using HookerType =
                    Hooker<S, decltype([]<class This, typename Ret, typename... Args>(
                                           Ret (*)(This *, Args...)) -> Ret (This::*)(Args...) {
                               return {};
                           }.template operator()(std::declval<Signature>()))>;
                return HookerType {
                    static_cast<decltype(HookerType::replace_)>(
                        &F::template operator()<HookerType::operator()>)
                };
            } else {
                using HookerType = Hooker<S, Signature>;
                return HookerType {
                    static_cast<decltype(HookerType::replace_)>(
                        &F::template operator()<HookerType::operator()>)
                };
            }
        };
    } hook;
};

template <typename T, T... Cs>
    requires(std::is_same_v<T, char>)
consteval auto operator""_sym() {
    return Symbol<FixedString<sizeof...(Cs), Cs..., T{}>{}>{};
}

template <FixedString S, FixedString P>
consteval auto operator|([[maybe_unused]] Symbol<S> lp32, [[maybe_unused]] Symbol<P> lp64) {
    if constexpr (is_arch_v<Arch::kLP64>) {
        return lp64;
    } else {
        return lp32;
    }
}
}  // namespace lsplant