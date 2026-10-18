LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# 输出可执行文件名：libs/arm64-v8a/fps_monitor.sh
LOCAL_MODULE := fps_monitor.sh

# 关闭警告、开 C++17 / 异常 / permissive（ImGui + 老代码需要）
LOCAL_CPPFLAGS += -w -std=c++17 -fexceptions -fpermissive
LOCAL_CFLAGS   += -w -std=c++17 -fexceptions -fpermissive

# 头文件搜索路径
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/native_surface/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_draw/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui/backends/

LOCAL_SRC_FILES := \
    src/main.cpp \
    src/Android_draw/draw.cpp \
    src/ImGui/imgui.cpp \
    src/ImGui/imgui_draw.cpp \
    src/ImGui/imgui_tables.cpp \
    src/ImGui/imgui_widgets.cpp \
    src/ImGui/backends/imgui_impl_android.cpp \
    src/ImGui/backends/imgui_impl_opengl3.cpp

LOCAL_LDLIBS := -lm -ldl -lz -llog -landroid -lEGL -lGLESv2 -lGLESv3

include $(BUILD_EXECUTABLE)
