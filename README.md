# fps_monitor：Android 全局实时帧率监测悬浮窗

一个用 **纯 NDK（C++）+ EGL + OpenGL ES3 + ImGui** 写的 Android 悬浮窗程序，
在**不写 Java、不依赖 Activity**的前提下，用 root 权限直接在屏幕上叠加一层实时帧率监控。

它绕过了 Android 普通悬浮窗（`SYSTEM_ALERT_WINDOW`）的限制，直接调用
SurfaceFlinger 的 `SurfaceControl` 创建图层，因此可以覆盖在任何界面之上运行。

---

## 一、它能做什么

- **实时帧率（FPS）**：每帧更新，能看到帧率的实时抖动，而不是被平滑过的数字。
- **帧耗时（Frame Time, ms）**：每帧耗时，越低越流畅。
- **最近 1 秒统计**：平均 / 最低 / 最高 / **1% Low**（1% Low 更能反映卡顿）。
- **系统全局帧率**：后台线程每 500ms 解析一次 `dumpsys SurfaceFlinger`，
  给出系统层面的合成帧率与 VSync / 刷新率。
- **最近 120 帧历史曲线**：直观看到帧率的波动趋势。
- **可拖动悬浮球**：球体描边颜色随帧率变化（绿=流畅 / 黄=一般 / 红=卡顿），
  球上直接显示当前帧率，单击可以开关详细面板。

## 二、怎么用

### 前置条件

- **已 root** 的 Android 手机（程序用 root 直接操作 SurfaceFlinger）。
- 用 AIDE / NDK 编译（见下方「编译」）。

### 操作方式

| 操作 | 效果 |
|------|------|
| 拖动悬浮球 | 移动球的位置（绝对跟手，不飘） |
| 单击悬浮球 | 打开 / 关闭详细面板 |
| 音量上键 | 打开详细面板 |
| 音量下键 | 关闭详细面板（悬浮球始终保留） |
| 面板内「重置位置」 | 球和面板回到默认位置 |
| 面板内「退出」 | 结束程序 |

> 悬浮球始终显示，不会被关掉；音量键只是控制详细面板的显示。

### 运行

```sh
# 前台运行
su -c /data/local/tmp/fps_monitor

# 后台运行
su -c "nohup /data/local/tmp/fps_monitor > /dev/null 2>&1 &"
```

## 三、编译

### 一键脚本（推荐，AIDE 环境）

```sh
su -c "sh /storage/emulated/0/fps_monitor/编译并运行.sh"
```

脚本会依次完成：调用 AIDE 自带的 `ndk-build` 编译 → 把产物复制到
`/data/local/tmp/fps_monitor` → 用 root 运行。

> 如果报错找不到 `ndk-build`，请打开 `编译并运行.sh`，
> 把 `NDK_BUILD=` 那一行改成你手机上 AIDE 的 `ndk-build` 实际路径。

### 手动编译

```sh
cd /storage/emulated/0/fps_monitor
<你的 ndk-build 路径>          # 在 jni/ 目录下编译
# 产物在 libs/<abi>/fps_monitor
```

## 四、工作原理（为什么需要 root）

普通 App 做悬浮窗要走 `SYSTEM_ALERT_WINDOW` 权限 + `WindowManager` + Java，
而且受窗口层级限制。本项目换了一条路：

1. **建窗口**：`android::ANativeWindowCreator::Create()`
   直接调 SurfaceFlinger / SurfaceControl 创建一个屏幕图层，拿到 `ANativeWindow`。
   这一步不需要 Activity、不需要 Java VM，但**需要 root**。
2. **渲染**：用 EGL 创建 OpenGL ES3 上下文，配合 ImGui 绘制界面。
3. **读触摸**：ImGui 的 Android 后端不会主动喂触摸事件（没有 Activity 就没有
   事件回调），所以**直接读 `/dev/input/event*`** 把触摸坐标喂给 ImGui。
   采用**只读、不独占（不 EVIOCGRAB）**的方式，触摸会正常透传给底层 App，
   不影响手机正常使用。
4. **读系统帧率**：后台线程定期执行 `dumpsys SurfaceFlinger` 并解析输出。
5. **读音量键**：遍历 `/dev/input/event*`，找到带 `KEY_VOLUMEUP / KEY_VOLUMEDOWN`
   的节点监听按键。

