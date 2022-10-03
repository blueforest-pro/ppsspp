# These are definitions for LOCAL_ variables for PPSSPP.
# They are shared between ppsspp_jni (lib for Android app) and ppsspp_headless.

LOCAL_CFLAGS := -DUSE_FFMPEG -DWITH_UPNP -DUSING_GLES2 -DMOBILE_DEVICE -O3 -fsigned-char -Wall -Wno-multichar -Wno-unused-variable -fno-strict-aliasing -D__STDC_CONSTANT_MACROS -DSPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS
# yes, it's really CPPFLAGS for C++
# deprecated-register is generated by Android default code and causes noise.
LOCAL_CPPFLAGS := -fexceptions -std=gnu++17 -fno-rtti -Wno-reorder -Wno-deprecated-register -Wno-nullability-completeness
LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../Common \
  $(LOCAL_PATH)/../.. \
  $(LOCAL_PATH)/../../ext \
  $(LOCAL_PATH)/../../ext/snappy \
  $(LOCAL_PATH)/../../ext/glslang \
  $(LOCAL_PATH)/../../ext/miniupnp \
  $(LOCAL_PATH)/../../ext/miniupnp-build \
  $(LOCAL_PATH)/../../ext/libpng17 \
  $(LOCAL_PATH)/../../ext/libzip \
  $(LOCAL_PATH)/../../ext/zstd/lib \
  $(LOCAL_PATH)/../../ext/armips \
  $(LOCAL_PATH)/../../ext/armips/ext/filesystem/include \
  $(LOCAL_PATH)/../../ext/armips/ext/tinyformat \
  $(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES := libzip glslang-build miniupnp-build
LOCAL_LDLIBS := -lz -landroid -lGLESv2 -lOpenSLES -lEGL -ldl -llog -latomic
ifneq ($(NDK_DEBUG),1)
  # Prettier stack traces are nice on other platforms.
  # Maybe we can switch to storing the pre-stripped builds at some point.
  ifeq ($(TARGET_ARCH_ABI),x86_64)
    LOCAL_LDFLAGS += -Wl,--gc-sections -Wl,--exclude-libs,ALL
  endif
endif

# ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
ifeq ($(findstring armeabi-v7a,$(TARGET_ARCH_ABI)),armeabi-v7a)
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv7/lib/libavformat.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv7/lib/libavcodec.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv7/lib/libswresample.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv7/lib/libswscale.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv7/lib/libavutil.a
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../ffmpeg/android/armv7/include

  LOCAL_CFLAGS := $(LOCAL_CFLAGS) -D_ARCH_32
endif
ifeq ($(TARGET_ARCH_ABI),armeabi)
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv6/lib/libavformat.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv6/lib/libavcodec.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv6/lib/libswresample.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv6/lib/libswscale.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/armv6/lib/libavutil.a
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../ffmpeg/android/armv6/include

  LOCAL_CFLAGS := $(LOCAL_CFLAGS) -D_ARCH_32 -march=armv6
endif
ifeq ($(TARGET_ARCH_ABI),x86)
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86/lib/libavformat.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86/lib/libavcodec.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86/lib/libswresample.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86/lib/libswscale.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86/lib/libavutil.a
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../ffmpeg/android/x86/include

  LOCAL_CFLAGS := $(LOCAL_CFLAGS) -D_ARCH_32 -D_M_IX86 -fomit-frame-pointer -mtune=atom -mfpmath=sse -mssse3 -mstackrealign
endif

ifeq ($(TARGET_ARCH_ABI),x86_64)
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86_64/lib/libavformat.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86_64/lib/libavcodec.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86_64/lib/libswresample.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86_64/lib/libswscale.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/x86_64/lib/libavutil.a
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../ffmpeg/android/x86_64/include

  LOCAL_CFLAGS := $(LOCAL_CFLAGS) -D_M_X64 -fomit-frame-pointer -mtune=atom -mfpmath=sse -mssse3 -mstackrealign
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/arm64/lib/libavformat.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/arm64/lib/libavcodec.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/arm64/lib/libswresample.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/arm64/lib/libswscale.a
  LOCAL_LDLIBS += $(LOCAL_PATH)/../../ffmpeg/android/arm64/lib/libavutil.a
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../ffmpeg/android/arm64/include
endif
