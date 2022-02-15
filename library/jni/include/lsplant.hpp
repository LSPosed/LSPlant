#pragma once

#include <jni.h>
#include <string_view>

namespace lsplant {

struct InitInfo {
    using InlineHookFunType = std::function<void *(void *target, void *hooker)>;
    using InlineUnhookFunType = std::function<bool(void *func)>;
    using ArtSymbolResolver = std::function<void *(std::string_view symbol_name)>;

    InlineHookFunType inline_hooker;
    InlineUnhookFunType inline_unhooker;
    ArtSymbolResolver art_symbol_resolver;
};

[[nodiscard]] [[maybe_unused]] [[gnu::visibility("default")]]
bool Init(JNIEnv *env, const InitInfo &info);

[[nodiscard]] [[maybe_unused]] [[gnu::visibility("default")]]
jmethodID
Hook(JNIEnv *env, jmethodID target_method, jobject hooker_object, jmethodID callback_method);

[[nodiscard]] [[maybe_unused]] [[gnu::visibility("default")]]
bool UnHook(JNIEnv *env, jmethodID target_method);

[[nodiscard]] [[maybe_unused]] [[gnu::visibility("default")]]
bool IsHooked(JNIEnv *env, jmethodID method);

[[nodiscard]] [[maybe_unused]] [[gnu::visibility("default")]]
bool Deoptimize(JNIEnv *env, jmethodID method);

[[nodiscard]] [[maybe_unused]] [[gnu::visibility("default")]]
void *GetNativeFunction(JNIEnv *env, jmethodID method);

} // namespace lsplant
