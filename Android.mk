ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)

commands_recovery_local_path := $(LOCAL_PATH)

LOCAL_SRC_FILES := \
    extendedcommands.c \
    commands.c \
    nandroid.c \
    recovery.c \
    install.c \
    roots.c \
    ui.c \
    bootloader.c \
    firmware.c \
    reboot.c \
    setprop.c \
    getprop.c \
    verifier.c 

LOCAL_SRC_FILES += test_roots.c

LOCAL_C_INCLUDES += bootable/recovery/mtdutils
LOCAL_C_INCLUDES += bootable/recovery/minui
LOCAL_C_INCLUDES += bootable/recovery/minzip

LOCAL_MODULE := recovery-$(TARGET_PRODUCT)
LOCAL_MODULE_STEM := recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true

RECOVERY_API_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

RECOVERY_VERSION := v0.3.2
LOCAL_CFLAGS += -DRECOVERY_VERSION="$(RECOVERY_VERSION)"


# This binary is in the recovery ramdisk, which is otherwise a copy of root.
# It gets copied there in config/Makefile.  LOCAL_MODULE_TAGS suppresses
# a (redundant) copy of the binary in /system/bin for user builds.
# TODO: Build the ramdisk image in a more principled way.

LOCAL_MODULE_TAGS := eng

LOCAL_SRC_FILES += default_recovery_ui.c

#External Shared Libraries
LOCAL_STATIC_LIBRARIES += libbusybox libclearsilverregex

#Internal Shared Libararies
LOCAL_STATIC_LIBRARIES += libamend libmkyaffs2image libunyaffs liberase_image libdump_image libflash_image

#Stock
LOCAL_STATIC_LIBRARIES += libminzip libunz libmtdutils libmincrypt
LOCAL_STATIC_LIBRARIES += libminui libpixelflinger_static libpng libcutils
LOCAL_STATIC_LIBRARIES += libstdc++ libc

LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_UNSTRIPPED_PATH := $(PRODUCT_OUT)/symbols/recovery

#Pickup by build/core/Makefile
RECOVERY_LINKS := busybox flash_image dump_image mkyaffs2image unyaffs erase_image nandroid reboot setprop getprop

include $(BUILD_EXECUTABLE)


#New
include $(commands_recovery_local_path)/mtdutils/Android.mk
include $(commands_recovery_local_path)/amend/Android.mk
include $(commands_recovery_local_path)/prebuilt/Android.mk

commands_recovery_local_path :=

endif   # TARGET_ARCH == arm
endif    # !TARGET_SIMULATOR

