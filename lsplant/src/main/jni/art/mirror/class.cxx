module;

#include <parallel_hashmap/phmap.h>

#include "logging.hpp"

export module clazz;

import common;
import art_method;
import thread;
import handle;
import hook_helper;

namespace lsplant::art::mirror {

export class Class {
private:
    inline static MemberFunction<
        "_ZN3art6mirror5Class13GetDescriptorEPNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE",
        Class, const char *(std::string *)>
        GetDescriptor_;

    inline static MemberFunction<"_ZN3art6mirror5Class11GetClassDefEv", Class,
                                 const dex::ClassDef *()>
        GetClassDef_;

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

    inline static Hooker<
        "_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS_11ClassStatusEPNS_6ThreadE",
        void(TrivialHandle<Class>, uint8_t, Thread *)>
        SetClassStatus_ = +[](TrivialHandle<Class> h, uint8_t new_status, Thread *self) {
            if (new_status == initialized_status) {
                BackupClassMethods(GetClassDef_(h.Get()), self);
            }
            return SetClassStatus_(h, new_status, self);
        };

    inline static Hooker<"_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS1_6StatusEPNS_6ThreadE",
                         void(Handle<Class>, int, Thread *)>
        SetStatus_ = +[](Handle<Class> h, int new_status, Thread *self) {
            if (new_status == static_cast<int>(initialized_status)) {
                BackupClassMethods(GetClassDef_(h.Get()), self);
            }
            return SetStatus_(h, new_status, self);
        };

    inline static Hooker<"_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS1_6StatusEPNS_6ThreadE",
                         void(TrivialHandle<Class>, uint32_t, Thread *)>
        TrivialSetStatus_ = +[](TrivialHandle<Class> h, uint32_t new_status, Thread *self) {
            if (new_status == initialized_status) {
                BackupClassMethods(GetClassDef_(h.Get()), self);
            }
            return TrivialSetStatus_(h, new_status, self);
        };

    inline static Hooker<"_ZN3art6mirror5Class9SetStatusENS1_6StatusEPNS_6ThreadE",
                         void(Class *, int, Thread *)>
        ClassSetStatus_ = +[](Class *thiz, int new_status, Thread *self) {
            if (new_status == static_cast<int>(initialized_status)) {
                BackupClassMethods(GetClassDef_(thiz), self);
            }
            return ClassSetStatus_(thiz, new_status, self);
        };

public:
    static bool Init(const HookHandler &handler) {
        if (!handler.dlsym(GetDescriptor_) || !handler.dlsym(GetClassDef_)) {
            return false;
        }

        int sdk_int = GetAndroidApiLevel();

        if (sdk_int < __ANDROID_API_O__) {
            if (!handler.hook(SetStatus_, ClassSetStatus_)) {
                return false;
            }
        } else {
            if (!handler.hook(SetClassStatus_, TrivialSetStatus_)) {
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
