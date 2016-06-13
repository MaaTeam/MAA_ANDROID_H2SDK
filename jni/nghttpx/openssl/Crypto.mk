local_c_flags :=

local_c_includes := $(log_c_includes)

local_additional_dependencies := $(LOCAL_PATH)/android-config.mk $(LOCAL_PATH)/Crypto.mk

include $(LOCAL_PATH)/Crypto-config.mk

#######################################
# target shared library
include $(CLEAR_VARS)
include $(LOCAL_PATH)/android-config.mk

LOCAL_SHARED_LIBRARIES := $(log_shared_libraries)

ifneq (,$(TARGET_BUILD_APPS))
LOCAL_SDK_VERSION := 9
endif
LOCAL_LDFLAGS += -ldl
#LOCAL_SHORT_COMMANDS := true
LOCAL_SRC_FILES += $(target_src_files)
LOCAL_CFLAGS += $(target_c_flags)
LOCAL_C_INCLUDES += $(target_c_includes)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libcrypto
LOCAL_ADDITIONAL_DEPENDENCIES := $(local_additional_dependencies)
include $(BUILD_STATIC_LIBRARY)
