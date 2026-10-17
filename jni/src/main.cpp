// =============================================================
// fps_monitor - Android 全局实时帧率监测悬浮窗
//
// 窗口创建：android::ANativeWindowCreator::Create()
//   —— 直接调 SurfaceFlinger 的 SurfaceControl 创建图层，
//      不需要 Activity / 不需要 Java / 需要 root。
// 渲染：EGL + OpenGL ES3 + ImGui（复用 Android_draw/draw.cpp 封装）
//
// 显示内容：
//   - 当前 FPS / Frame Time（每帧更新，能看到锯齿）
//   - 最近 1 秒统计：平均 / 最低 / 最高 / 1% Low
//   - 系统全局帧率（SurfaceFlinger，后台线程每 500ms 采样）
//   - VSync / 刷新率
//   - 最近 120 帧历史曲线
//   - 可拖动悬浮球，球上直接显示当前帧率
//
// 用法（root）：
//   su -c /data/local/tmp/fps_monitor
//   后台： nohup /data/local/tmp/fps_monitor > /dev/null 2>&1 &
// =============================================================

#define _GNU_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <climits>
#include <ctime>
#include <unistd.h>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <dirent.h>
#include <cerrno>
#include <linux/input.h>
#include <android/log.h>

#include "Android_draw/draw.h"

#define LOG_TAG "FPS_MON"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================
// 时间工具
// =============================================================
static inline int64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// =============================================================
// 帧率统计器（1 秒滑动窗口，每帧更新）
// =============================================================
struct FpsStats {
    std::deque<std::pair<int64_t, int64_t>> samples;  // (记录时刻ns, 帧耗时ns)
    static const int64_t WINDOW_NS = 1000000000LL;

    float current_fps = 0.f;
    float current_ms  = 0.f;
    float avg_fps     = 0.f;
    float min_fps     = 0.f;
    float max_fps     = 0.f;
    float low_1pct    = 0.f;
    int   frame_count = 0;

    void push(int64_t frame_time_ns) {
        int64_t t = now_ns();
        samples.push_back({t, frame_time_ns});

        current_ms  = frame_time_ns / 1e6f;
        current_fps = frame_time_ns > 0 ? (1e9f / (float)frame_time_ns) : 0.f;

        int64_t cutoff = t - WINDOW_NS;
        while (!samples.empty() && samples.front().first < cutoff)
            samples.pop_front();

        recompute();
    }

    void recompute() {
        size_t n = samples.size();
        frame_count = (int)n;
        if (n == 0) {
            avg_fps = min_fps = max_fps = low_1pct = 0.f;
            return;
        }

        std::vector<int64_t> ft;
        ft.reserve(n);
        double sum = 0.0;
        for (auto& s : samples) {
            if (s.second > 0) {
                ft.push_back(s.second);
                sum += (double)s.second;
            }
        }
        if (ft.empty()) {
            avg_fps = min_fps = max_fps = low_1pct = 0.f;
            return;
        }

        double avg_ms = sum / ft.size();
        avg_fps = avg_ms > 0 ? (float)(1e9 / avg_ms) : 0.f;

        auto [mn, mx] = std::minmax_element(ft.begin(), ft.end());
        min_fps = *mx > 0 ? (float)(1e9 / (double)*mx) : 0.f;   // 最长帧 -> 最低 fps
        max_fps = *mn > 0 ? (float)(1e9 / (double)*mn) : 0.f;   // 最短帧 -> 最高 fps

        // 1% Low：最慢的 1% 帧的平均帧耗时 -> 换算成 fps
        std::sort(ft.begin(), ft.end());                        // 升序
        int lowN = std::max(1, (int)(ft.size() * 0.01f));
        double lowSum = 0.0;
        for (int i = (int)ft.size() - lowN; i < (int)ft.size(); i++)
            lowSum += (double)ft[i];
        double lowAvgMs = lowSum / lowN;
        low_1pct = lowAvgMs > 0 ? (float)(1e9 / lowAvgMs) : 0.f;
    }
};

