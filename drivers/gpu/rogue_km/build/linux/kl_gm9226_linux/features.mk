########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

SYS_CFLAGS := \
 -fno-short-enums \
 -funwind-tables \
 -ffunction-sections \
 -fdata-sections \
 -D__linux__


 # These libraries are not coming from the NDK now, so we need to include them
 # from the ANDROID_ROOT source tree.
 # modified by semidrive begin
SYS_INCLUDES += \
  -isystem $(ANDROID_ROOT)/bionic/libc/include \
  -isystem $(ANDROID_ROOT)/bionic/libc/kernel/android/uapi \
  -isystem $(ANDROID_ROOT)/bionic/libc/kernel/uapi \
  -isystem $(ANDROID_ROOT)/bionic/libm/include \
  -isystem $(ANDROID_ROOT)/external/libdrm/include/drm \
  -isystem $(ANDROID_ROOT)/external/zlib/src \
  -isystem $(ANDROID_ROOT)/frameworks/native/include \
  -isystem $(JAVA_HOME)/include \
  -isystem $(JAVA_HOME)/include/linux
 # modified by semidrive end


SYS_CXXFLAGS += \
  -isystem $(LIBCXX_INCLUDE_PATH) -D_USING_LIBCXX
