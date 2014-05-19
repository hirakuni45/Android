# Copyright (C) 2010 The Android Open Source Project
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
#
LOCAL_PATH := $(call my-dir)

common_SRC_FILES := \
	libpng/png.c \
	libpng/pngerror.c \
	libpng/pngget.c \
	libpng/pngmem.c \
	libpng/pngpread.c \
	libpng/pngread.c \
	libpng/pngrio.c \
	libpng/pngrtran.c \
	libpng/pngrutil.c \
	libpng/pngset.c \
	libpng/pngtrans.c \
	libpng/pngwio.c \
	libpng/pngwrite.c \
	libpng/pngwtran.c \
	libpng/pngwutil.c \

# Previously these arm-specific flags were never applied.
# TODO: apply the flags and fix the build.
# my_cflags_arm := -DPNG_ARM_NEON_OPT=2 -DPNG_ARM_NEON_CHECK_SUPPORTED
my_cflags_arm :=

# BUG: http://llvm.org/PR19472 - SLP vectorization (on ARM at least) crashes
# when we can't lower a vectorized bswap.
my_cflags_arm += -fno-slp-vectorize

my_src_files_arm := \
    libpng/arm/arm_init.c \
    libpng/arm/filter_neon.S


common_CFLAGS := -std=gnu89 #-fvisibility=hidden ## -fomit-frame-pointer

ifeq ($(HOST_OS),windows)
	ifeq ($(USE_MINGW),)
#		Case where we're building windows but not under linux (so it must be cygwin)
#		In this case, gcc cygwin doesn't recognize -fvisibility=hidden
		$(info libpng: Ignoring gcc flag $(common_CFLAGS) on Cygwin)
	common_CFLAGS :=
	endif
endif

ifeq ($(HOST_OS),darwin)
common_CFLAGS += -no-integrated-as
common_ASFLAGS += -no-integrated-as
endif

common_C_INCLUDES +=

common_COPY_HEADERS_TO := libpng
common_COPY_HEADERS := png.h pngconf.h pngusr.h

include $(CLEAR_VARS)
#LOCAL_CLANG := true
LOCAL_SRC_FILES := $(common_SRC_FILES)
LOCAL_CFLAGS += $(common_CFLAGS) -ftrapv
LOCAL_CFLAGS_arm := $(my_cflags_arm)
LOCAL_ASFLAGS += $(common_ASFLAGS)
LOCAL_SRC_FILES_arm := $(my_src_files_arm)

LOCAL_C_INCLUDES += $(common_C_INCLUDES) \
	external/zlib
LOCAL_SHARED_LIBRARIES := \
	libz

LOCAL_MODULE:= libpng
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE    := simple-test
LOCAL_CFLAGS    := -Werror -fexceptions
LOCAL_C_INCLUDES:= c:/MinGW/msys/1.0/local/boost_1_54_0 libpng
LOCAL_SRC_FILES :=	main.cpp \
					minizip/ioapi.c \
					minizip/unzip.c \
					utils/file_info.cpp \
					utils/file_io.cpp \
					utils/string_utils.cpp \
					utils/unzip.cpp \
					img_io/png_io.cpp \
					img_io/img_utils.cpp \
					gl_fw/glmobj.cpp \
					gl_fw/gltexfb.cpp \
					side/arcade.cpp \
					side/i8080.cpp \
					side/i8080opc.cpp \
					side/i8080sub.cpp
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv1_CM -lz
LOCAL_STATIC_LIBRARIES := android_native_app_glue libpng

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)

