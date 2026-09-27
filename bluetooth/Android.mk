LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := android.hardware.bluetooth@1.0-service.mtk
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_INIT_RC := android.hardware.bluetooth@1.0-service.mtk.rc

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
    service.cc \
    async_fd_watcher.cc \
    bluetooth_hci.cc \
    h4_protocol.cc \
    hci_packetizer.cc \
    hci_protocol.cc \
    mct_protocol.cc \
    vendor_interface.cc \
    radiomod.c

LOCAL_SHARED_LIBRARIES := \
    android.hardware.bluetooth@1.0 \
    libbase \
    libcutils \
    libhardware \
    libdl \
    libhidlbase \
    libhidltransport \
    liblog \
    libutils \
    libnvram

include $(BUILD_EXECUTABLE)
