#include "lsplant.hpp"

#include <android/api-level.h>
#include <sys/mman.h>
#include <sys/system_properties.h>

#include <array>
#include <atomic>

#include "art/instrumentation.hpp"
#include "art/runtime/art_method.hpp"
#include "art/runtime/class_linker.hpp"
#include "art/runtime/gc/scoped_gc_critical_section.hpp"
#include "art/runtime/jit/jit_code_cache.hpp"
#include "art/thread.hpp"
#include "art/thread_list.hpp"
#include "common.hpp"
#include "dex_builder.h"
#include "utils/jni_helper.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "Simplify"
#pragma ide diagnostic ignored "UnreachableCode"
namespace lsplant {

using art::ArtMethod;
using art::ClassLinker;
using art::Instrumentation;
using art::Thread;
using art::gc::ScopedGCCriticalSection;
using art::jit::JitCodeCache;
using art::mirror::Class;
using art::thread_list::ScopedSuspendAll;

namespace {
template <typename T, T... chars>
inline consteval auto operator""_uarr() {
    return std::array<uint8_t, sizeof...(chars)>{static_cast<uint8_t>(chars)...};
}

consteval inline auto GetTrampoline() {
    if constexpr (kArch == Arch::kArm) {
        return std::make_tuple("\x00\x00\x9f\xe5\x20\xf0\x90\xe5\x78\x56\x34\x12"_uarr,
                               // NOLINTNEXTLINE
                               uint8_t{32u}, uintptr_t{8u});
    }
    if constexpr (kArch == Arch::kArm64) {
        return std::make_tuple(
            "\x60\x00\x00\x58\x10\x00\x40\xf8\x00\x02\x1f\xd6\x78\x56\x34\x12\x78\x56\x34\x12"_uarr,
            // NOLINTNEXTLINE
            uint8_t{44u}, uintptr_t{12u});
    }
    if constexpr (kArch == Arch::kX86) {
        return std::make_tuple("\xb8\x78\x56\x34\x12\xff\x70\x20\xc3"_uarr,
                               // NOLINTNEXTLINE
                               uint8_t{56u}, uintptr_t{1u});
    }
    if constexpr (kArch == Arch::kX8664) {
        return std::make_tuple("\x48\xbf\x78\x56\x34\x12\x78\x56\x34\x12\xff\x77\x20\xc3"_uarr,
                               // NOLINTNEXTLINE
                               uint8_t{96u}, uintptr_t{2u});
    }
}

auto [trampoline, entry_point_offset, art_method_offset] = GetTrampoline();

jmethodID method_get_name = nullptr;
jmethodID method_get_declaring_class = nullptr;
jmethodID class_get_name = nullptr;
jmethodID class_get_class_loader = nullptr;
jmethodID class_get_declared_constructors = nullptr;
jfieldID class_access_flags = nullptr;
jclass in_memory_class_loader = nullptr;
jmethodID in_memory_class_loader_init = nullptr;
jmethodID load_class = nullptr;
jmethodID set_accessible = nullptr;
jclass executable = nullptr;

std::string generated_class_name;
std::string generated_source_name;
std::string generated_field_name;
std::string generated_method_name;

bool InitConfig(const InitInfo &info) {
    if (info.generated_class_name.empty()) {
        LOGE("generated class name cannot be empty");
        return false;
    }
    generated_class_name = info.generated_class_name;
    if (info.generated_field_name.empty()) {
        LOGE("generated field name cannot be empty");
        return false;
    }
    generated_field_name = info.generated_field_name;
    if (info.generated_method_name.empty()) {
        LOGE("generated method name cannot be empty");
        return false;
    }
    generated_method_name = info.generated_method_name;
    generated_source_name = info.generated_source_name;
    return true;
}

bool InitJNI(JNIEnv *env) {
    executable = JNI_NewGlobalRef(env, JNI_FindClass(env, "java/lang/reflect/Executable"));
    if (!executable) {
        LOGE("Failed to found Executable");
        return false;
    }

    if (method_get_name = JNI_GetMethodID(env, executable, "getName", "()Ljava/lang/String;");
        !method_get_name) {
        LOGE("Failed to find getName method");
        return false;
    }
    if (method_get_declaring_class =
            JNI_GetMethodID(env, executable, "getDeclaringClass", "()Ljava/lang/Class;");
        !method_get_declaring_class) {
        LOGE("Failed to find getName method");
        return false;
    }
    auto clazz = JNI_FindClass(env, "java/lang/Class");
    if (!clazz) {
        LOGE("Failed to find Class");
        return false;
    }

    if (class_get_class_loader =
            JNI_GetMethodID(env, clazz, "getClassLoader", "()Ljava/lang/ClassLoader;");
        !class_get_class_loader) {
        LOGE("Failed to find getClassLoader");
        return false;
    }

    if (class_get_declared_constructors = JNI_GetMethodID(env, clazz, "getDeclaredConstructors",
                                                          "()[Ljava/lang/reflect/Constructor;");
        !class_get_declared_constructors) {
        LOGE("Failed to find getDeclaredConstructors");
        return false;
    }

    if (class_get_name = JNI_GetMethodID(env, clazz, "getName", "()Ljava/lang/String;");
        !class_get_name) {
        LOGE("Failed to find getName");
        return false;
    }

    if (class_access_flags = JNI_GetFieldID(env, clazz, "accessFlags", "I"); !class_access_flags) {
        LOGE("Failed to find Class.accessFlags");
        return false;
    }

    in_memory_class_loader =
        JNI_NewGlobalRef(env, JNI_FindClass(env, "dalvik/system/InMemoryDexClassLoader"));
    in_memory_class_loader_init = JNI_GetMethodID(
        env, in_memory_class_loader, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");

    load_class = JNI_GetMethodID(env, in_memory_class_loader, "loadClass",
                                 "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!load_class) {
        load_class = JNI_GetMethodID(env, in_memory_class_loader, "findClass",
                                     "(Ljava/lang/String;)Ljava/lang/Class;");
    }
    auto accessible_object = JNI_FindClass(env, "java/lang/reflect/AccessibleObject");
    if (!accessible_object) {
        LOGE("Failed to find AccessibleObject");
        return false;
    }
    if (set_accessible = JNI_GetMethodID(env, accessible_object, "setAccessible", "(Z)V");
        !set_accessible) {
        LOGE("Failed to find AccessibleObject.setAccessible");
        return false;
    }
    return true;
}

inline void UpdateTrampoline(uint8_t offset) {
    trampoline[entry_point_offset / CHAR_BIT] |= offset << (entry_point_offset % CHAR_BIT);
    trampoline[entry_point_offset / CHAR_BIT + 1] |=
        offset >> (CHAR_BIT - entry_point_offset % CHAR_BIT);
}

bool InitNative(JNIEnv *env, const HookHandler &handler) {
    if (!handler.inline_hooker || !handler.inline_unhooker || !handler.art_symbol_resolver) {
        return false;
    }
    if (!ArtMethod::Init(env, handler)) {
        LOGE("Failed to init art method");
        return false;
    }
    UpdateTrampoline(ArtMethod::GetEntryPointOffset());
    if (!Thread::Init(handler)) {
        LOGE("Failed to init thread");
        return false;
    }
    if (!ClassLinker::Init(handler)) {
        LOGE("Failed to init class linker");
        return false;
    }
    if (!Class::Init(env, handler)) {
        LOGE("failed to init mirror class");
        return false;
    }
    if (!Instrumentation::Init(handler)) {
        LOGE("failed to init instrumentation");
        return false;
    }
    if (!ScopedSuspendAll::Init(handler)) {
        LOGE("failed to init scoped suspend all");
        return false;
    }
    if (!ScopedGCCriticalSection::Init(handler)) {
        LOGE("failed to init scoped gc critical section");
        return false;
    }
    if (!JitCodeCache::Init(handler)) {
        LOGE("failed to init jit code cache");
        return false;
    }
    return true;
}

std::tuple<jclass, jfieldID, jmethodID, jmethodID> BuildDex(JNIEnv *env, jobject class_loader,
                                                            std::string_view shorty, bool is_static,
                                                            std::string_view method_name,
                                                            std::string_view hooker_class,
                                                            std::string_view callback_name) {
    // NOLINTNEXTLINE
    using namespace startop::dex;

    if (shorty.empty()) {
        LOGE("Invalid shorty");
        return {nullptr, nullptr, nullptr, nullptr};
    }

    DexBuilder dex_file;

    auto parameter_types = std::vector<TypeDescriptor>();
    parameter_types.reserve(shorty.size() - 1);
    std::string storage;
    auto return_type =
        shorty[0] == 'L' ? TypeDescriptor::Object : TypeDescriptor::FromDescriptor(shorty[0]);
    if (!is_static) parameter_types.push_back(TypeDescriptor::Object);  // this object
    for (const char &param : shorty.substr(1)) {
        parameter_types.push_back(param == 'L'
                                      ? TypeDescriptor::Object
                                      : TypeDescriptor::FromDescriptor(static_cast<char>(param)));
    }

    ClassBuilder cbuilder{dex_file.MakeClass(generated_class_name)};
    if (!generated_source_name.empty()) cbuilder.set_source_file(generated_source_name);

    auto hooker_type = TypeDescriptor::FromClassname(hooker_class.data());

    auto *hooker_field = cbuilder.CreateField(generated_field_name, hooker_type)
                             .access_flags(dex::kAccStatic)
                             .Encode();

    auto hook_builder{cbuilder.CreateMethod(
        generated_method_name == "{target}" ? method_name.data() : generated_method_name,
        Prototype{return_type, parameter_types})};
    // allocate tmp first because of wide
    auto tmp{hook_builder.AllocRegister()};
    hook_builder.BuildConst(tmp, static_cast<int>(parameter_types.size()));
    auto hook_params_array{hook_builder.AllocRegister()};
    hook_builder.BuildNewArray(hook_params_array, TypeDescriptor::Object, tmp);
    for (size_t i = 0U, j = 0U; i < parameter_types.size(); ++i, ++j) {
        hook_builder.BuildBoxIfPrimitive(Value::Parameter(j), parameter_types[i],
                                         Value::Parameter(j));
        hook_builder.BuildConst(tmp, static_cast<int>(i));
        hook_builder.BuildAput(Instruction::Op::kAputObject, hook_params_array, Value::Parameter(j),
                               tmp);
        if (parameter_types[i].is_wide()) ++j;
    }
    auto handle_hook_method{dex_file.GetOrDeclareMethod(
        hooker_type, callback_name.data(),
        Prototype{TypeDescriptor::Object, TypeDescriptor::Object.ToArray()})};
    hook_builder.AddInstruction(
        Instruction::GetStaticObjectField(hooker_field->decl->orig_index, tmp));
    hook_builder.AddInstruction(
        Instruction::InvokeVirtualObject(handle_hook_method.id, tmp, tmp, hook_params_array));
    if (return_type == TypeDescriptor::Void) {
        hook_builder.BuildReturn();
    } else if (return_type.is_primitive()) {
        auto box_type{return_type.ToBoxType()};
        const ir::Type *type_def = dex_file.GetOrAddType(box_type);
        hook_builder.AddInstruction(Instruction::Cast(tmp, Value::Type(type_def->orig_index)));
        hook_builder.BuildUnBoxIfPrimitive(tmp, box_type, tmp);
        hook_builder.BuildReturn(tmp, false, return_type.is_wide());
    } else {
        const ir::Type *type_def = dex_file.GetOrAddType(return_type);
        hook_builder.AddInstruction(Instruction::Cast(tmp, Value::Type(type_def->orig_index)));
        hook_builder.BuildReturn(tmp, true);
    }
    auto *hook_method = hook_builder.Encode();

    auto backup_builder{cbuilder.CreateMethod("backup", Prototype{return_type, parameter_types})};
    if (return_type == TypeDescriptor::Void) {
        backup_builder.BuildReturn();
    } else if (return_type.is_wide()) {
        LiveRegister zero = backup_builder.AllocRegister();
        LiveRegister zero_wide = backup_builder.AllocRegister();
        backup_builder.BuildConstWide(zero, 0);
        backup_builder.BuildReturn(zero, /*is_object=*/false, true);
    } else {
        LiveRegister zero = backup_builder.AllocRegister();
        LiveRegister zero_wide = backup_builder.AllocRegister();
        backup_builder.BuildConst(zero, 0);
        backup_builder.BuildReturn(zero, /*is_object=*/!return_type.is_primitive(), false);
    }
    auto *backup_method = backup_builder.Encode();

    slicer::MemView image{dex_file.CreateImage()};

    auto dex_buffer = JNI_NewDirectByteBuffer(env, const_cast<void *>(image.ptr()),
                                              static_cast<jlong>(image.size()));
    auto my_cl = JNI_NewObject(env, in_memory_class_loader, in_memory_class_loader_init, dex_buffer,
                               class_loader);
    if (my_cl) {
        auto *target_class =
            JNI_Cast<jclass>(
                JNI_CallObjectMethod(env, my_cl, load_class,
                                     JNI_NewStringUTF(env, generated_class_name.data())))
                .release();
        if (target_class) {
            return {
                target_class,
                JNI_GetStaticFieldID(env, target_class, hooker_field->decl->name->c_str(),
                                     hooker_field->decl->type->descriptor->c_str()),
                JNI_GetStaticMethodID(env, target_class, hook_method->decl->name->c_str(),
                                      hook_method->decl->prototype->Signature().data()),
                JNI_GetStaticMethodID(env, target_class, backup_method->decl->name->c_str(),
                                      backup_method->decl->prototype->Signature().data()),
            };
        }
    }
    return {nullptr, nullptr, nullptr, nullptr};
}

static_assert(std::endian::native == std::endian::little, "Unsupported architecture");

union Trampoline {
public:
    uintptr_t address;
    unsigned count : 12;
};

static_assert(sizeof(Trampoline) == sizeof(uintptr_t), "Unsupported architecture");
static_assert(std::atomic_uintptr_t::is_always_lock_free, "Unsupported architecture");

std::atomic_uintptr_t trampoline_pool{0};
std::atomic_flag trampoline_lock{false};
constexpr size_t kTrampolineSize = RoundUpTo(sizeof(trampoline), kPointerSize);
constexpr size_t kPageSize = 4096;  // assume
constexpr size_t kTrampolineNumPerPage = kPageSize / kTrampolineSize;
constexpr uintptr_t kAddressMask = 0xFFFU;

void *GenerateTrampolineFor(art::ArtMethod *hook) {
    unsigned count;
    uintptr_t address;
    while (true) {
        auto tl = Trampoline{.address = trampoline_pool.fetch_add(1, std::memory_order_release)};
        count = tl.count;
        address = tl.address & ~kAddressMask;
        if (address == 0 || count >= kTrampolineNumPerPage) {
            if (trampoline_lock.test_and_set(std::memory_order_acq_rel)) {
                trampoline_lock.wait(true, std::memory_order_acquire);
                continue;
            }
            address = reinterpret_cast<uintptr_t>(mmap(nullptr, kPageSize,
                                                       PROT_READ | PROT_WRITE | PROT_EXEC,
                                                       MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
            if (address == reinterpret_cast<uintptr_t>(MAP_FAILED)) {
                PLOGE("mmap trampoline");
                trampoline_lock.clear(std::memory_order_release);
                trampoline_lock.notify_all();
                return nullptr;
            }
            count = 0;
            tl.address = address;
            tl.count = count + 1;
            trampoline_pool.store(tl.address, std::memory_order_release);
            trampoline_lock.clear(std::memory_order_release);
            trampoline_lock.notify_all();
        }
        LOGV("trampoline: count = %u, address = %zx, target = %zx", count, address,
             address + count * kTrampolineSize);
        address = address + count * kTrampolineSize;
        break;
    }
    auto *address_ptr = reinterpret_cast<char *>(address);
    std::memcpy(address_ptr, trampoline.data(), trampoline.size());

    *reinterpret_cast<art::ArtMethod **>(address_ptr + art_method_offset) = hook;

    __builtin___clear_cache(address_ptr, reinterpret_cast<char *>(address + trampoline.size()));

    return address_ptr;
}

bool DoHook(ArtMethod *target, ArtMethod *hook, ArtMethod *backup) {
    ScopedGCCriticalSection section(art::Thread::Current(), art::gc::kGcCauseDebugger,
                                    art::gc::kCollectorTypeDebugger);
    ScopedSuspendAll suspend("LSPlant Hook", false);
    LOGV("Hooking: target = %p, hook = %p, backup = %p", target, hook, backup);

    if (auto *trampoline = GenerateTrampolineFor(hook); !trampoline) {
        LOGE("Failed to generate trampoline");
        return false;
        // NOLINTNEXTLINE
    } else {
        LOGV("Generated trampoline %p", trampoline);

        target->SetNonCompilable();
        hook->SetNonCompilable();

        // copy after setNonCompilable
        backup->CopyFrom(target);

        target->SetNonIntrinsic();

        target->SetEntryPoint(trampoline);

        if (!backup->IsStatic()) backup->SetPrivate();

        LOGV("Done hook: target(%p:0x%x) -> %p; backup(%p:0x%x) -> %p; hook(%p:0x%x) -> %p", target,
             target->GetAccessFlags(), target->GetEntryPoint(), backup, backup->GetAccessFlags(),
             backup->GetEntryPoint(), hook, hook->GetAccessFlags(), hook->GetEntryPoint());

        return true;
    }
}

bool DoUnHook(ArtMethod *target, ArtMethod *backup) {
    ScopedGCCriticalSection section(art::Thread::Current(), art::gc::kGcCauseDebugger,
                                    art::gc::kCollectorTypeDebugger);
    ScopedSuspendAll suspend("LSPlant Hook", false);
    LOGV("Unhooking: target = %p, backup = %p", target, backup);
    auto access_flags = target->GetAccessFlags();
    target->CopyFrom(backup);
    target->SetAccessFlags(access_flags);
    LOGV("Done unhook: target(%p:0x%x) -> %p; backup(%p:0x%x) -> %p;", target,
         target->GetAccessFlags(), target->GetEntryPoint(), backup, backup->GetAccessFlags(),
         backup->GetEntryPoint());
    return true;
}

}  // namespace

void OnPending(art::ArtMethod *target, art::ArtMethod *hook, art::ArtMethod *backup) {
    LOGD("On pending hook for %p", target);
    if (!DoHook(target, hook, backup)) {
        LOGE("Pending hook failed");
    }
}

inline namespace v1 {

using ::lsplant::IsHooked;

[[maybe_unused]] bool Init(JNIEnv *env, const InitInfo &info) {
    bool static kInit = InitConfig(info) && InitJNI(env) && InitNative(env, info);
    return kInit;
}

[[maybe_unused]] jobject Hook(JNIEnv *env, jobject target_method, jobject hooker_object,
                              jobject callback_method) {
    if (!target_method || !JNI_IsInstanceOf(env, target_method, executable)) {
        LOGE("target method is not an executable");
        return nullptr;
    }
    if (!callback_method || !JNI_IsInstanceOf(env, callback_method, executable)) {
        LOGE("callback method is not an executable");
        return nullptr;
    }

    jmethodID hook_method = nullptr;
    jmethodID backup_method = nullptr;
    jfieldID hooker_field = nullptr;

    auto target_class =
        JNI_Cast<jclass>(JNI_CallObjectMethod(env, target_method, method_get_declaring_class));
    constexpr static uint32_t kAccClassIsProxy = 0x00040000;
    bool is_proxy = JNI_GetIntField(env, target_class, class_access_flags) & kAccClassIsProxy;
    auto *target = ArtMethod::FromReflectedMethod(env, target_method);
    bool is_static = target->IsStatic();

    if (IsHooked(target) || IsPending(target)) {
        LOGW("Skip duplicate hook");
        return nullptr;
    }

    ScopedLocalRef<jclass> built_class{env};
    {
        auto callback_name =
            JNI_Cast<jstring>(JNI_CallObjectMethod(env, callback_method, method_get_name));
        JUTFString method_name(callback_name);
        auto callback_class = JNI_Cast<jclass>(
            JNI_CallObjectMethod(env, callback_method, method_get_declaring_class));
        auto callback_class_loader =
            JNI_CallObjectMethod(env, callback_class, class_get_class_loader);
        auto callback_class_name =
            JNI_Cast<jstring>(JNI_CallObjectMethod(env, callback_class, class_get_name));
        JUTFString class_name(callback_class_name);
        if (!JNI_IsInstanceOf(env, hooker_object, callback_class)) {
            LOGE("callback_method is not a method of hooker_object");
            return nullptr;
        }
        std::tie(built_class, hooker_field, hook_method, backup_method) =
            WrapScope(env, BuildDex(env, callback_class_loader,
                                    ArtMethod::GetMethodShorty(env, target_method), is_static,
                                    method_name.get(), class_name.get(), method_name.get()));
        if (!built_class || !hooker_field || !hook_method || !backup_method) {
            LOGE("Failed to generate hooker");
            return nullptr;
        }
    }

    auto reflected_hook = JNI_ToReflectedMethod(env, built_class, hook_method, is_static);
    auto reflected_backup = JNI_ToReflectedMethod(env, built_class, backup_method, is_static);

    JNI_CallVoidMethod(env, reflected_backup, set_accessible, JNI_TRUE);

    auto *hook = ArtMethod::FromReflectedMethod(env, reflected_hook);
    auto *backup = ArtMethod::FromReflectedMethod(env, reflected_backup);

    JNI_SetStaticObjectField(env, built_class, hooker_field, hooker_object);

    if (is_static && !Class::IsInitialized(env, target_class.get())) {
        auto *miror_class = Class::FromReflectedClass(env, target_class);
        if (!miror_class) {
            LOGE("Failed to decode target class");
            return nullptr;
        }
        const auto *class_def = miror_class->GetClassDef();
        if (!class_def) {
            LOGE("Failed to get target class def");
            return nullptr;
        }
        LOGD("Record pending hook for %p", target);
        RecordPending(class_def, target, hook, backup);
        return JNI_NewGlobalRef(env, reflected_backup);
    }
    if (DoHook(target, hook, backup)) {
        jobject global_backup = JNI_NewGlobalRef(env, reflected_backup);
        RecordHooked(target, global_backup);
        if (!is_proxy) [[likely]] {
            RecordJitMovement(target, backup);
        }
        return global_backup;
    }

    return nullptr;
}

[[maybe_unused]] bool UnHook(JNIEnv *env, jobject target_method) {
    if (!target_method || !JNI_IsInstanceOf(env, target_method, executable)) {
        LOGE("target method is not an executable");
        return false;
    }
    auto *target = ArtMethod::FromReflectedMethod(env, target_method);
    jobject reflected_backup = nullptr;
    {
        std::unique_lock lk(pending_methods_lock_);
        if (auto it = pending_methods_.find(target); it != pending_methods_.end()) {
            pending_methods_.erase(it);
            return true;
        }
    }
    {
        std::unique_lock lk(hooked_methods_lock_);
        if (auto it = hooked_methods_.find(target); it != hooked_methods_.end()) {
            reflected_backup = it->second;
            hooked_methods_.erase(it);
        }
    }
    if (reflected_backup == nullptr) {
        LOGE("Unable to unhook a method that is not hooked");
        return false;
    }
    auto *backup = ArtMethod::FromReflectedMethod(env, reflected_backup);
    env->DeleteGlobalRef(reflected_backup);
    return DoUnHook(target, backup);
}

[[maybe_unused]] bool IsHooked(JNIEnv *env, jobject method) {
    if (!method || !JNI_IsInstanceOf(env, method, executable)) {
        LOGE("method is not an executable");
        return false;
    }
    auto *art_method = ArtMethod::FromReflectedMethod(env, method);

    if (std::shared_lock lk(hooked_methods_lock_); hooked_methods_.contains(art_method)) {
        return true;
    }
    if (std::shared_lock lk(pending_methods_lock_); pending_methods_.contains(art_method)) {
        return true;
    }
    return false;
}

[[maybe_unused]] bool Deoptimize(JNIEnv *env, jobject method) {
    if (!method || !JNI_IsInstanceOf(env, method, executable)) {
        LOGE("method is not an executable");
        return false;
    }
    auto *art_method = ArtMethod::FromReflectedMethod(env, method);
    if (IsHooked(art_method)) {
        std::shared_lock lk(hooked_methods_lock_);
        auto it = hooked_methods_.find(art_method);
        if (it != hooked_methods_.end()) {
            auto *reflected_backup = it->second;
            art_method = ArtMethod::FromReflectedMethod(env, reflected_backup);
        }
    }
    if (!art_method) {
        return false;
    }
    return ClassLinker::SetEntryPointsToInterpreter(art_method);
}

[[maybe_unused]] void *GetNativeFunction(JNIEnv *env, jobject method) {
    if (!method || !JNI_IsInstanceOf(env, method, executable)) {
        LOGE("method is not an executable");
        return nullptr;
    }
    auto *art_method = ArtMethod::FromReflectedMethod(env, method);
    if (!art_method->IsNative()) {
        LOGE("method is not native");
        return nullptr;
    }
    return art_method->GetData();
}
[[maybe_unused]] bool MakeClassInheritable(JNIEnv *env, jclass target) {
    if (!target) {
        LOGE("target class is null");
        return false;
    }
    auto constructors =
        JNI_Cast<jobjectArray>(JNI_CallObjectMethod(env, target, class_get_declared_constructors));
    uint8_t access_flags = JNI_GetIntField(env, target, class_access_flags);
    constexpr static uint32_t kAccFinal = 0x0010;
    JNI_SetIntField(env, target, class_access_flags, static_cast<jint>(access_flags & ~kAccFinal));
    for (auto &constructor : constructors) {
        auto *method = ArtMethod::FromReflectedMethod(env, constructor.get());
        if (method && (!method->IsPublic() || !method->IsProtected())) method->SetProtected();
        if (method && method->IsFinal()) method->SetNonFinal();
    }
    return true;
}
}  // namespace v1

}  // namespace lsplant

#pragma clang diagnostic pop