// =============================================================
// 系统全局帧率（读 SurfaceFlinger，后台线程，非阻塞）
// =============================================================
struct SystemFps {
    std::atomic<float>   sys_fps{0.f};
    std::atomic<float>   vsync_hz{0.f};
    std::atomic<int64_t> last_update{0};
    std::atomic<bool>    running{false};
    std::thread          th;

    void start() {
        running = true;
        th = std::thread([this] { worker(); });
    }
    void stop() {
        running = false;
        if (th.joinable()) th.join();
    }

    void worker() {
        while (running) {
            refresh();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // 取 key 后面的第一个数字
    static bool numAfter(const char* line, const char* key, double* out) {
        const char* p = strstr(line, key);
        if (!p) return false;
        p += strlen(key);
        while (*p && !isdigit((unsigned char)*p) && *p != '-' && *p != '.') p++;
        if (!*p) return false;
        return sscanf(p, "%lf", out) == 1;
    }

    void refresh() {
        FILE* fp = popen("dumpsys SurfaceFlinger 2>/dev/null", "r");
        if (!fp) return;

        static std::deque<int64_t> drawnNs;   // mLastTimeDrawn 采样序列（ns）
        double refreshHz = 0.0;
        double vsyncPeriod = 0.0;

        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) {
            double v;
            if (refreshHz <= 0.0 &&
                (strstr(buf, "Refresh rate") || strstr(buf, "refresh-rate"))) {
                if (numAfter(buf, ":", &v) && v > 1.0 && v < 500.0) refreshHz = v;
            }
            if (vsyncPeriod <= 0.0 &&
                (strstr(buf, "mVsyncPeriod") || strstr(buf, "vsync period"))) {
                if (numAfter(buf, ":", &v) && v > 0.0) vsyncPeriod = v;
            }
            if (strstr(buf, "mLastTimeDrawn")) {
                if (numAfter(buf, "mLastTimeDrawn", &v) && v > 0.0)
                    drawnNs.push_back((int64_t)v);
            }
            if (strstr(buf, "mLastTimeCompleted")) {
                if (numAfter(buf, "mLastTimeCompleted", &v) && v > 0.0)
                    drawnNs.push_back((int64_t)v);
            }
        }
        pclose(fp);

        if (drawnNs.size() > 64) {
            while (drawnNs.size() > 64) drawnNs.pop_front();
        }

        // VSync / 刷新率
        if (vsyncPeriod > 0.0) {
            double periodNs = vsyncPeriod > 1000.0 ? vsyncPeriod : vsyncPeriod * 1e6; // 视单位而定
            if (periodNs > 0) {
                float hz = (float)(1e9 / periodNs);
                if (hz > 1.f && hz < 500.f) vsync_hz.store(hz);
            }
        } else if (refreshHz > 0.0) {
            vsync_hz.store((float)refreshHz);
        }

        // 由 mLastTimeDrawn 时间戳序列估算全局帧率
        if (drawnNs.size() >= 2) {
            std::vector<int64_t> s(drawnNs.begin(), drawnNs.end());
            std::sort(s.begin(), s.end());
            int64_t span = s.back() - s.front();
            int64_t cnt  = (int64_t)s.size() - 1;
            if (span > 0 && cnt > 0) {
                double intervalNs = (double)span / (double)cnt;
                double fps = 1e9 / intervalNs;
                if (fps > 1.0 && fps < 1000.0) sys_fps.store((float)fps);
            }
        }

        last_update.store(now_ns());
    }
};

// =============================================================
// 触摸读取（只读，不 EVIOCGRAB，所以不影响手机正常使用）
//   ImGui 的 Android 后端不会自己喂事件，必须直接读 /dev/input/event*
// =============================================================
struct TouchReader {
    int   fd = -1;
    int   minX = 0, maxX = 0, minY = 0, maxY = 0;
    bool  useMT = false;              // true=多点协议B(ABS_MT_*)，false=单点/协议A(ABS_X/Y)
    char  dev[64] = {0};
    float touch_s = 1.f;

