//
// Created by 泓清 on 2022/8/26.
//

#include "Android_draw/draw.h"
// Var
EGLDisplay display = EGL_NO_DISPLAY;
EGLConfig config;
EGLSurface surface = EGL_NO_SURFACE;
ANativeWindow *native_window;
ANativeWindow *(*createNativeWindow) (const char *surface_name, uint32_t screen_width, uint32_t screen_height, uint32_t format,uint32_t flags, bool author);
android::ANativeWindowCreator::DisplayInfo displayInfo{0};
//ExternFunction externFunction;
EGLContext context = EGL_NO_CONTEXT;
//MDisplayInfo displayInfo;
uint32_t orientation = 0;
bool g_Initialized = false;
bool initDraw(bool log) {
    screen_config();
    orientation = displayInfo.orientation;
    return initDraw(displayInfo.width+displayInfo.height, displayInfo.height+displayInfo.width, log);
}

bool initDraw(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    if (!init_egl(_screen_x, _screen_y, log)) {
        return false;
    }
    return ImGui_init();
}
// bool init_egl1(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    // native_window = externFunction.createNativeWindow("Ssage",_screen_x, _screen_y, true);
    // // native_window = android::ANativeWindowCreator::Create("ScreenRecorder", _screen_x, _screen_y, 1, 0x40);
    // ANativeWindow_acquire(native_window);
    // display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    // if (display == EGL_NO_DISPLAY) {
        // printf("eglGetDisplay error=%u\n", glGetError());
        // return false;
    // }
    // if (log) {
        // printf("eglGetDisplay ok\n");
    // }
    // if (eglInitialize(display, 0, 0) != EGL_TRUE) {
        // printf("eglInitialize error=%u\n", glGetError());
        // return false;
    // }
    // if (log) {
        // printf("eglInitialize ok\n");
    // }
    // EGLint num_config = 0;
    // const EGLint attribList[] = {
            // EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            // EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            // EGL_BLUE_SIZE, 5,   //-->delete
            // EGL_GREEN_SIZE, 6,  //-->delete
            // EGL_RED_SIZE, 5,    //-->delete
            // EGL_BUFFER_SIZE, 32,  //-->new field
            // EGL_DEPTH_SIZE, 16,
            // EGL_STENCIL_SIZE, 8,
            // EGL_NONE
    // };
    // const EGLint attrib_list[] = {
            // EGL_CONTEXT_CLIENT_VERSION,
            // 3,
            // EGL_NONE
    // };

    // if (log) {
        // printf("num_config=%d\n", num_config);
    // }
    // if (eglChooseConfig(display, attribList, &config, 1, &num_config) != EGL_TRUE) {
        // printf("eglChooseConfig  error=%u\n", glGetError());
        // return false;
    // }
    // if (log) {
        // printf("eglChooseConfig ok\n");
    // }
    // EGLint egl_format;
    // eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format);
    // ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);
    // context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
    // if (context == EGL_NO_CONTEXT) {
        // printf("eglCreateContext  error = %u\n", glGetError());
        // return false;
    // }
    // if (log) {
        // printf("eglCreateContext ok\n");
    // }
    // surface = eglCreateWindowSurface(display, config, native_window, nullptr);
    // if (surface == EGL_NO_SURFACE) {
        // printf("eglCreateWindowSurface  error = %u\n", glGetError());
        // return false;
    // }
    // if (log) {
        // printf("eglCreateWindowSurface ok\n");
    // }
    // if (!eglMakeCurrent(display, surface, surface, context)) {
        // printf("eglMakeCurrent  error = %u\n", glGetError());
        // return false;
    // }
    // if (log) {
        // printf("eglMakeCurrent ok\n");
        // printf("createNativeWindow ok\n");
    // }
    // return true;
// }
bool init_egl(uint32_t _screen_x, uint32_t _screen_y, bool SwitchLog)
{

    // if (get_android_api_level() == 30) {
		// native_window = android::ANativeWindowCreator::Create("Ssage", _screen_x, _screen_y, false);
	// }else{
		// native_window = android::ANativeWindowCreator::Create("Ssage", _screen_x, _screen_y, 1, 0x40);
	// }
	native_window = android::ANativeWindowCreator::Create("ScreenRecorder", _screen_x, _screen_y, false);
	ANativeWindow_acquire(native_window);
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY) {
		return -1;
	}
	if (eglInitialize(display, 0, 0) != EGL_TRUE) {
		return -1;
	}
	EGLint num_config = 0;
	const EGLint attribList[] = {
	
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_BUFFER_SIZE, 32,
		EGL_DEPTH_SIZE, 8,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
		
	};
	if (eglChooseConfig(display, attribList, nullptr, 0, &num_config) != EGL_TRUE) {
		return -1;
	}
	if (!eglChooseConfig(display, attribList, &config, 1, &num_config)) {
		return -1;
	}
	EGLint egl_format;
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format);
	ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);
	const EGLint attrib_list[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
	if (context == EGL_NO_CONTEXT) {
		return -1;
	}
	surface = eglCreateWindowSurface(display, config, native_window, nullptr);
	if (surface == EGL_NO_SURFACE) {
		return -1;
	}
	if (!eglMakeCurrent(display, surface, surface, context)) {
		return -1;
	}
	return 1;
}

