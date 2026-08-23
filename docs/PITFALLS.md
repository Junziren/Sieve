# SortSynth JUCE/VST3 踩坑日志

## Phase 0: 骨架 (CMake + 空插件)
[2025-06-05] C++ 编译器未安装 → D:\VS2022BuildTools 已安装
[2025-06-05] cmake 崩溃 (STATUS_STACK_BUFFER_OVERRION) → 路径中文 "下载的" 导致，subst J: 解决
[2025-06-05] GitHub FetchContent 不可达 → 改用本地 JUCE add_subdirectory()

## Phase 1: 音频链
[2025-06-05] JUCE 8 juce_add_plugin SOURCES 参数无效 → 改用 target_sources() 添加源文件
[2025-06-05] GrainVoice 独立实现，未用 juce::Synthesiser → 更精确 grain 控制

## Phase 2: 排序引擎
[2025-06-05] 排序算法静态函数链接失败 → .cpp 中缺 SortingAlgorithms:: 前缀
[2025-06-05] QuickSort 用 std::stack 迭代 → 避免递归栈溢出

## Phase 3: UI
[2025-06-05] LookAndFeel_V4 → JUCE 8 用 LookAndFeel_V3
[2025-06-05] browseForFileToOpen 被 #if JUCE_MODAL_LOOPS_PERMITTED 排除 → 改用 launchAsync()
[2025-06-05] Font 构造函数 → JUCE 8 要求 Font(FontOptions())

## Phase 4: 可视化
[2025-06-05] SortVisualizer 需 #include PluginProcessor.h 不能 forward-declare VoiceState → std::array 需要完整类型
[2025-06-05] inline static const 初始化 constexpr 错误 → 改用 inline static const C-style array

## Phase 5: 暗黑主题
[2025-06-05] PathStrokeType::rounded 是 EndCapStyle 不是 JointStyle → 改用 PathStrokeType::curved
[2025-06-05] setLookAndFeel 需在析构时 setLookAndFeel(nullptr)

## 通用踩坑
| 坑 | 解法 |
|---|---|
| 中文路径 | subst 虚拟盘 |
| JUCE 8 API 变化 | 查源码 + 示例 CMakeLists.txt |
| VST3 无模态对话框 | launchAsync 回调模式 |
| MSVC constexpr array | 用 inline static const 普通数组 |