    std::atomic<int>  tx{0};
    std::atomic<int>  ty{0};
    std::atomic<bool> down{false};
    std::atomic<bool> running{false};
    std::thread       th;

    bool ok() const { return fd >= 0; }

    bool init() {
        // 遍历所有输入设备，选最像触摸屏的那个：
        //   多点协议B(ABS_MT_*)=2 分，单点/协议A(ABS_X/Y)=1 分，带 BTN_TOUCH 再加 2 分
        int bestScore = -1;
        for (int i = 0; i < 32; i++) {
            char path[64];
            snprintf(path, sizeof(path), "/dev/input/event%d", i);
            int f = open(path, O_RDONLY | O_NONBLOCK);
            if (f < 0) continue;

            struct input_absinfo ix, iy;
            bool hasMT = (ioctl(f, EVIOCGABS(ABS_MT_POSITION_X), &ix) == 0 && ix.maximum > 0 &&
                          ioctl(f, EVIOCGABS(ABS_MT_POSITION_Y), &iy) == 0 && iy.maximum > 0);
            if (!hasMT) {
                // 老设备/单点触摸回退到 ABS_X / ABS_Y
                if (!(ioctl(f, EVIOCGABS(ABS_X), &ix) == 0 && ix.maximum > 0 &&
                      ioctl(f, EVIOCGABS(ABS_Y), &iy) == 0 && iy.maximum > 0)) {
                    close(f);
                    continue;
                }
            }

            unsigned long keybits[(KEY_MAX / 8) + 1] = {0};
            ioctl(f, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
            bool hasTouch = (keybits[BTN_TOUCH / 8] >> (BTN_TOUCH % 8)) & 1;

            int score = (hasMT ? 2 : 1) + (hasTouch ? 2 : 0);
            if (score > bestScore) {
                if (fd >= 0) close(fd);
                fd = f;
                bestScore = score;
                minX = ix.minimum; maxX = ix.maximum;
                minY = iy.minimum; maxY = iy.maximum;
                useMT = hasMT;
                snprintf(dev, sizeof(dev), "%s", path);
            } else {
                close(f);
            }
        }
        if (fd < 0) {
            LOGE("未找到触摸设备 /dev/input/event*（触摸不可用）");
            return false;
        }

        // 触摸坐标范围 -> 屏幕像素：与葫芦娃一致
        int rw = maxX - minX, rh = maxY - minY;
        if (rw <= 0) rw = maxX;
        if (rh <= 0) rh = maxY;
        int dw = displayInfo.width  > 0 ? displayInfo.width  : rw;
        int dh = displayInfo.height > 0 ? displayInfo.height : rh;
        touch_s = (float)(rw + rh) / (float)(dw + dh);
        if (touch_s <= 0.f) touch_s = 1.f;

        LOGI("触摸设备 %s: %s minX=%d maxX=%d minY=%d maxY=%d touch_s=%.3f",
             dev, useMT ? "多点协议B" : "单点/协议A", minX, maxX, minY, maxY, touch_s);
        return true;
    }

    void start() {
        if (!ok()) return;
        running = true;
        th = std::thread([this] { worker(); });
    }
    void stop() {
        running = false;
        if (th.joinable()) th.join();
        if (fd >= 0) { close(fd); fd = -1; }
    }

    void worker() {
        struct input_event ev;
        int  rawX = 0, rawY = 0;
        bool tracking = false;

        while (running) {
            struct pollfd pfd{ fd, POLLIN, 0 };
            int r = poll(&pfd, 1, 100);
            if (r <= 0) continue;

            while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type == EV_ABS) {
                    if      (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) rawX = ev.value;
                    else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) rawY = ev.value;
                    else if (ev.code == ABS_MT_TRACKING_ID) tracking = (ev.value != -1);
                } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                    tracking = (ev.value != 0);
                } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                    int   o  = displayInfo.orientation;
                    float rw = (float)(maxX - minX), rh = (float)(maxY - minY);
                    float nx = (float)(rawX - minX), ny = (float)(rawY - minY);
                    float rx = nx, ry = ny;
                    if (o == 3)      { rx = rh - ny; ry = nx; }
                    else if (o == 2) { rx = rw - nx; ry = rh - ny; }
                    else if (o == 1) { rx = ny;      ry = rw - nx; }

