module;

#include <parallel_hashmap/phmap.h>

#include "logging.hpp"

export module lsplant:clazz;

import :common;
import :art_method;
import :thread;
import :handle;
import hook_helper;

export namespace lsplant::art::mirror {

class Class {
private:
    inline static auto GetDescriptor_ =
        "_ZN3art6mirror5Class13GetDescriptorEPNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE"_sym.as<const char *(Class::*)(std::string *)>;

    inline static auto GetClassDef_ =
            "_ZN3art6mirror5Class11GetClassDefEv"_sym.as<const dex::ClassDef *(Class::*)()>;

    using BackupMethods = phmap::flat_hash_map<art::ArtMethod *, void *>;
    inline static phmap::flat_hash_map<const art::Thread *,
                                       phmap::flat_hash_map<const dex::ClassDef *, BackupMethods>>
        backup_methods_;
    inline static std::mutex backup_methods_lock_;

    inline static uint8_t initialized_status = 0;

    static void BackupClassMethods(const dex::ClassDef *class_def, art::Thread *self) {
        BackupMethods out;
        if (!class_def) return;
        {
            hooked_classes_.if_contains(class_def, [&out](const auto &it) {
                for (auto method : it.second) {
                    if (method->IsStatic()) {
                        LOGV("Backup hooked method %p because of initialization", method);
                        out.emplace(method, method->GetEntryPoint());
                    }
                }
            });
        }
        {
            deoptimized_classes_.if_contains(class_def, [&out](const auto &it) {
                for (auto method : it.second) {
                    if (method->IsStatic()) {
                        LOGV("Backup deoptimized method %p because of initialization", method);
                        out.emplace(method, method->GetEntryPoint());
                    }
                }
            });
        }
        if (!out.empty()) [[unlikely]] {
            std::unique_lock lk(backup_methods_lock_);
            backup_methods_[self].emplace(class_def, std::move(out));
        }
    }

    inline static auto SetClassStatus_ =
            "_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS_11ClassStatusEPNS_6ThreadE"_sym.hook->*[]
        <Backup auto backup>
        (TrivialHandle<Class> h, uint8_t new_status, Thread *self) static -> void {
            if (new_status == initialized_status) {
                BackupClassMethods(GetClassDef_(h.Get()), self);
            }
            return backup(h, new_status, self);
        };

    inline static auto SetStatus_ =
        "_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS1_6StatusEPNS_6ThreadE"_sym.hook->*[]
        <Backup auto backup>
         (Handle<Class> h, int new_status, Thread *self) static -> void {
            if (new_status == static_cast<int>(initialized_status)) {
                BackupClassMethods(GetClassDef_(h.Get()), self);
            }
            return backup(h, new_status, self);
        };

    inline static auto TrivialSetStatus_ =
        "_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS1_6StatusEPNS_6ThreadE"_sym.hook->*[]
        <Backup auto backup>
        (TrivialHandle<Class> h, uint32_t new_status, Thread *self) static -> void {
            if (new_status == initialized_status) {
                BackupClassMethods(GetClassDef_(h.Get()), self);
            }
            return backup(h, new_status, self);
        };

    inline static auto ClassSetStatus_ =
        "_ZN3art6mirror5Class9SetStatusENS1_6StatusEPNS_6ThreadE"_sym.hook->*[]
        <MemBackup auto backup>
        (Class *thiz, int new_status, Thread *self) static -> void {
            if (new_status == static_cast<int>(initialized_status)) {
                BackupClassMethods(GetClassDef_(thiz), self);
            }
            return backup(thiz, new_status, self);
        };

public:
    static bool Init(const HookHandler &handler) {
        if (!handler(GetDescriptor_) || !handler(GetClassDef_)) {
            return false;
        }

        int sdk_int = GetAndroidApiLevel();

        if (sdk_int < __ANDROID_API_O__) {
            if (!handler(SetStatus_, ClassSetStatus_)) {
                return false;
            }
        } else {
            if (!handler(SetClassStatus_, TrivialSetStatus_)) {
                return false;
            }
        }

        if (sdk_int >= __ANDROID_API_R__) {
            initialized_status = 15;
        } else if (sdk_int >= __ANDROID_API_P__) {
            initialized_status = 14;
        } else if (sdk_int == __ANDROID_API_O_MR1__) {
            initialized_status = 11;
        } else {
            initialized_status = 10;
        }

        return true;
    }

    const char *GetDescriptor(std::string *storage) { return GetDescriptor_(this, storage); }

    std::string GetDescriptor() {
        std::string storage;
        return GetDescriptor(&storage);
    }

    const dex::ClassDef *GetClassDef() { return GetClassDef_(this); }

    static auto PopBackup(const dex::ClassDef *class_def, art::Thread *self) {
        BackupMethods methods;
        if (!backup_methods_.size()) [[likely]] {
            return methods;
        }
        if (class_def) {
            std::unique_lock lk(backup_methods_lock_);
            for (auto it = backup_methods_.begin(); it != backup_methods_.end();) {
                if (auto found = it->second.find(class_def); found != it->second.end()) {
                    methods.merge(std::move(found->second));
                    it->second.erase(found);
                }
                if (it->second.empty()) {
                    backup_methods_.erase(it++);
                } else {
                    it++;
                }
            }
        } else if (self) {
            std::unique_lock lk(backup_methods_lock_);
            if (auto found = backup_methods_.find(self); found != backup_methods_.end()) {
                for (auto it = found->second.begin(); it != found->second.end();) {
                    methods.merge(std::move(it->second));
                    found->second.erase(it++);
                }
                backup_methods_.erase(found);
            }
        }
        return methods;
    }
};

}  // namespace lsplant::art::mirror