## Phase 6: 命名/品牌/发布 (2026-06-06)
[2026-06-06] pip cmake 3.30.5 全面崩溃 STATUS_STACK_BUFFER_OVERRUN → pip install cmake --force-reinstall 升级到 4.3.2 解决
[2026-06-06] vcvars64.bat >nul 2>&1 破坏环境变量 → 不得重定向 vcvars64.bat 输出
[2026-06-06] 缺少 rc.exe/mt.exe → PATH 需加 Windows SDK bin 目录 (Kits\10\bin\10.0.26100.0\x64)
[2026-06-06] 中文源文件编码 → MSVC 默认 GBK 解析 UTF-8 源文件导致乱码/崩溃，需 target_compile_options(... PRIVATE /utf-8)
[2026-06-06] Component::paint 不能用 lambda 赋值 (C2659) → 必须用子类 override 虚函数
[2026-06-06] PowerShell -replace 中 `n 不解析 → 用 @'...'@ 多行字符串
[2026-06-06] DialogWindow 异步启动 → 用 LaunchOptions + launchAsync()
[2026-06-06] JUCE 免费版 → 必须 GPLv3 许可 (不是 MIT)
[2026-06-06] VST3 安装包 → ZIP 包含 plugin.vst3/ + moduleinfo.json + README

## 新增通用踩坑
| 坑 | 解法 |
|---|---|
| pip cmake 崩溃 | 升级到最新版 |
| vcvars 重定向 | 绝不重定向 vcvars 输出 |
| SDK bin 不在 PATH | 手动添加 $sdk\bin\$ver\x64 |
| MSVC + UTF-8 中文 | /utf-8 编译选项 |
| lambda paint | 子类 override |
| PowerShell 字符串换行 | heredoc @' '@ |
| JUCE 免费版许可 | GPLv3 |

## Phase 7: DSP 防爆音修复 (2026-08-23)
[2026-08-23] 颗粒尾部硬切爆音（sustain 电平直接砍 0）→ setGrain 预计算 fadeOutLen = min(颗粒长×35%, 用户release)，且 ≥ 8×pitchRate 保证高速播放下平滑，renderNextBlock 按 remaining 乘线性窗
[2026-08-23] 颗粒池纯轮转，高速排序时抢占正在发声的 voice → 电平跳变蜂鸣 → advanceVoice 先扫描空闲槽，全忙才轮转抢占；noteOn 记录旧 envLevel 做 ~2ms 交叉淡出（GrainVoice stealFade*）
[2026-08-23] pitchRate 无钳制（MIDI 127 → ~40x 采样，线性插值混叠严重）→ noteToRate() 钳制 ±24 半音（0.25x–4x）
[2026-08-23] 手写峰值限幅器无 lookahead、瞬时增益钳制 → 密集颗粒流下劈啪/抽吸 → 换 juce::dsp::Limiter (threshold -1dB, release 100ms)，CMake 链接 juce::juce_dsp
[2026-08-23] note-off 释放所有 active 颗粒（误伤其他按住的音符）→ GrainVoice 新增 ownerNote，按音符归属释放
[2026-08-23] velocity 传入 setGrain 但从未使用 → velocityScale 乘进输出电平
[2026-08-23] 构建脚本硬编码 D:\SortSynth（项目已迁至 E:）→ 全部改 %~dp0 位置无关；build_configure.bat 中 vcvars >nul 重定向（已知坑）已移除
[2026-08-23] build/ CMake 缓存指向旧源目录 → 删整个 build/ 重配（juce_build 子目录有独立缓存，只删 CMakeCache.txt 不够）
[2026-08-23] duration 调高后出现电流声：颗粒变长 → 16 voice 池全忙 → 每个排序步进都抢占全电平槽，波形跳变以步进频率周期出现（1ms 步进 = 1kHz 蜂鸣）。修法：池扩到 32；抢占只允许已在尾部淡出的槽（isTailFading，听不见跳变）；全忙则跳过该步音频触发（密度上限）。教训：包络连续 ≠ 无爆音，波形跳变才决定听感
[2026-08-23] 头文件里 getter（公有区）先于常量声明（私有区）使用 GRAIN_POOL_SIZE → C2065；类内常量要么提到 class 开头，要么注意声明顺序
[2026-08-23] 电流声残留：抢占淡出的基准电平错用 envLevel（不含尾部窗增益），被抢的尾部槽实际输出只有 envLevel×window，新颗粒却按全 envLevel 起跳 → 抢占瞬间向上跳变。修法：noteOn 里按 envLevel×当前窗增益记录 stealFadeLevel
[2026-08-23] 尾部抢占阈值 fadeOutLen×0.5 太松（波形跳变幅度 ∝ 阈值）→ 收紧到 ×0.2；边界回退线性插值，采样从线性升级 Catmull-Rom（成像抑制）
[2026-08-23] 高频电流声仍残留：即使抢占基准电平已含窗增益（包络连续），被抢槽的"波形"仍在旧电平处跳到新内容——单槽复用数学上不可能波形连续，且跳变以步进频率周期重复（speed=1ms → 1kHz 蜂鸣，音高随 speed 变化）。终极修法：彻底去掉抢占，只复用完全空闲槽，全忙则跳过该步触发；池 32→48 补偿密度。教训：连续性必须看波形，包络/电平连续只是必要条件

## Phase 8: WebView 界面 (2026-08-23)
[2026-08-23] 设计参考抓取：minimal.audio 403 拒绝 WebFetch → 从 MusicTech 评测页 curl 提取截图 URL，视觉模型分析两张截图确认设计语言（近黑底/深灰面板/紫高亮/白标记），Sound on Sound 文字佐证"灰与紫"
[2026-08-23] JUCE WebView 三连坑：① JUCE_WEB_BROWSER=0 默认关闭 ② JUCE_USE_WIN_WEBVIEW2 默认 0（不开则 withResourceProvider 整个不存在，报"不是成员"）③ 动态加载也需 SDK 头文件 WebView2.h。三个都要显式设置/提供
[2026-08-23] WebView2 SDK 获取：nuget 包就是 zip，curl 直接下 https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/<ver> 解压到 %USERPROFILE%/AppData/Local/PackageManagement/NuGet/Packages/Microsoft.Web.WebView2.<ver>/，JUCE 的 FindWebView2.cmake 自动发现，静态链接 WebView2LoaderStatic.lib（免 DLL 部署）
[2026-08-23] juce_add_binary_data 的 SOURCES 路径要相对 CMakeLists 所在目录算清（Source/CMakeLists 引用 ../Resources/web），路径错了 ninja 报 missing and no known rule
[2026-08-23] BinaryData:: 符号要 #include <BinaryData.h>，链接 binary data target 后 include 路径自动可用
[2026-08-23] Web UI 无头截图验证：chrome --headless --screenshot + --virtual-time-budget=4000（否则入场动画没播完，面板半透明）；emitImage 不生效时存 PNG 再用视觉模型分析
[2026-08-23] WebView2 用户数据目录必须显式指定（temp/SieveWebView2），否则默认写宿主 DAW 目录可能无权限