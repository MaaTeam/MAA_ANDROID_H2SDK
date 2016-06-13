LOCAL_PATH := $(call my-dir)
NGHTTPX_PATH := $(LOCAL_PATH)
include $(CLEAR_VARS)
include $(NGHTTPX_PATH)/libev-4.19/Android.mk
include $(NGHTTPX_PATH)/openssl/Android.mk
include $(NGHTTPX_PATH)/nghttp2-1.6.0/Android.mk