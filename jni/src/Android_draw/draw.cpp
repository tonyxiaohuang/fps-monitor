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
	__android_log_print(ANDROID_LOG_INFO, "FPS_MON", "Create 窗口=%p (%ux%u)", native_window, _screen_x, _screen_y);
	if (native_window == nullptr) {
		__android_log_print(ANDROID_LOG_ERROR, "FPS_MON", "创建窗口失败：Create() 返回空指针（需要 root / 设备不支持）");
		return 0;
	}
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
    ImGuiIO *Io = &ImGui::GetIO();

    // 需要的字形范围：ASCII/拉丁 + 界面实际用到的汉字
    // （范围过大 = 全 CJK 3 万+ 字形，图集 >1GB 会 OOM 崩溃）
    static ImVector<ImWchar> RanGes;
    if (RanGes.empty())
    {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(Io->Fonts->GetGlyphRangesDefault());
        static const char* ui_zh =
            "一不个为主于位低作入全出初到制刷前力动化匹协单口可听启均备多失始完屏已帧幕平应开循悬成打找据接摸操改放数新旋无时显曲更最有未权没测浮点率环球用的监直示秒程窗第系线绘统缩置耗能行要触计议设读调败转运近进退部配重量键限需音题高（），：";
        builder.AddText(ui_zh);
        builder.BuildRanges(&RanGes);
    }

    // 候选字体：stb_truetype 只认 TrueType(glyf) 与部分 CFF，
    // NotoSansCJK 是 CID-keyed CFF（stb 常失败），所以优先真 TrueType 中文字体。
    static const char* cands[] = {
        "/system/fonts/MiSansVF.ttf",              // 小米 MiSans（TrueType 变量字体）
        "/system/fonts/MiSansTCVF.ttf",
        "/system/fonts/HONORSansCN-Regular.ttf",   // 荣耀
        "/system/fonts/DroidSansFallbackFull.ttf", // 老设备
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/NotoSansCJK-Regular.ttc",   // 部分设备（CFF，可能不被支持）
        "/system/fonts/NotoSerifCJK-Regular.ttc",
        "/system/fonts/NotoSansSC-Regular.otf",
    };

    for (const char* p : cands)
    {
        if (access(p, R_OK) != 0) continue;
        Io->Fonts->Clear();                        // 上一个字体若不可用，清空重来
        ImFontConfig cfg;
        cfg.SizePixels = 45;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        ImFont* f = Io->Fonts->AddFontFromFileTTF(p, 45.0f, &cfg, RanGes.Data);
        if (f != nullptr && Io->Fonts->Build())
        {
            __android_log_print(ANDROID_LOG_INFO, "FPS_MON",
                                "中文字体 OK: %s 图集=%dx%d", p, Io->Fonts->TexWidth, Io->Fonts->TexHeight);
            return true;
        }
        __android_log_print(ANDROID_LOG_WARN, "FPS_MON", "字体不兼容，跳过: %s", p);
    }

    // 都不行 → 内置默认字体（无中文，但保证不崩）
    Io->Fonts->Clear();
    ImFontConfig fc;
    fc.SizePixels = 22.0f;
    Io->Fonts->AddFontDefault(&fc);
    Io->Fonts->Build();
    __android_log_print(ANDROID_LOG_ERROR, "FPS_MON",
                        "未找到可用中文字体，回退内置默认字体（中文可能显示为方块）");
    return false;
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
    // 内部会逐个探测候选字体，选中可用的那个并构建好图集；失败会回退内置默认字体（不会崩）
    Android_LoadSystemFont();
    ImGui::GetStyle().ScaleAllSizes(3.0f);
    ImGuiMenustyle1();
    g_Initialized = true;
    return true;
}

void drawBegin() {
    static bool s_first = true;
    if (s_first) __android_log_print(ANDROID_LOG_INFO, "FPS_MON", "[B1] screen_config ...");
    screen_config();
    if (s_first) __android_log_print(ANDROID_LOG_INFO, "FPS_MON", "[B2] OpenGL3_NewFrame ...");
    ImGui_ImplOpenGL3_NewFrame();
  //  cout << " width:" << displayInfo.width << " height:" << displayInfo.height << " orientation:" << displayInfo.orientation << endl;
    if (s_first) __android_log_print(ANDROID_LOG_INFO, "FPS_MON", "[B3] Android_NewFrame ...");
    ImGui_ImplAndroid_NewFrame((int32_t) displayInfo.width+displayInfo.height, (int32_t) displayInfo.height+displayInfo.width);
    if (s_first) __android_log_print(ANDROID_LOG_INFO, "FPS_MON", "[B4] ImGui::NewFrame ...");
    ImGui::NewFrame();
    if (s_first) { __android_log_print(ANDROID_LOG_INFO, "FPS_MON", "[B5] drawBegin 完成"); s_first = false; }
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
