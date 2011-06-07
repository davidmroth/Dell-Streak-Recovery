# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.



LOCAL_PATH := $(call my-dir)

#----------------------------------------------------------------------
# Modified adbd binary for recovery
#----------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE := adbd_recovery
LOCAL_MODULE_STEM := adbd
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)



#----------------------------------------------------------------------
# Nandroid MD5 script
#----------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_SRC_FILES := nandroid-md5.sh
LOCAL_MODULE := nandroid-md5.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
include $(BUILD_PREBUILT)



#----------------------------------------------------------------------
# Recovery default.prop
#----------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE := recovery.prop
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)
include $(BUILD_PREBUILT)



#----------------------------------------------------------------------
# Fix_permission Script
#----------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE := fix_permissions
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)
include $(BUILD_PREBUILT)



#----------------------------------------------------------------------
# Modified init.rc used for recovery
#----------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE := init.rc-recovery
LOCAL_MODULE_STEM := init.rc
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)
include $(BUILD_PREBUILT)



#----------------------------------------------------------------------
# Modified sh binary for recovery (statically built)
#----------------------------------------------------------------------

#include $(CLEAR_VARS)
#LOCAL_MODULE := sh_recovery
#LOCAL_MODULE_STEM := sh
#LOCAL_MODULE_CLASS := EXECUTABLES
#LOCAL_MODULE_PATH := $(PRODUCT_OUT)
#LOCAL_SRC_FILES := $(LOCAL_MODULE)
#include $(BUILD_PREBUILT)
