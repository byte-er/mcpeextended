LOCAL_PATH := $(call my-dir)

# ---------------------------------------------------------------------------
# Source list is generated with wildcards so the Android build always stays in
# sync with the desktop CMake build (no more hand-maintained file lists).
# ---------------------------------------------------------------------------
SRC_ROOT := $(LOCAL_PATH)/../../../src

# Recursive wildcard helper
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Desktop/other-platform-only translation units
SRC_EXCLUDE := \
	$(SRC_ROOT)/AppPlatform_glfw.cpp \
	$(SRC_ROOT)/AppPlatform_win32.cpp \
	$(SRC_ROOT)/main_android.cpp \
	$(SRC_ROOT)/main_dedicated.cpp \
	$(SRC_ROOT)/main_rpi.cpp \
	$(SRC_ROOT)/platform/audio/SoundSystemAL.cpp

# ------------------------------- RakNet ------------------------------------
include $(CLEAR_VARS)
LOCAL_MODULE     := RakNet
RAKNET_SRCS      := $(wildcard $(SRC_ROOT)/raknet/*.cpp)
LOCAL_SRC_FILES  := $(RAKNET_SRCS:$(LOCAL_PATH)/%=%)
LOCAL_C_INCLUDES := $(SRC_ROOT)/raknet
LOCAL_CFLAGS     := -Wno-psabi -w -O2
LOCAL_CPPFLAGS   := -std=c++14
include $(BUILD_STATIC_LIBRARY)

# ----------------------------- minecraftpe ---------------------------------
include $(CLEAR_VARS)
LOCAL_MODULE := minecraftpe

MC_SRCS := $(filter-out $(SRC_EXCLUDE) $(SRC_ROOT)/raknet/%, $(call rwildcard,$(SRC_ROOT),*.cpp))
LOCAL_SRC_FILES := $(MC_SRCS:$(LOCAL_PATH)/%=%)

LOCAL_CFLAGS := -DPLATFORM_ANDROID -DPRE_ANDROID23 -DNDEBUG \
	-O2 -fno-strict-aliasing -ffunction-sections -fdata-sections \
	-Wno-narrowing -Wno-c++11-narrowing -Wno-invalid-source-encoding \
	-Wno-reserved-user-defined-literal -Wno-everything
LOCAL_CPPFLAGS := -std=c++14 -frtti
LOCAL_C_INCLUDES := $(SRC_ROOT)
LOCAL_LDFLAGS := -Wl,--gc-sections -Wl,-z,max-page-size=16384

LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv1_CM -lOpenSLES -lz
LOCAL_STATIC_LIBRARIES := RakNet

include $(BUILD_SHARED_LIBRARY)

