LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libev
LOCAL_SRC_FILES := \
	ev.c \
	event.c

include $(BUILD_STATIC_LIBRARY)