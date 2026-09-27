LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    bridge.cpp \
    codec.cpp \
    mtk.cpp

LOCAL_C_INCLUDES := hardware/ril/include

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    liblog \
    libutils

LOCAL_CFLAGS := -Wall -Werror -DRIL_SHLIB -DANDROID_MULTI_SIM -DANDROID_SIM_COUNT_$(SIM_COUNT)

LOCAL_MODULE := libmtkrilbridge
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := first
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
