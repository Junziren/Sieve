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