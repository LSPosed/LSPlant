#pragma once

#include <string_view>
#include <shared_mutex>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <sys/system_properties.h>
#include "logging.hpp"
#include "utils/hook_helper.hpp"
#include "lsplant.hpp"

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
# error "unsupported architecture"
#endif
}

inline static constexpr auto kArch = GetArch();

template<typename T>
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

template<typename T>
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
}

namespace {
// target, backup
inline std::unordered_map<art::ArtMethod *, jobject> hooked_methods_;
inline std::shared_mutex hooked_methods_lock_;

inline std::list<std::pair<art::ArtMethod *, art::ArtMethod *>> jit_movements_;
inline std::shared_mutex jit_movements_lock_;

inline std::unordered_map<const art::dex::ClassDef *, std::list<std::tuple<art::ArtMethod *, art::ArtMethod *, art::ArtMethod *>>> pending_classes_;
inline std::shared_mutex pending_classes_lock_;

inline std::unordered_set<const art::ArtMethod *> pending_methods_;
inline std::shared_mutex pending_methods_lock_;
}

inline bool IsHooked(art::ArtMethod *art_method) {
    std::shared_lock lk(hooked_methods_lock_);
    return hooked_methods_.contains(art_method);
}

inline bool IsPending(art::ArtMethod *art_method) {
    std::shared_lock lk(pending_methods_lock_);
    return pending_methods_.contains(art_method);
}

inline bool IsPending(const art::dex::ClassDef *class_def) {
    std::shared_lock lk(pending_classes_lock_);
    return pending_classes_.contains(class_def);
}

inline std::list<std::pair<art::ArtMethod *, art::ArtMethod *>> GetJitMovements() {
    std::unique_lock lk(jit_movements_lock_);
    return std::move(jit_movements_);
}

inline void RecordHooked(art::ArtMethod *target, jobject backup) {
    std::unique_lock lk(hooked_methods_lock_);
    hooked_methods_.emplace(target, backup);
}

inline void RecordJitMovement(art::ArtMethod *target, art::ArtMethod *backup) {
    std::unique_lock lk(jit_movements_lock_);
    jit_movements_.emplace_back(target, backup);
}

inline void
RecordPending(const art::dex::ClassDef *class_def, art::ArtMethod *target, art::ArtMethod *hook,
              art::ArtMethod *backup) {
    {
        std::unique_lock lk(pending_methods_lock_);
        pending_methods_.emplace(target);
    }
    std::unique_lock lk(pending_classes_lock_);
    pending_classes_[class_def].emplace_back(std::make_tuple(target, hook, backup));
}

void OnPending(art::ArtMethod *target, art::ArtMethod *hook, art::ArtMethod *backup);

inline void OnPending(const art::dex::ClassDef *class_def) {
    {
        std::shared_lock lk(pending_classes_lock_);
        if (!pending_classes_.contains(class_def)) return;
    }
    typename decltype(pending_classes_)::value_type::second_type set;
    {
        std::unique_lock lk(pending_classes_lock_);
        auto it = pending_classes_.find(class_def);
        if (it == pending_classes_.end()) return;
        set = std::move(it->second);
        pending_classes_.erase(it);
    }
    for (auto&[target, hook, backup]: set) {
        {
            std::unique_lock mlk(pending_methods_lock_);
            pending_methods_.erase(target);
        }
        OnPending(target, hook, backup);
    }
}
}
