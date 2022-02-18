LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE           := lsplant
LOCAL_C_INCLUDES       := $(LOCAL_PATH)/include
LOCAL_SRC_FILES        := lsplant.cc
LOCAL_EXPORT_C_INCLUDES:= $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := dex_builder
LOCAL_EXPORT_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)

include jni/external/dex_builder/Android.mk

