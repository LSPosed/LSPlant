#pragma once

#include <sys/system_properties.h>

#include <list>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "logging.hpp"
#include "lsplant.hpp"
#include "utils/hook_helper.hpp"

namespace lsplant {

enum class Arch {
    kArm,
    kArm64,
    kX86,
    kX8664,
};

consteval inline Arch GetArch() {
#if defined(__i386__)
    return Arch::kX86;
#elif defined(__x86_64__)
    return Arch::kX8664;
#elif defined(__arm__)
    return Arch::kArm;
#elif defined(__aarch64__)
    return Arch::kArm64;
#else
#error "unsupported architecture"
#endif
}

inline static constexpr auto kArch = GetArch();

template <typename T>
constexpr inline auto RoundUpTo(T v, size_t size) {
    return v + size - 1 - ((v + size - 1) & (size - 1));
}

inline auto GetAndroidApiLevel() {
    static auto kApiLevel = []() {
        std::array<char, PROP_VALUE_MAX> prop_value;
        __system_property_get("ro.build.version.sdk", prop_value.data());
        int base = atoi(prop_value.data());
        __system_property_get("ro.build.version.preview_sdk", prop_value.data());
        return base + atoi(prop_value.data());
    }();
    return kApiLevel;
}

inline static constexpr auto kPointerSize = sizeof(void *);

template <typename T>
inline T GetArtSymbol(const std::function<void *(std::string_view)> &resolver,
                      std::string_view symbol) requires(std::is_pointer_v<T>) {
    if (auto *result = resolver(symbol); result) {
        return reinterpret_cast<T>(result);
    } else {
        LOGW("Failed to find symbol %*s", static_cast<int>(symbol.length()), symbol.data());
        return nullptr;
    }
}

namespace art {
    class ArtMethod;
    namespace dex {
    class ClassDef;
    }
}  // namespace art

namespace {
// target, backup
inline std::unordered_map<art::ArtMethod *, std::pair<jobject, art::ArtMethod *>> hooked_methods_;
inline std::shared_mutex hooked_methods_lock_;

inline std::list<std::pair<art::ArtMethod *, art::ArtMethod *>> jit_movements_;
inline std::shared_mutex jit_movements_lock_;
}  // namespace

inline bool IsHooked(art::ArtMethod *art_method) {
    std::shared_lock lk(hooked_methods_lock_);
    return hooked_methods_.contains(art_method);
}

inline std::list<std::pair<art::ArtMethod *, art::ArtMethod *>> GetJitMovements() {
    std::unique_lock lk(jit_movements_lock_);
    return std::move(jit_movements_);
}

inline void RecordHooked(art::ArtMethod *target, jobject reflected_backup, art::ArtMethod* backup) {
    std::unique_lock lk(hooked_methods_lock_);
    hooked_methods_.emplace(target, std::make_pair(reflected_backup, backup));
}

inline void RecordJitMovement(art::ArtMethod *target, art::ArtMethod *backup) {
    std::unique_lock lk(jit_movements_lock_);
    jit_movements_.emplace_back(target, backup);
}

}  // namespace lsplant
