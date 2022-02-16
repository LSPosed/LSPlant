#include "lsplant.hpp"

#include <sys/mman.h>
#include <android/api-level.h>
#include <atomic>
#include <array>
#include <sys/system_properties.h>
#include "utils/jni_helper.hpp"
#include "art/runtime/gc/scoped_gc_critical_section.hpp"
#include "art/runtime.hpp"
#include "art/thread.hpp"
#include "art/instrumentation.hpp"
#include "art/runtime/jit/jit_code_cache.hpp"
#include "art/runtime/art_method.hpp"
#include "art/thread_list.hpp"
#include "art/runtime/class_linker.hpp"
#include "external/dex_builder/dex_builder.h"
#include "common.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "UnreachableCode"
namespace lsplant {

using art::ArtMethod;
using art::thread_list::ScopedSuspendAll;
using art::ClassLinker;
using art::mirror::Class;
using art::Runtime;
using art::Thread;
using art::Instrumentation;
using art::gc::ScopedGCCriticalSection;
using art::jit::JitCodeCache;

namespace {
template<typename T, T... chars>
inline consteval auto operator ""_arr() {
    return std::array<T, sizeof...(chars)>{ chars... };
}

consteval inline auto GetTrampoline() {
    if constexpr(kArch == Arch::kArm) {
        return std::make_tuple(
                "\x00\x00\x9f\xe5\x20\xf0\x90\xe5\x78\x56\x34\x12"_arr,
                // NOLINTNEXTLINE
                uint8_t{ 4u }, uintptr_t{ 8u });
    }
    if constexpr(kArch == Arch::kArm64) {
        return std::make_tuple(
                "\x60\x00\x00\x58\x10\x00\x40\xf8\x00\x02\x1f\xd6\x78\x56\x34\x12\x78\x56\x34\x12"_arr,
                // NOLINTNEXTLINE
                uint16_t{ 5u }, uintptr_t{ 12u });
    }
    if constexpr(kArch == Arch::kX86) {
        return std::make_tuple(
                "\xb8\x78\x56\x34\x12\xff\x70\x20\xc3"_arr,
                // NOLINTNEXTLINE
                uint8_t{ 7u }, uintptr_t{ 1u });
    }
    if constexpr(kArch == Arch::kX8664) {
        return std::make_tuple(
                "\x48\xbf\x78\x56\x34\x12\x78\x56\x34\x12\xff\x77\x20\xc3"_arr,
                // NOLINTNEXTLINE
                uint8_t{ 12u }, uintptr_t{ 2u });
    }
}

auto[trampoline, entry_point_offset, art_method_offset] = GetTrampoline();

jmethodID method_get_name = nullptr;
jmethodID method_get_declaring_class = nullptr;
jmethodID class_get_canonical_name = nullptr;
jmethodID class_get_class_loader = nullptr;
jmethodID class_is_proxy = nullptr;
jclass in_memory_class_loader = nullptr;
jmethodID in_memory_class_loader_init = nullptr;
jmethodID load_class = nullptr;

bool InitJNI(JNIEnv *env) {
    auto executable = JNI_FindClass(env, "java/lang/reflect/Executable");
    if (!executable) {
        LOGE("Failed to found Executable");
        return false;
    }

    if (method_get_name = JNI_GetMethodID(env, executable, "getName",
                                          "()Ljava/lang/String;"); !method_get_name) {
        LOGE("Failed to find getName method");
        return false;
    }
    if (method_get_declaring_class = JNI_GetMethodID(env, executable, "getDeclaringClass",
                                                     "()Ljava/lang/Class;"); !method_get_declaring_class) {
        LOGE("Failed to find getName method");
        return false;
    }
    auto clazz = JNI_FindClass(env, "java/lang/Class");
    if (!clazz) {
        LOGE("Failed to find Class");
        return false;
    }

    if (class_get_class_loader = JNI_GetMethodID(env, clazz, "getClassLoader",
                                                 "()Ljava/lang/ClassLoader;");
            !class_get_class_loader) {
        LOGE("Failed to find getClassLoader");
        return false;
    }

    if (class_get_canonical_name = JNI_GetMethodID(env, clazz, "getCanonicalName",
                                                   "()Ljava/lang/String;");
            !class_get_canonical_name) {
        LOGE("Failed to find getCanonicalName");
        return false;
    }

    if (class_is_proxy = JNI_GetMethodID(env, clazz, "isProxy", "()Z"); !class_is_proxy) {
        LOGE("Failed to find isProxy");
        return false;
    }

    in_memory_class_loader = JNI_NewGlobalRef(env, JNI_FindClass(env,
                                                                 "dalvik/system/InMemoryDexClassLoader"));
    in_memory_class_loader_init = JNI_GetMethodID(env, in_memory_class_loader, "<init>",
                                                  "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");

    load_class = JNI_GetMethodID(env, in_memory_class_loader, "loadClass",
                                 "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!load_class) {
        load_class = JNI_GetMethodID(env, in_memory_class_loader, "findClass",
                                     "(Ljava/lang/String;)Ljava/lang/Class;");
    }
    return true;
}

inline void UpdateTrampoline(decltype(entry_point_offset) offset) {
    *reinterpret_cast<decltype(entry_point_offset) *>(trampoline.data() +
                                                      entry_point_offset) = offset;
}

bool InitNative(JNIEnv *env, const HookHandler &handler) {
    if (!handler.inline_hooker || !handler.inline_unhooker || !handler.art_symbol_resolver)
        return false;
    if (!Runtime::Init(handler)) {
        LOGE("Failed to init runtime");
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
        LOGE("failed to init intrumentation");
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

std::tuple<jclass, jfieldID, jmethodID, jmethodID>
BuildDex(JNIEnv *env, jobject class_loader,
         std::string_view shorty, bool is_static, std::string_view method_name,
         std::string_view hooker_class, std::string_view callback_name) {
    // NOLINTNEXTLINE
    using namespace startop::dex;

    if (shorty.empty()) {
        LOGE("Invalid shorty");
        return { nullptr, nullptr, nullptr, nullptr };
    }

    DexBuilder dex_file;

    auto parameter_types = std::vector<TypeDescriptor>();
    parameter_types.reserve(shorty.size() - 1);
    std::string storage;
    auto return_type =
            shorty[0] == 'L' ? TypeDescriptor::Object : TypeDescriptor::FromDescriptor(
                    shorty[0]);
    if (!is_static) parameter_types.push_back(TypeDescriptor::Object); // this object
    for (const char &param: shorty.substr(1)) {
        parameter_types.push_back(
                param == 'L' ? TypeDescriptor::Object : TypeDescriptor::FromDescriptor(
                        static_cast<char>(param)));
    }

    // TODO(yujincheng08): customize it
    ClassBuilder cbuilder{ dex_file.MakeClass("LspHooker_") };
    // TODO(yujincheng08): customize it
    cbuilder.set_source_file("LSP");

    auto hooker_type = TypeDescriptor::FromClassname(hooker_class.data());

    // TODO(yujincheng08): customize it
    auto *hooker_field = cbuilder.CreateField("hooker", hooker_type)
            .access_flags(dex::kAccStatic)
            .Encode();

    auto hook_builder{ cbuilder.CreateMethod(
            method_name.data(), Prototype{ return_type, parameter_types }) };
    // allocate tmp first because of wide
    auto tmp{ hook_builder.AllocRegister() };
    hook_builder.BuildConst(tmp, static_cast<int>(parameter_types.size()));
    auto hook_params_array{ hook_builder.AllocRegister() };
    hook_builder.BuildNewArray(hook_params_array, TypeDescriptor::Object, tmp);
    for (size_t i = 0U, j = 0U; i < parameter_types.size(); ++i, ++j) {
        hook_builder.BuildBoxIfPrimitive(Value::Parameter(j), parameter_types[i],
                                         Value::Parameter(j));
        hook_builder.BuildConst(tmp, static_cast<int>(i));
        hook_builder.BuildAput(Instruction::Op::kAputObject, hook_params_array,
                               Value::Parameter(j), tmp);
        if (parameter_types[i].is_wide()) ++j;
    }
    auto handle_hook_method{ dex_file.GetOrDeclareMethod(
            hooker_type, callback_name.data(),
            Prototype{ TypeDescriptor::Object, TypeDescriptor::Object.ToArray() }) };
    hook_builder.AddInstruction(
            Instruction::GetStaticObjectField(hooker_field->decl->orig_index, tmp));
    hook_builder.AddInstruction(Instruction::InvokeVirtualObject(
            handle_hook_method.id, tmp, tmp, hook_params_array));
    if (return_type == TypeDescriptor::Void) {
        hook_builder.BuildReturn();
    } else if (return_type.is_primitive()) {
        auto box_type{ return_type.ToBoxType() };
        const ir::Type *type_def = dex_file.GetOrAddType(box_type);
        hook_builder.AddInstruction(
                Instruction::Cast(tmp, Value::Type(type_def->orig_index)));
        hook_builder.BuildUnBoxIfPrimitive(tmp, box_type, tmp);
        hook_builder.BuildReturn(tmp, false, return_type.is_wide());
    } else {
        const ir::Type *type_def = dex_file.GetOrAddType(return_type);
        hook_builder.AddInstruction(
                Instruction::Cast(tmp, Value::Type(type_def->orig_index)));
        hook_builder.BuildReturn(tmp, true);
    }
    auto *hook_method = hook_builder.Encode();

    auto backup_builder{
            cbuilder.CreateMethod("backup", Prototype{ return_type, parameter_types }) };
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

    slicer::MemView image{ dex_file.CreateImage() };

    auto *dex_buffer = env->NewDirectByteBuffer(const_cast<void *>(image.ptr()), image.size());
    auto my_cl = JNI_NewObject(env, in_memory_class_loader, in_memory_class_loader_init,
                               dex_buffer, class_loader);
    env->DeleteLocalRef(dex_buffer);

    if (my_cl) {
        auto target_class = JNI_Cast<jclass>(JNI_CallObjectMethod(env, my_cl, load_class,
                                                                  JNI_NewStringUTF(env,
                                                                                   "LspHooker_")));
        return {
                target_class.release(),
                JNI_GetStaticFieldID(env, target_class, hooker_field->decl->name->c_str(),
                                     hooker_field->decl->type->Decl().data()),
                JNI_GetStaticMethodID(env, target_class, hook_method->decl->name->c_str(),
                                      hook_method->decl->prototype->Signature().data()),
                JNI_GetStaticMethodID(env, target_class, backup_method->decl->name->c_str(),
                                      backup_method->decl->prototype->Signature().data()),
        };
    }
    return { nullptr, nullptr, nullptr, nullptr };
}

static_assert(std::endian::native == std::endian::little, "Unsupported architecture");

union Trampoline {
public:
    uintptr_t address;
    unsigned count: 12;
};

static_assert(sizeof(Trampoline) == sizeof(uintptr_t), "Unsupported architecture");
static_assert(std::atomic_uintptr_t::is_always_lock_free, "Unsupported architecture");

std::atomic_uintptr_t trampoline_pool{ 0 };
std::atomic_flag trampoline_lock{ false };
constexpr size_t kTrampolineSize = RoundUpTo(sizeof(trampoline), kPointerSize);
constexpr size_t kPageSize = 4096; // assume
constexpr size_t kTrampolineNumPerPage = kPageSize / kTrampolineSize;
constexpr uintptr_t kAddressMask = 0xFFFU;

void *GenerateTrampolineFor(art::ArtMethod *hook) {
    unsigned count;
    uintptr_t address;
    while (true) {
        auto tl = Trampoline{ .address = trampoline_pool.fetch_add(1, std::memory_order_release) };
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
    ScopedGCCriticalSection section(art::Thread::Current(),
                                    art::gc::kGcCauseDebugger,
                                    art::gc::kCollectorTypeDebugger);
    ScopedSuspendAll suspend("Yahfa Hook", false);
    LOGD("target = %p, hook = %p, backup = %p", target, hook, backup);

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

        backup->SetPrivate();

        LOGD("done hook");
        LOGV("target(%p:0x%x) -> %p; backup(%p:0x%x) -> %p; hook(%p:0x%x) -> %p",
             target, target->GetAccessFlags(), target->GetEntryPoint(),
             backup, backup->GetAccessFlags(), backup->GetEntryPoint(),
             hook, hook->GetAccessFlags(), hook->GetEntryPoint());

        return true;
    }
}

bool DoUnHook(ArtMethod *target, ArtMethod *backup) {
    ScopedGCCriticalSection section(art::Thread::Current(),
                                    art::gc::kGcCauseDebugger,
                                    art::gc::kCollectorTypeDebugger);
    ScopedSuspendAll suspend("Yahfa Hook", false);
    auto access_flags = target->GetAccessFlags();
    target->CopyFrom(backup);
    target->SetAccessFlags(access_flags);
    return true;
}

}  // namespace

void OnPending(art::ArtMethod *target, art::ArtMethod *hook, art::ArtMethod *backup) {
    LOGD("On pending hook");
    if (!DoHook(target, hook, backup)) {
        LOGE("Pending hook failed");
    }
}

[[maybe_unused]]
bool Init(JNIEnv *env, const InitInfo &info) {
    bool static kInit = InitJNI(env) && InitNative(env, info);
    return kInit;
}


// TODO: sync? does not record hook immediately
[[maybe_unused]]
jmethodID
Hook(JNIEnv *env, jmethodID target_method, jobject hooker_object, jmethodID callback_method) {
    auto reflected_target = JNI_ToReflectedMethod(env, jclass{ nullptr }, target_method, false);
    auto reflected_callback = JNI_ToReflectedMethod(env, jclass{ nullptr }, callback_method, false);
    jmethodID hook_method = nullptr;
    jmethodID backup_method = nullptr;
    jfieldID hooker_field = nullptr;

    auto target_class = JNI_Cast<jclass>(
            JNI_CallObjectMethod(env, reflected_target, method_get_declaring_class));
    bool is_proxy = JNI_CallBooleanMethod(env, target_class, class_is_proxy);
    auto *target = ArtMethod::FromReflectedMethod(env, reflected_target);
    bool is_static = target->IsStatic();

    if (IsHooked(target) || IsPending(target)) {
        LOGW("Skip duplicate hook");
        return nullptr;
    }

    ScopedLocalRef<jclass> built_class{ env };
    {
        auto callback_name = JNI_Cast<jstring>(
                JNI_CallObjectMethod(env, reflected_callback, method_get_name));
        JUTFString method_name(callback_name);
        auto callback_class = JNI_Cast<jclass>(JNI_CallObjectMethod(env, reflected_callback,
                                                                    method_get_declaring_class));
        auto callback_class_loader = JNI_CallObjectMethod(env, callback_class,
                                                          class_get_class_loader);
        auto callback_class_name = JNI_Cast<jstring>(JNI_CallObjectMethod(env, callback_class,
                                                                          class_get_canonical_name));
        JUTFString class_name(callback_class_name);
        if (env->IsInstanceOf(hooker_object, callback_class)) {
            LOGE("callback_method is not a method of hooker_object");
            return nullptr;
        }
        std::tie(built_class, hooker_field, hook_method, backup_method) =
                WrapScope(env, BuildDex(env, callback_class_loader,
                                        ArtMethod::GetMethodShorty(env, target_method),
                                        is_static,
                                        method_name.get(),
                                        class_name.get(),
                                        method_name.get()));
        if (!built_class || !hooker_field || !hook_method || !backup_method) {
            LOGE("Failed to generate hooker");
            return nullptr;
        }
    }

    auto reflected_hook = JNI_ToReflectedMethod(env, jclass{ nullptr }, hook_method, false);
    auto reflected_backup = JNI_ToReflectedMethod(env, jclass{ nullptr }, backup_method, false);

    auto *hook = ArtMethod::FromReflectedMethod(env, reflected_hook);
    auto *backup = ArtMethod::FromReflectedMethod(env, reflected_backup);

    env->SetStaticObjectField(built_class.get(), hooker_field, hooker_object);

    if (is_static && !Class::IsInitialized(env, target_class.get())) {
        auto *miror_class = Class::FromReflectedClass(env, target_class);
        if (!miror_class) {
            LOGE("Failed to decode target class");
            return nullptr;
        }
        auto class_def = miror_class->GetClassDef();
        if (!class_def) {
            LOGE("Failed to get target class def");
            return nullptr;
        }
        RecordPending(class_def, target, hook, backup);
        return backup_method;
    } else if (DoHook(target, hook, backup)) {
        RecordHooked(target, JNI_NewGlobalRef(env, reflected_backup));
        if (!is_proxy) [[likely]] RecordJitMovement(target, backup);
        return backup_method;
    }

    return nullptr;
}

[[maybe_unused]]
bool UnHook(JNIEnv *env, jmethodID target_method) {
    auto reflected_target = JNI_ToReflectedMethod(env, jclass{ nullptr }, target_method, false);
    auto *target = ArtMethod::FromReflectedMethod(env, reflected_target);
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
        if (auto it = hooked_methods_.find(target);it != hooked_methods_.end()) {
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

[[maybe_unused]]
bool IsHooked(JNIEnv *env, jmethodID method) {
    auto reflected = JNI_ToReflectedMethod(env, jclass{ nullptr }, method, false);
    auto *art_method = ArtMethod::FromReflectedMethod(env, reflected);

    if (std::shared_lock lk(hooked_methods_lock_); hooked_methods_.contains(art_method)) {
        return true;
    }
    if (std::shared_lock lk(pending_methods_lock_); pending_methods_.contains(art_method)) {
        return true;
    }
    return false;
}

[[maybe_unused]]
bool Deoptimize(JNIEnv *env, jmethodID method) {
    auto reflected = JNI_ToReflectedMethod(env, jclass{ nullptr }, method, false);
    auto *art_method = ArtMethod::FromReflectedMethod(env, reflected);
    return ClassLinker::SetEntryPointsToInterpreter(art_method);
}

[[maybe_unused]]
void *GetNativeFunction(JNIEnv *env, jmethodID method) {
    auto reflected = JNI_ToReflectedMethod(env, jclass{ nullptr }, method, false);
    auto *art_method = ArtMethod::FromReflectedMethod(env, reflected);
    return art_method->GetData();
}

}  // namespace lsplant

#pragma clang diagnostic pop