                    tx.store((int)(rx / touch_s));
                    ty.store((int)(ry / touch_s));
                    down.store(tracking);
                }
            }
        }
    }
};

// =============================================================
// 音量键读取（上=开面板，下=关面板；帧率球始终保留）
//   监听所有带音量键的 /dev/input/event*，诊断信息打到 logcat(FPS_MON)
// =============================================================
struct KeyReader {
    std::vector<int> fds;         // 所有音量键设备
    std::vector<int> grabbed;     // 被独占的设备
    std::atomic<int>  cmd{0};     // +1 开, -1 关
    std::atomic<bool> running{false};
    std::thread       th;

    static bool bitSet(const unsigned long* b, int code) {
        return (b[code / (8 * sizeof(long))] >> (code % (8 * sizeof(long)))) & 1UL;
    }

    bool init() {
        DIR* d = opendir("/dev/input");
        if (!d) { LOGE("打不开 /dev/input (errno=%d)", errno); return false; }

        std::vector<int> fallback;
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            if (strncmp(e->d_name, "event", 5) != 0) continue;
            char path[128];
            snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
            int f = open(path, O_RDONLY | O_NONBLOCK);
            if (f < 0) { LOGE("打不开 %s (errno=%d)", path, errno); continue; }

            unsigned long keybits[(KEY_MAX / (8 * sizeof(long))) + 1] = {0};
            if (ioctl(f, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) != 0) {
                fallback.push_back(f);           // 查不到能力位就先留着
                continue;
            }
            bool hasVolUp = bitSet(keybits, KEY_VOLUMEUP);
            bool hasVolDn = bitSet(keybits, KEY_VOLUMEDOWN);
            if (!hasVolUp && !hasVolDn) { close(f); continue; }

            fds.push_back(f);
            bool hasPower = bitSet(keybits, KEY_POWER);
            if (!hasPower && ioctl(f, EVIOCGRAB, 1) == 0) grabbed.push_back(f);
            LOGI("监听音量键: %s (volUp=%d volDn=%d power=%d grab=%d)",
                 path, (int)hasVolUp, (int)hasVolDn, (int)hasPower,
                 (int)(!hasPower && !grabbed.empty() && grabbed.back() == f));
        }
        closedir(d);

        if (fds.empty()) {
            // 能力位都读不到（部分设备 SELinux 拦截）→ 退而监听所有可开节点
            LOGI("未匹配到音量键能力位，改为监听全部可读设备 (%zu)", fallback.size());
            for (int f : fallback) fds.push_back(f);
        } else {
            for (int f : fallback) close(f);
        }

        if (fds.empty()) { LOGE("音量键不可用：没有可读的 /dev/input 设备"); return false; }
        LOGI("音量键监听 %zu 个设备", fds.size());
        return true;
    }

    void start() {
        if (fds.empty()) return;
        running = true;
        th = std::thread([this] { worker(); });
    }
    void stop() {
        running = false;
        if (th.joinable()) th.join();
        for (int f : grabbed) ioctl(f, EVIOCGRAB, 0);
        for (int f : fds) close(f);
        fds.clear();
        grabbed.clear();
    }

    void worker() {
        std::vector<pollfd> pfds(fds.size());
        for (size_t i = 0; i < fds.size(); i++) { pfds[i].fd = fds[i]; pfds[i].events = POLLIN; }

        struct input_event ev;
        while (running) {
            int r = poll(pfds.data(), pfds.size(), 100);
            if (r <= 0) continue;
            for (size_t i = 0; i < pfds.size(); i++) {
                if (!(pfds[i].revents & POLLIN)) continue;
                while (read(pfds[i].fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                    if (ev.type != EV_KEY) continue;
                    if (ev.code != KEY_VOLUMEUP && ev.code != KEY_VOLUMEDOWN) continue;
                    LOGI("音量键 code=%d value=%d", ev.code, ev.value);
                    if (ev.value == 1) {                    // 只认按下
                        if (ev.code == KEY_VOLUMEUP)        cmd.store(+1);
                        else                                cmd.store(-1);
                    }
                }
            }
        }
    }
};

