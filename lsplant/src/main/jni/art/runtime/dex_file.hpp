#pragma once

#include <memory>
#include <string>

#include "common.hpp"

namespace lsplant::art {
class DexFile {
    struct Header {
        uint8_t magic_[8];
        uint32_t checksum_;  // See also location_checksum_
    };
    CREATE_FUNC_SYMBOL_ENTRY(std::unique_ptr<DexFile>, OpenMemory, const uint8_t* dex_file,
                             size_t size, const std::string& location, uint32_t location_checksum,
                             void* mem_map, const void* oat_dex_file, std::string* error_msg) {
        if (OpenMemorySym) [[likely]] {
            return OpenMemorySym(dex_file, size, location, location_checksum, mem_map, oat_dex_file,
                                 error_msg);
        }
        return nullptr;
    }

public:
    static std::unique_ptr<DexFile> OpenMemory(const void* dex_file, size_t size,
                                               std::string location, std::string* error_msg) {
        return OpenMemory(reinterpret_cast<const uint8_t*>(dex_file), size, location,
                          reinterpret_cast<const Header*>(dex_file)->checksum_, nullptr, nullptr,
                          error_msg);
    }

    jobject ToJavaDexFile(JNIEnv* env) {
        auto java_dex_file = env->AllocObject(dex_file_class);
        auto cookie = JNI_NewLongArray(env, dex_file_start_index + 1);
        cookie[oat_file_index] = 0;
        cookie[dex_file_start_index] = reinterpret_cast<jlong>(this);
        cookie.commit();
        JNI_SetObjectField(env, java_dex_file, cookie_field, cookie);
        if (internal_cookie_field) {
            JNI_SetObjectField(env, java_dex_file, internal_cookie_field, cookie);
        }
        JNI_SetObjectField(env, java_dex_file, file_name_field, JNI_NewStringUTF(env, ""));
        return java_dex_file;
    }

    static bool Init(JNIEnv* env, const HookHandler& handler) {
        auto sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_O__) [[likely]] {
            return true;
        }
        if (!RETRIEVE_FUNC_SYMBOL(
                OpenMemory,
                LP_SELECT("_ZN3art7DexFile10OpenMemoryEPKhjRKNSt3__112basic_stringIcNS3_11char_"
                          "traitsIcEENS3_9allocatorIcEEEEjPNS_6MemMapEPKNS_10OatDexFileEPS9_",
                          "_ZN3art7DexFile10OpenMemoryEPKhmRKNSt3__112basic_stringIcNS3_11char_"
                          "traitsIcEENS3_9allocatorIcEEEEjPNS_6MemMapEPKNS_10OatDexFileEPS9_")))
            [[unlikely]] {
            return false;
        }
        dex_file_class = JNI_NewGlobalRef(env, JNI_FindClass(env, "dalvik/system/DexFile"));
        if (!dex_file_class) {
            return false;
        }
        cookie_field = JNI_GetFieldID(env, dex_file_class, "mCookie", "Ljava/lang/Object;");
        if (!cookie_field) {
            return false;
        }
        file_name_field = JNI_GetFieldID(env, dex_file_class, "mFileName", "Ljava/lang/String;");
        if (!file_name_field) {
            return false;
        }
        if (sdk_int >= __ANDROID_API_N__) {
            internal_cookie_field =
                JNI_GetFieldID(env, dex_file_class, "mInternalCookie", "Ljava/lang/Object;");
            if (!internal_cookie_field) {
                return false;
            }
            dex_file_start_index = 1u;
        }
        return true;
    }

private:
    inline static jclass dex_file_class = nullptr;
    inline static jfieldID cookie_field = nullptr;
    inline static jfieldID file_name_field = nullptr;
    inline static jfieldID internal_cookie_field = nullptr;
    inline static size_t oat_file_index = 0u;
    inline static size_t dex_file_start_index = 0u;
};
}  // namespace lsplant::art
