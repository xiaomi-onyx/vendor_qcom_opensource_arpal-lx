LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libpalclient
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS += -v -Wall -Wthread-safety
LOCAL_TIDY := true

LOCAL_C_INCLUDES := $(PAL_BASE_PATH)/inc

LOCAL_SRC_FILES := \
    PalClientWrapper.cpp \
    PalCallback.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libutils \
    vendor.qti.hardware.pal-V1-ndk \
    libfmq \
    libbinder_ndk

LOCAL_STATIC_LIBRARIES := \
    libaidlcommonsupport \
    libpalaidltypeconverter

LOCAL_EXPORT_HEADER_LIBRARY_HEADERS := libarpal_headers
LOCAL_HEADER_LIBRARIES := libarpal_headers

include $(BUILD_SHARED_LIBRARY)
