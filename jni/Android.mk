LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := cname
LOCAL_SRC_FILES := libcname.cc
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