// =============================================================
// 全局状态
// =============================================================
static std::atomic<bool> g_running{true};

// 全局 UI 缩放：按屏幕短边自适应（基准 1080），兼容小屏/高分屏/平板
static float g_ui = 1.0f;

static FpsStats   g_fps;
static SystemFps  g_sysfps;
static TouchReader g_touch;
static KeyReader   g_keys;

// 历史曲线（最近 120 帧）
static std::deque<float> g_curve;
static const size_t CURVE_MAX = 120;

// 悬浮球
static bool  g_show_ball = true;
static float g_ball_x = -1.f, g_ball_y = -1.f;
static float g_ball_size = 150.f;
static bool  g_ball_dragging = false;
static float g_ball_accum = 0.f;
static float g_ball_offx = 0.f, g_ball_offy = 0.f;

// 主面板
static bool  g_show_panel = true;
static float g_panel_x = -1.f, g_panel_y = -1.f;
static float g_panel_w = 900.f;
static bool  g_force_panel_pos = false;
static bool  g_show_curve = true;

static bool  g_have_prev = false;
static int64_t g_last_frame_ns = 0;

// =============================================================
// 悬浮球
// =============================================================
static void drawFloatBall() {
    if (!g_show_ball) return;
    ImGuiIO& io = ImGui::GetIO();

    float r = g_ball_size * 0.5f;
    ImVec2 c(g_ball_x + r, g_ball_y + r);

    float dx = io.MousePos.x - c.x;
    float dy = io.MousePos.y - c.y;
    bool hovered = (dx * dx + dy * dy) <= (r * r);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !g_ball_dragging) {
        g_ball_dragging = true;
        g_ball_accum = 0.f;
        // 记录按下时手指与球左上角的偏移，之后球 = 手指 - 偏移（绝对跟随，不累计漂移）
        g_ball_offx = io.MousePos.x - g_ball_x;
        g_ball_offy = io.MousePos.y - g_ball_y;
    }
    if (g_ball_dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            g_ball_x = io.MousePos.x - g_ball_offx;
            g_ball_y = io.MousePos.y - g_ball_offy;
            g_ball_accum += fabsf(io.MouseDelta.x) + fabsf(io.MouseDelta.y);
        } else {
            g_ball_dragging = false;
            if (g_ball_accum < 12.f * g_ui)   // 没怎么动 -> 视为点击
                g_show_panel = !g_show_panel;
        }
    }

    float W = (float)displayInfo.width;
    float H = (float)displayInfo.height;
    if (W > 0 && H > 0) {
        g_ball_x = std::max(0.f, std::min(g_ball_x, W - g_ball_size));
        g_ball_y = std::max(0.f, std::min(g_ball_y, H - g_ball_size));
    }
    c = ImVec2(g_ball_x + r, g_ball_y + r);

    // 帧率决定描边颜色：绿/黄/红
    float f = g_fps.current_fps;
    ImU32 ring = f >= 55.f ? IM_COL32(70, 220, 130, 255)
               : f >= 30.f ? IM_COL32(255, 200, 70, 255)
                           : IM_COL32(255, 90, 90, 255);
    ImU32 glow = (ring & 0x00FFFFFFu) | (60u << 24);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddCircleFilled(c, r + 10.f * g_ui, glow, 72);                        // 外发光
    dl->AddCircleFilled(c, r, IM_COL32(14, 20, 34, 235), 72);                 // 球体
    dl->AddCircle(c, r, ring, 72, 6.f * g_ui);                                // 彩色描边
    dl->AddCircle(c, r - 14.f * g_ui, IM_COL32(255, 255, 255, 40), 72, 2.f * g_ui);

    char t[32];
    snprintf(t, sizeof(t), "%.0f", f);
    ImVec2 ts = ImGui::CalcTextSize(t);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.62f),
                IM_COL32(255, 255, 255, 255), t);
    const char* lbl = "FPS";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + ts.y * 0.28f),
                IM_COL32(150, 185, 230, 255), lbl);
}