## 五、全设备兼容性设计

程序针对不同机型做了多处自适应，尽量做到「换设备也能用」：

- **UI 自适应缩放**：以屏幕短边 / 1080 为缩放系数（限制在 0.7~2.0 之间），
  字体、间距、圆角、悬浮球、面板、曲线、按钮等尺寸全部按比例缩放。
  小屏、高分屏、平板都不会错位或过大过小。
- **触摸协议兼容**：自动扫描并"打分"选择触摸设备——
  支持多点协议 B（`ABS_MT_POSITION_X/Y`），也能回退到单点 / 协议 A（`ABS_X/Y`），
  优先选择带 `BTN_TOUCH` 的设备；坐标会先减去面板 `minimum` 再按屏幕旋转方向换算。
- **旋转兼容**：按 `displayInfo.orientation` 把触摸坐标旋转到当前屏幕方向。
- **分辨率兜底**：若某设备 `GetDisplayInfo()` 失败返回 0，会保留上一次有效值，
  首次失败则用 1080x2340 的默认值，避免 0 尺寸窗口导致崩溃。
- **音量键兼容**：遍历所有输入设备，能力位读不到时退化为监听全部可读设备。

## 六、目录结构

```
fps_monitor/
├── 编译并运行.sh              # 一键编译 + 部署 + 运行
├── README.md                  # 本介绍
└── jni/
    ├── Android.mk             # ndk-build 配置（可执行文件 fps_monitor）
    ├── Application.mk         # 目标 ABI / 最低 API / STL
    ├── include/
    │   ├── ImGui/             # ImGui 头文件
    │   ├── native_surface/
    │   │   └── ANativeWindowCreator.h   # 创建窗口（核心）
    │   └── Android_draw/
    │       └── draw.h         # 窗口 / EGL / ImGui 封装声明
    └── src/
        ├── main.cpp           # 主程序：帧率逻辑、触摸、音量键、界面
        ├── Android_draw/
        │   └── draw.cpp       # EGL / ImGui 初始化与每帧提交
        └── ImGui/             # ImGui 源码 + Android / OpenGL3 后端
```

## 七、常见问题

**Q：启动后什么都没显示？**
A：确认是以 root 运行（`id -u` 是否为 0），并查看日志
`su -c "logcat -d -s FPS_MON"`。

**Q：系统 FPS 显示 `--`？**
A：说明 `dumpsys SurfaceFlinger` 的解析没匹配到，不同 Android 版本的输出格式不同。
不影响当前帧率和本地统计，可后续适配。

**Q：音量键没反应？**
A：先 `su -c getevent -lt` 按一下音量键，确认对应的是哪个 `/dev/input/eventX` 和
keycode，再对照 `logcat -d -s FPS_MON` 里的监听日志排查。

**Q：按音量键会同时调音量吗？**
A：如果音量键所在节点**不含**电源键，程序会 `EVIOCGRAB` 独占，按键不会影响系统音量；
如果音量键与电源键在**同一节点**，为避免抢走电源键（导致无法锁屏），程序改为只读观察，
此时按音量键会同时调节系统音量。

**Q：需要装 Xposed / LSPosed / Magisk 模块吗？**
A：不需要。只需要 root。

## 八、注意事项

- 本程序需要 **root**，请自行评估风险。
- 程序只读取触摸与音量键事件、只读取系统帧率信息，**不修改、不注入**其他 App。
- 仅供性能观察 / 调试使用。

## 九、第三方组件与致谢

- **Dear ImGui**（MIT License）—— 界面绘制，见 `jni/` 下各源码文件头部声明。
- **ANativeWindowCreator**（`jni/include/native_surface/ANativeWindowCreator.h`）——
  直接创建 SurfaceFlinger 窗口图层，是本项目能「免 Java 悬浮窗」的关键。
- 中文字体**不内嵌字体文件**：运行时从 `/system/fonts` 逐个探测可用字体并做「能不能被 stb_truetype 解析」的验证，
  优先小米 `MiSansVF.ttf` 等真 TrueType 中文字体（`NotoSansCJK` 是 CFF 轮廓，stb 渲染不了，会自动跳过），
  全部不可用才回退 ImGui 内置字体（此时中文可能显示为方块，但不会崩溃）。

## 十、许可证

本项目基于 **MIT License** 发布，详见 [LICENSE](LICENSE)。
