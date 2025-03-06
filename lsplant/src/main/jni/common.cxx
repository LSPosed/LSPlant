module;

#include <jni.h>
#include <parallel_hashmap/phmap.h>
#include <sys/system_properties.h>

#include <list>
#include <shared_mutex>
#include <string_view>

#include "logging.hpp"

export module lsplant:common;
export import jni_helper;
export import hook_helper;

export namespace lsplant {

namespace art {
class ArtMethod;
namespace mirror {
class Class;
}
namespace dex {
class ClassDef {};
}  // namespace dex

}  // namespace art

enum class Arch {
    kArm,
    kArm64,
    kX86,
    kX86_64,
    kRiscv64,
};

consteval inline Arch GetArch() {
#if defined(__i386__)
    return Arch::kX86;
#elif defined(__x86_64__)
    return Arch::kX86_64;
#elif defined(__arm__)
    return Arch::kArm;
#elif defined(__aarch64__)
    return Arch::kArm64;
#elif defined(__riscv)
    return Arch::kRiscv64;
#else
#error "unsupported architecture"
#endif
}

template <class K, class V, class Hash = phmap::priv::hash_default_hash<K>,
          class Eq = phmap::priv::hash_default_eq<K>,
          class Alloc = phmap::priv::Allocator<phmap::priv::Pair<const K, V>>, size_t N = 4>
using SharedHashMap = phmap::parallel_flat_hash_map<K, V, Hash, Eq, Alloc, N, std::shared_mutex>;

template <class T, class Hash = phmap::priv::hash_default_hash<T>,
          class Eq = phmap::priv::hash_default_eq<T>, class Alloc = phmap::priv::Allocator<T>,
          size_t N = 4>
using SharedHashSet = phmap::parallel_flat_hash_set<T, Hash, Eq, Alloc, N, std::shared_mutex>;

constexpr auto kArch = GetArch();

template <typename T>
constexpr inline auto RoundUpTo(T v, size_t size) {
    return v + size - 1 - ((v + size - 1) & (size - 1));
}

[[gnu::const]] inline auto GetAndroidApiLevel() {
    static auto kApiLevel = []() {
        std::array<char, PROP_VALUE_MAX> prop_value;
        __system_property_get("ro.build.version.sdk", prop_value.data());
        int base = atoi(prop_value.data());
        __system_property_get("ro.build.version.preview_sdk", prop_value.data());
        return base + atoi(prop_value.data());
    }();
    return kApiLevel;
}

inline auto IsJavaDebuggable(JNIEnv * env) {
    static auto kDebuggable = [&env]() {
        auto sdk_int = GetAndroidApiLevel();
        if (sdk_int < __ANDROID_API_P__) {
            return false;
        }
        auto runtime_class = JNI_FindClass(env, "dalvik/system/VMRuntime");
        if (!runtime_class) {
            LOGE("Failed to find VMRuntime");
            return false;
        }
        auto get_runtime_method = JNI_GetStaticMethodID(env, runtime_class, "getRuntime",
                                                        "()Ldalvik/system/VMRuntime;");
        if (!get_runtime_method) {
            LOGE("Failed to find VMRuntime.getRuntime()");
            return false;
        }
        auto is_debuggable_method =
            JNI_GetMethodID(env, runtime_class, "isJavaDebuggable", "()Z");
        if (!is_debuggable_method) {
            LOGE("Failed to find VMRuntime.isJavaDebuggable()");
            return false;
        }
        auto runtime = JNI_CallStaticObjectMethod(env, runtime_class, get_runtime_method);
        if (!runtime) {
            LOGE("Failed to get VMRuntime");
            return false;
        }
        bool is_debuggable = JNI_CallBooleanMethod(env, runtime, is_debuggable_method);
        LOGD("java runtime debuggable %s", is_debuggable ? "true" : "false");
        return is_debuggable;
    }();
    return kDebuggable;
}

constexpr auto kPointerSize = sizeof(void *);

SharedHashMap<art::ArtMethod *, std::pair<jobject, art::ArtMethod *>> hooked_methods_;

SharedHashMap<const art::dex::ClassDef *, phmap::flat_hash_set<art::ArtMethod *>>
    hooked_classes_;

SharedHashSet<art::ArtMethod *> deoptimized_methods_set_;

SharedHashMap<const art::dex::ClassDef *, phmap::flat_hash_set<art::ArtMethod *>>
    deoptimized_classes_;

std::list<std::pair<art::ArtMethod *, art::ArtMethod *>> jit_movements_;
std::shared_mutex jit_movements_lock_;

inline art::ArtMethod *IsHooked(art::ArtMethod * art_method, bool including_backup = false) {
    art::ArtMethod *backup = nullptr;
    hooked_methods_.if_contains(art_method, [&backup, &including_backup](const auto &it) {
        if (including_backup || it.second.first) backup = it.second.second;
    });
    return backup;
}

inline art::ArtMethod *IsBackup(art::ArtMethod * art_method) {
    art::ArtMethod *backup = nullptr;
    hooked_methods_.if_contains(art_method, [&backup](const auto &it) {
        if (!it.second.first) backup = it.second.second;
    });
    return backup;
}

inline bool IsDeoptimized(art::ArtMethod * art_method) {
    return deoptimized_methods_set_.contains(art_method);
}

inline std::list<std::pair<art::ArtMethod *, art::ArtMethod *>> GetJitMovements() {
    std::unique_lock lk(jit_movements_lock_);
    return std::move(jit_movements_);
}

inline void RecordHooked(art::ArtMethod * target, const art::dex::ClassDef *class_def,
                         jobject reflected_backup, art::ArtMethod *backup) {
    hooked_classes_.lazy_emplace_l(
        class_def, [&target](auto &it) { it.second.emplace(target); },
        [&class_def, &target](const auto &ctor) {
            ctor(class_def, phmap::flat_hash_set<art::ArtMethod *>{target});
        });
    hooked_methods_.insert({std::make_pair(target, std::make_pair(reflected_backup, backup)),
                            std::make_pair(backup, std::make_pair(nullptr, target))});
}

inline void RecordDeoptimized(const art::dex::ClassDef *class_def, art::ArtMethod *art_method) {
    { deoptimized_classes_[class_def].emplace(art_method); }
    deoptimized_methods_set_.insert(art_method);
}

inline void RecordJitMovement(art::ArtMethod * target, art::ArtMethod * backup) {
    std::unique_lock lk(jit_movements_lock_);
    jit_movements_.emplace_back(target, backup);
}
}  // namespace lsplant