// =============================================================
// 主题（在 draw.cpp 的 ImGui_init 之后调用一次）
// =============================================================
static void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding     = ImVec2(28 * g_ui, 24 * g_ui);
    s.ItemSpacing       = ImVec2(18 * g_ui, 16 * g_ui);
    s.WindowRounding    = 22.f * g_ui;
    s.ChildRounding     = 16.f * g_ui;
    s.FrameRounding     = 10.f * g_ui;
    s.GrabRounding      = 10.f * g_ui;
    s.PopupRounding     = 12.f * g_ui;
    s.ScrollbarRounding = 12.f * g_ui;
    s.WindowBorderSize  = 2.f * g_ui;
    s.FrameBorderSize   = 0.f;
    s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    s.FramePadding      = ImVec2(12 * g_ui, 9 * g_ui);
    s.ScrollbarSize     = 40 * g_ui;
    s.GrabMinSize       = 40 * g_ui;

    const ImVec4 accent(0.30f, 0.63f, 1.00f, 1.0f);
    auto& c = s.Colors;
    c[ImGuiCol_WindowBg]      = ImVec4(0.045f, 0.055f, 0.085f, 0.88f);
    c[ImGuiCol_ChildBg]       = ImVec4(0.075f, 0.095f, 0.150f, 0.95f);
    c[ImGuiCol_Border]        = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    c[ImGuiCol_TitleBg]       = ImVec4(0.07f, 0.09f, 0.15f, 0.95f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.16f, 0.30f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.09f, 0.15f, 0.95f);
    c[ImGuiCol_Text]          = ImVec4(0.92f, 0.95f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]  = ImVec4(0.55f, 0.62f, 0.72f, 1.00f);
    c[ImGuiCol_FrameBg]       = ImVec4(0.12f, 0.14f, 0.20f, 0.85f);
    c[ImGuiCol_FrameBgHovered]= ImVec4(0.16f, 0.20f, 0.30f, 0.90f);
    c[ImGuiCol_Button]        = ImVec4(accent.x, accent.y, accent.z, 0.32f);
    c[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    c[ImGuiCol_ButtonActive]  = ImVec4(accent.x, accent.y, accent.z, 0.85f);
    c[ImGuiCol_CheckMark]     = accent;
    c[ImGuiCol_Separator]     = ImVec4(accent.x, accent.y, accent.z, 0.30f);
    c[ImGuiCol_PlotLines]     = accent;
}