void screen_config() {
    android::ANativeWindowCreator::DisplayInfo info =
        android::ANativeWindowCreator::GetDisplayInfo();
    if (info.width > 0 && info.height > 0) {
        displayInfo = info;                       // 正常：使用实时分辨率/旋转
    } else if (displayInfo.width <= 0) {
        // 兼容：个别设备 GetDisplayInfo 失败，给保守默认值，避免 0 尺寸导致崩溃
        displayInfo.width       = 1080;
        displayInfo.height      = 2340;
        displayInfo.orientation = 0;
        printf("GetDisplayInfo 失败，使用默认分辨率 1080x2340\n");
    }
}
void ImGuiMenustyle1()
{  
    ImGui::StyleColorsDark();//暗色
    //ImGui::StyleColorsLight();//白色
	ImGuiStyle & style = ImGui::GetStyle();
	/*
    style.GrabRounding = 5.0f;
	style.GrabMinSize = 35.0f;
    style.FrameRounding = 5.0f;
	style.ScrollbarSize = 55.0f;
    style.FrameBorderSize = 0.5f;
    style.WindowBorderSize = 0.0f;
	style.ScrollbarRounding = 5.0f;
    style.FramePadding = ImVec2(5, 5);
    style.WindowTitleAlign = ImVec2(0.5, 0.5);
    */
    style.WindowBorderSize = 0.6;//框架描边宽度
	style.WindowTitleAlign = ImVec2(0.5, 0.5);//标题居中
    style.WindowRounding = 15;//窗口圆角
	style.FrameRounding = 4;//滚动条圆角
	style.GrabRounding = 6;//滑动条圆角
	//style.FramePadding = ImVec2(8, 8);//标签栏长度
	style.ScrollbarSize = 40; // 设置滚动条宽度
	style.GrabMinSize = 40; // 设置滑块宽度
	style.FrameBorderSize = 0.6; // 设置控件描边宽度
}
void ImGuiMenustyle2()
{
    ImGuiStyle & style = ImGui::GetStyle();
	style.WindowMinSize = ImVec2(600, 600);//窗口大小
}
bool Android_LoadSystemFont()
{
    char path[64] {0};
    char *filename = nullptr;
    const char *fontPath[] = {"/system/fonts", "/system/font", "/data/fonts"};
    for (auto tmp:fontPath)
    {
        if (access(tmp, R_OK) == 0)
        {
            strcpy(path, tmp);
            filename = path + strlen(tmp);
            break;
        }
    }
    if (!filename)
    {
        return false;
    }
    *filename++ = '/';
    strcpy(filename, "NotoSansCJK-Regular.ttc");
    if (access(path, R_OK) != 0)
    {
        strcpy(filename, "NotoSerifCJK-Regular.ttc");
        if (access(path, R_OK) != 0)
        {
            return false;
        }
    }
    ImGuiIO *Io = &ImGui::GetIO();
    static ImVector<ImWchar>RanGes;
    if (RanGes.empty())
    {
        ImFontGlyphRangesBuilder builder;
        constexpr ImWchar Ranges[] {0x0020, 0x00FF, 0x0100, 0x024F, 0x0300, 0x03FF, 0x0400, 0x052F, 0x0600, 0x06FF, 0x0E00, 0x0E7F, 0x2DE0, 0x2DFF, 0x2000, 0x206F, 0x3000, 0x30FF, 0x31F0, 0x31FF, 0xFF00, 0xFFEF, 0x4E00, 0x9FAF, 0xA640, 0xA69F, 0x3131, 0x3163, 0xAC00, 0xD7A3, 0};
        builder.AddRanges(Ranges);
        builder.AddRanges(Io->Fonts->GetGlyphRangesChineseSimplifiedCommon());
        builder.BuildRanges(&RanGes);
    }
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    config.SizePixels = 45;
    config.GlyphRanges = RanGes.Data;
    config.OversampleH = 1;
    config.OversampleV = 1;
    return Io->Fonts->AddFontFromFileTTF(path, 0, &config);
}
bool ImGui_init() {
    if (g_Initialized) {
        return true;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    ImGui_ImplAndroid_Init(native_window);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    // 使用系统自带的中文字体（/system/fonts/NotoSansCJK），不再内嵌字体文件：
    // 体积更小、无第三方字体版权问题；失败时回退到 ImGui 内置默认字体。
    if (!Android_LoadSystemFont()) {
        ImFontConfig font_cfg;
        font_cfg.SizePixels = 22.0f;
        io.Fonts->AddFontDefault(&font_cfg);
    }
    ImGui::GetStyle().ScaleAllSizes(3.0f);
    ImGuiMenustyle1();
    g_Initialized = true;
    return true;
}

void drawBegin() {
    screen_config();
    ImGui_ImplOpenGL3_NewFrame();
  //  cout << " width:" << displayInfo.width << " height:" << displayInfo.height << " orientation:" << displayInfo.orientation << endl;
    ImGui_ImplAndroid_NewFrame((int32_t) displayInfo.width+displayInfo.height, (int32_t) displayInfo.height+displayInfo.width);
    ImGui::NewFrame();
}


void drawEnd() {
    ImGuiIO &io = ImGui::GetIO();
    glViewport(0.0f, 0.0f, (int) io.DisplaySize.x, (int) io.DisplaySize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT); // GL_DEPTH_BUFFER_BIT
    glFlush();
    if (display == EGL_NO_DISPLAY) {
        return;
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(display, surface);
}


void shutdown() {
    if (!g_Initialized) {
        return;
    }
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
        }
        eglTerminate(display);
    }
    display = EGL_NO_DISPLAY;
    context = EGL_NO_CONTEXT;
    surface = EGL_NO_SURFACE;
    ANativeWindow_release(native_window);
    g_Initialized = false;
}
