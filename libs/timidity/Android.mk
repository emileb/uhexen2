LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)

LOCAL_MODULE := timidity

LOCAL_C_INCLUDES := $(LOCAL_PATH) \

LOCAL_SRC_FILES := common.c \
                   	instrum.c \
                   	mix.c \
                   	output.c \
                   	playmidi.c \
                   	readmidi.c \
                   	resample.c \
                   	stream.c \
                   	tables.c \
                   	timidity.c

LOCAL_CFLAGS           := -Wall  -DTIMIDITY_BUILD=1 -DTIMIDITY_STATIC=1

include $(BUILD_STATIC_LIBRARY)


