//
// Created by 泓清 on 2022/8/26.
// 精简版：去掉了原项目里与外挂图标(icon/女孩.h 等)相关的依赖，
// 只保留 EGL + ImGui + native_surface 的封装。
//
#ifndef NATIVESURFACE_DRAW_H
#define NATIVESURFACE_DRAW_H

// System libs
#include <iostream>
#include <thread>
#include <chrono>
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <android/native_window.h>
#include <EGL/eglext.h>
#include <GLES3/gl3platform.h>
#include <GLES3/gl3ext.h>
#include <GLES3/gl32.h>

// Project libs
#include "native_surface/ANativeWindowCreator.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/timer.h"

using namespace std;
using namespace std::chrono_literals;

extern bool g_Initialized;

// Func
bool init_egl(uint32_t _screen_x, uint32_t _screen_y, bool log = false);
bool initDraw(uint32_t _screen_x, uint32_t _screen_y, bool log = false);
bool initDraw(bool log = false);
bool ImGui_init();
void screen_config();
void drawBegin();
void drawEnd();
void shutdown();

extern android::ANativeWindowCreator::DisplayInfo displayInfo;

#endif //NATIVESURFACE_DRAW_H
