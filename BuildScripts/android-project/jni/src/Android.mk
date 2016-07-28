LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := main
LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/libFOnline.so
include $(PREBUILT_SHARED_LIBRARY)