// =============================================================
// 主面板
// =============================================================
static void drawPanel() {
    if (!g_show_panel) return;

    if (g_panel_x < 0 || g_panel_y < 0) {
        g_panel_x = displayInfo.width * 0.04f;
        g_panel_y = displayInfo.height * 0.05f;
    }

    if (displayInfo.width > 0)
        g_panel_w = std::min(g_panel_w, (float)displayInfo.width * 0.92f);
    ImGui::SetNextWindowSize(ImVec2(g_panel_w, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(g_panel_x, g_panel_y),
                            (g_force_panel_pos ? ImGuiCond_Always : ImGuiCond_Once));
    g_force_panel_pos = false;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    ImGui::Begin("帧率监测", &g_show_panel, flags);

    g_panel_x = ImGui::GetWindowPos().x;
    g_panel_y = ImGui::GetWindowPos().y;
    g_panel_w = ImGui::GetWindowSize().x;

    const ImVec4 accent(0.30f, 0.63f, 1.00f, 1.0f);
    const ImVec4 good  (0.35f, 0.90f, 0.55f, 1.0f);
    const ImVec4 warn  (1.00f, 0.75f, 0.30f, 1.0f);

    // ---- 顶部大号当前 FPS 卡片 ----
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.13f, 0.21f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(accent.x, accent.y, accent.z, 0.55f));
    ImGui::BeginChild("##fpscard", ImVec2(0, 270 * g_ui), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowFontScale(2.6f);
    ImGui::TextColored(accent, "%.0f", g_fps.current_fps);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine(0.f, 26.f);
    ImGui::TextDisabled("FPS");
    ImGui::Spacing();
    ImGui::Text("帧耗时    %.2f ms", g_fps.current_ms);
    ImGui::Text("窗口刷新  %d 帧/秒", g_fps.frame_count);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ---- 最近 1 秒统计 ----
    ImGui::TextColored(good, "最近 1 秒统计");
    ImGui::Separator();
    ImGui::Columns(2, "##stats", false);
    ImGui::Text("平均  %.1f", g_fps.avg_fps);
    ImGui::Text("最高  %.1f", g_fps.max_fps);
    ImGui::NextColumn();
    ImGui::Text("最低  %.1f", g_fps.min_fps);
    ImGui::TextColored(warn, "1%% Low  %.1f", g_fps.low_1pct);
    ImGui::Columns(1);

    ImGui::Spacing();

    // ---- 系统全局帧率 ----
    ImGui::TextColored(accent, "系统 / SurfaceFlinger");
    ImGui::Separator();
    float sfps = g_sysfps.sys_fps.load();
    if (sfps > 0.f) ImGui::Text("系统 FPS  %.1f", sfps);
    else            ImGui::TextDisabled("系统 FPS  --（无数据）");
    ImGui::SameLine(0.f, 40.f);
    ImGui::Text("VSync  %.1f Hz", g_sysfps.vsync_hz.load());
    int64_t age_ms = (now_ns() - g_sysfps.last_update.load()) / 1000000;
    ImGui::TextDisabled("数据更新于 %lld ms 前", (long long)age_ms);

    ImGui::Spacing();

    ImGui::Separator();

    // ---- 历史曲线 ----
    if (g_show_curve && g_curve.size() > 1) {
        ImGui::TextColored(accent, "最近 %d 帧曲线", (int)g_curve.size());
        ImGui::Separator();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float aw = std::min(g_panel_w - 80.f * g_ui, 820.f * g_ui);
        float ah = 220.f * g_ui;
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(aw, ah));

        dl->AddRectFilled(p0, ImVec2(p0.x + aw, p0.y + ah),
                          IM_COL32(12, 17, 30, 220), 10.f * g_ui);
        dl->AddRect(p0, ImVec2(p0.x + aw, p0.y + ah),
                    IM_COL32(70, 95, 140, 190), 10.f * g_ui, 0, 2.f * g_ui);

        float maxFps = 144.f;
        for (float f : g_curve) if (f > maxFps) maxFps = f;

        // 参考线（60 / 120）
        for (float ref : {60.f, 120.f}) {
            if (ref > maxFps) continue;
            float y = p0.y + ah - ah * (ref / maxFps);
            dl->AddLine(ImVec2(p0.x, y), ImVec2(p0.x + aw, y),
                        IM_COL32(90, 110, 150, 120), 1.f);
        }

        int n = (int)g_curve.size();
        std::vector<ImVec2> pts;
        pts.reserve(n);
        for (int i = 0; i < n; i++) {
            float x = p0.x + aw * i / (float)(CURVE_MAX - 1);
            float y = p0.y + ah - ah * (g_curve[i] / maxFps);
            pts.push_back(ImVec2(x, y));
        }
        dl->AddPolyline(pts.data(), (int)pts.size(),
                        IM_COL32(90, 210, 255, 255),
                        ImDrawFlags_None, 3.5f * g_ui);
        dl->AddCircleFilled(pts.back(), 7.f * g_ui, IM_COL32(255, 110, 110, 255));

        char txt[32];
        snprintf(txt, sizeof(txt), "%.0f", maxFps);
        dl->AddText(ImVec2(p0.x + 10 * g_ui, p0.y + 6 * g_ui), IM_COL32(180, 200, 225, 255), txt);
        dl->AddText(ImVec2(p0.x + 10 * g_ui, p0.y + ah - 48 * g_ui), IM_COL32(180, 200, 225, 255), "0");
    }

    ImGui::Spacing();

    // ---- 设置 ----
    ImGui::TextColored(accent, "设置");
    ImGui::Separator();
    ImGui::Checkbox("显示曲线", &g_show_curve);
    ImGui::SameLine(0.f, 40.f);
    ImGui::Checkbox("显示悬浮球", &g_show_ball);

    ImGui::Spacing();
    float bw = (g_panel_w - 90.f * g_ui) * 0.5f;
    if (ImGui::Button("重置位置", ImVec2(bw, 96 * g_ui))) {
        g_ball_x = displayInfo.width - g_ball_size - 30.f;
        g_ball_y = displayInfo.height * 0.3f;
        g_panel_x = displayInfo.width * 0.04f;
        g_panel_y = displayInfo.height * 0.05f;
        g_force_panel_pos = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("退出", ImVec2(bw, 96 * g_ui))) {
        g_running = false;
    }

    ImGui::End();
}

