module;

#include <android/api-level.h>

export module lsplant:scope_gc_critical_section;

import :thread;
import :common;
import hook_helper;

namespace lsplant::art::gc {
// Which types of collections are able to be performed.
enum CollectorType {
    // No collector selected.
    kCollectorTypeNone,
    // Non concurrent mark-sweep.
    kCollectorTypeMS,
    // Concurrent mark-sweep.
    kCollectorTypeCMS,
    // Semi-space / mark-sweep hybrid, enables compaction.
    kCollectorTypeSS,
    // Heap trimming collector, doesn't do any actual collecting.
    kCollectorTypeHeapTrim,
    // A (mostly) concurrent copying collector.
    kCollectorTypeCC,
    // The background compaction of the concurrent copying collector.
    kCollectorTypeCCBackground,
    // Instrumentation critical section fake collector.
    kCollectorTypeInstrumentation,
    // Fake collector for adding or removing application image spaces.
    kCollectorTypeAddRemoveAppImageSpace,
    // Fake collector used to implement exclusion between GC and debugger.
    kCollectorTypeDebugger,
    // A homogeneous space compaction collector used in background transition
    // when both foreground and background collector are CMS.
    kCollectorTypeHomogeneousSpaceCompact,
    // Class linker fake collector.
    kCollectorTypeClassLinker,
    // JIT Code cache fake collector.
    kCollectorTypeJitCodeCache,
    // Hprof fake collector.
    kCollectorTypeHprof,
    // Fake collector for installing/removing a system-weak holder.
    kCollectorTypeAddRemoveSystemWeakHolder,
    // Fake collector type for GetObjectsAllocated
    kCollectorTypeGetObjectsAllocated,
    // Fake collector type for ScopedGCCriticalSection
    kCollectorTypeCriticalSection,
};

// What caused the GC?
enum GcCause {
    // Invalid GC cause used as a placeholder.
    kGcCauseNone,
    // GC triggered by a failed allocation. Thread doing allocation is blocked waiting for GC before
    // retrying allocation.
    kGcCauseForAlloc,
    // A background GC trying to ensure there is free memory ahead of allocations.
    kGcCauseBackground,
    // An explicit System.gc() call.
    kGcCauseExplicit,
    // GC triggered for a native allocation when NativeAllocationGcWatermark is exceeded.
    // (This may be a blocking GC depending on whether we run a non-concurrent collector).
    kGcCauseForNativeAlloc,
    // GC triggered for a collector transition.
    kGcCauseCollectorTransition,
    // Not a real GC cause, used when we disable moving GC (currently for
    // GetPrimitiveArrayCritical).
    kGcCauseDisableMovingGc,
    // Not a real GC cause, used when we trim the heap.
    kGcCauseTrim,
    // Not a real GC cause, used to implement exclusion between GC and instrumentation.
    kGcCauseInstrumentation,
    // Not a real GC cause, used to add or remove app image spaces.
    kGcCauseAddRemoveAppImageSpace,
    // Not a real GC cause, used to implement exclusion between GC and debugger.
    kGcCauseDebugger,
    // GC triggered for background transition when both foreground and background collector are CMS.
    kGcCauseHomogeneousSpaceCompact,
    // Class linker cause, used to guard filling art methods with special values.
    kGcCauseClassLinker,
    // Not a real GC cause, used to implement exclusion between code cache metadata and GC.
    kGcCauseJitCodeCache,
    // Not a real GC cause, used to add or remove system-weak holders.
    kGcCauseAddRemoveSystemWeakHolder,
    // Not a real GC cause, used to prevent hprof running in the middle of GC.
    kGcCauseHprof,
    // Not a real GC cause, used to prevent GetObjectsAllocated running in the middle of GC.
    kGcCauseGetObjectsAllocated,
    // GC cause for the profile saver.
    kGcCauseProfileSaver,
};

export class GCCriticalSection {
private:
    [[maybe_unused]] void *self_;
    [[maybe_unused]] const char *section_name_;
};

export class ScopedGCCriticalSection {
    inline static auto constructor_ =
            "_ZN3art2gc23ScopedGCCriticalSectionC2EPNS_6ThreadENS0_7GcCauseENS0_13CollectorTypeE"_sym.as<void(ScopedGCCriticalSection::*)(Thread *, GcCause, CollectorType)>;
    inline static auto destructor_ =
            "_ZN3art2gc23ScopedGCCriticalSectionD2Ev"_sym.as<void(ScopedGCCriticalSection::*)()>;

public:
    ScopedGCCriticalSection(Thread *self, GcCause cause, CollectorType collector_type) {
        if (constructor_) {
            constructor_(this, self, cause, collector_type);
        }
    }

    ~ScopedGCCriticalSection() {
        if (destructor_) destructor_(this);
    }

    static bool Init(const HookHandler &handler) {
        // for Android M, it's safe to not found since we have suspendVM & resumeVM
        auto sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_N__) [[likely]] {
            if (!handler(constructor_) || !handler(destructor_)) {
                return false;
            }
        }
        return true;
    }

private:
    [[maybe_unused]] GCCriticalSection critical_section_;
    [[maybe_unused]] const char *old_no_suspend_reason_;
};
}  // namespace lsplant::art::gc