// =============================================================
// main
// =============================================================
int main(int argc, char** argv) {
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    if (getuid() != 0) {
        LOGE("需要 root 权限运行（直接操作 SurfaceFlinger）");
    }

    LOGI("fps_monitor 启动");

    if (!initDraw(false)) {
        LOGE("initDraw 失败（窗口/EGL/ImGui 初始化失败）");
        return -1;
    }

    // 按屏幕短边自适应 UI（基准 1080），兼容小屏 / 高分屏 / 平板
    {
        int shortSide = std::min(displayInfo.width, displayInfo.height);
        if (shortSide <= 0) shortSide = 1080;
        g_ui = std::min(2.0f, std::max(0.7f, shortSide / 1080.0f));
        ImGui::GetIO().FontGlobalScale = g_ui;
        g_ball_size = 150.f * g_ui;
        g_panel_w   = std::min(900.f * g_ui, (float)displayInfo.width * 0.92f);
        LOGI("屏幕 %dx%d 旋转=%d  UI缩放=%.2f",
             displayInfo.width, displayInfo.height, displayInfo.orientation, g_ui);
    }

    applyTheme();

    // 悬浮球初始位置
    g_ball_x = std::max(0.f, displayInfo.width - g_ball_size - 30.f);
    g_ball_y = displayInfo.height * 0.3f;

    g_sysfps.start();
    g_touch.init();
    g_touch.start();
    g_keys.init();
    g_keys.start();

    while (g_running) {
        // 把 /dev/input 的触摸喂给 ImGui（必须在 drawBegin 之前）
        if (g_touch.ok()) {
            ImGuiIO& io = ImGui::GetIO();
            io.AddMousePosEvent((float)g_touch.tx.load(), (float)g_touch.ty.load());
            io.AddMouseButtonEvent(0, g_touch.down.load());
        }

        // 音量键：上=开面板，下=关面板（帧率球始终保留）
        int keyCmd = g_keys.cmd.exchange(0);
        if (keyCmd > 0)      g_show_panel = true;
        else if (keyCmd < 0) g_show_panel = false;

        int64_t t = now_ns();
        if (g_have_prev) {
            int64_t dt = t - g_last_frame_ns;
            if (dt > 0 && dt < 2000000000LL) {       // 忽略 >2s 的异常（如息屏）
                g_fps.push(dt);
                g_curve.push_back(g_fps.current_fps);
                if (g_curve.size() > CURVE_MAX) g_curve.pop_front();
            }
        }
        g_last_frame_ns = t;
        g_have_prev = true;

        drawBegin();      // ImGui 画图开始
        drawFloatBall();
        drawPanel();
        drawEnd();        // ImGui 画图结束 + 提交

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    g_keys.stop();
    g_touch.stop();
    g_sysfps.stop();
    shutdown();

    LOGI("fps_monitor 退出");
    return 0;
}
