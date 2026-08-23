# SortSynth JUCE/C++ VST3 — Implementation Plan v1.0

## Date: 2025-06-05
## Status: Planned, awaiting execution

---

## Lessons Learned from Max/M4L Attempt (DO NOT REPEAT)

| # | Mistake | JUCE Fix |
|---|---------|----------|
| 1 | Hand-generating .maxpat JSON — extremely fragile, invisible errors | Use JUCE Projucer or CMake `juce_add_plugin()` — compiler catches everything |
| 2 | Coding JS in PowerShell heredocs — quote escaping hell | Write C++ in .cpp files with proper IDE. No string escaping. |
| 3 | Assuming Max auto-detects outlets/connections | C++ function signatures are compiler-enforced. No guessing. |
| 4 | Relative paths to sub-folders breaking silently | CMake targets with explicit include paths. No magic strings. |
| 5 | Testing only by "open in Max and see errors" | pluginval runs in CI. Every commit verified. |
| 6 | Building full UI before verifying audio chain | Test buffer→slice→playback FIRST with no UI, then add controls |
| 7 | Version-specific API (Max 9 @interp gone) | Pin JUCE version via FetchContent GIT_TAG. No surprises. |
| 8 | Debugging via console spam (post()) | JUCE Logger + breakpoints + pluginval structured output |

---

## Architecture Decisions

- **Language**: C++20
- **Build**: CMake + juce_add_plugin()
- **Format**: VST3 only (Win/Mac)
- **UI Controls**: foleys_gui_magic (XML declarative + CSS)
- **UI Theme**: MELT LookAndFeel (dark)
- **Bar Chart**: Custom juce::OpenGLRenderer
- **Animation**: Glow pulse + smooth transitions
- **Testing**: pluginval --strict
- **Scope**: MVP — load → sort → play → visualize

## File Structure

```
SortSynth/
├── CMakeLists.txt
├── pluginval/
│   └── config.json
├── Source/
│   ├── CMakeLists.txt
│   ├── PluginProcessor.h/cpp
│   ├── PluginEditor.h/cpp
│   ├── GrainEngine.h/cpp
│   ├── SortingAlgorithms.h/cpp
│   ├── GrainVoice.h/cpp
│   ├── SortVisualizer.h/cpp
│   └── Theme.h/cpp
├── Resources/
│   ├── sortsynth_gui.xml
│   └── sortsynth_dark.css
└── modules/
    ├── juce/              # JUCE (FetchContent GIT_TAG)
    ├── foleys_gui_magic/  # Declarative GUI
    └── MELT/              # Modern LookAndFeel
```

## Critical Implementation Order (Follow THIS)

Phase 0: Skeleton
  1. CMakeLists.txt with juce_add_plugin(), empty Processor/Editor
  2. Verify compiles → produces .vst3
  3. pluginval runs (all tests fail but structure exists)

Phase 1: Audio Chain (NO UI)
  4. GrainEngine: loadFile() from binary resource (embedded test tone)
  5. GrainEngine: sliceBuffer() verified with assertions
  6. GrainVoice: getNextSample() verified
  7. SortingAlgorithms: bubble only, verify steps
  8. PluginProcessor::processBlock: single grain playback → AUDIO OUTPUT
  9. VERIFY: pluginval passes process-audio tests

Phase 2: Sorting + Playback
  10. All 9 algorithms
  11. Timer-driven sort stepping
  12. Play grain on each step → polyphonic output
  13. APVTS parameters: speed, sliceCount, grainDuration, algorithm

Phase 3: UI (foleys_gui_magic)
  14. PluginEditor loads sortsynth_gui.xml
  15. Wire up: file load button, algorithm selector, speed dial, slice menu
  16. MELT theme applied
  17. CSS styling for dark neon aesthetic

Phase 4: Visualization (OpenGL)
  18. SortVisualizer OpenGL component
  19. Bar chart rendering (height by originalIndex)
  20. Compare glow / Swap flash animations
  21. Smooth height transitions

Phase 5: Polish + Test
  22. Edge cases (empty buffer, 512 grains turbo, rapid algo switching)
  23. pluginval --strict full pass
  24. Performance profiling (CPU at 512 grains × 4 voices)

---

## 补充计划 v1.1

### 1. 技术层：线程安全与性能优化

#### 1.1 音频线程隔离

```
┌─────────────────┐    无锁队列     ┌─────────────────┐
│ 排序步进线程     │ ─────────────→ │ 音频线程         │
│ (juce::ThreadPool)│   step indices │ (processBlock)   │
│ 预计算排序步骤   │                │ 仅播放 + 读参数  │
└─────────────────┘                └─────────────────┘
        │                                  │
        │         环形缓冲区               │
        └──────────────┬──────────────────┘
                       ▼
              ┌─────────────────┐
              │ OpenGL 渲染线程  │
              │ 仅接收增量状态   │
              └─────────────────┘
```

**线程职责严格划分**：

| 线程 | 职责 | 禁止操作 |
|---|---|---|
| 音频线程 (processBlock) | 颗粒播放、参数原子读取 | 内存分配、锁、文件IO、日志 |
| 排序线程 (ThreadPool) | 预计算 SortStep 队列 | 直接操作音频 buffer |
| OpenGL 线程 | 渲染柱状图动画 | 阻塞等待音频线程 |
| 消息线程 (UI) | 控件交互、文件加载 | 长时间计算 |

**排序算法优化**：
- Bubble/Shaker：Early exit 检测——记录最后一次交换位置，后续已排序区间跳过
- sliceCount ≥ 256 时自动降级为 Quick/Shell/Merge（n log n 算法）
- APVTS 新增 `performanceMode` 参数（Normal / Turbo），Turbo 模式跳过 compare 步骤音频触发

#### 1.2 OpenGL 可视化性能

- GLSL shader 实现 Glow（高斯模糊采样邻域像素）、Swap flash（时间 uniform 变量控制红色通道脉冲）
- VBO/VAO 在 `newOpenGLContextCreated()` 中一次性分配，`renderOpenGL()` 仅更新 `glBufferSubData`
- `juce::OpenGLContext::setContinuousRepainting(false)` — 仅 `triggerRepaint()` 时渲染
- 渲染帧率限制：定时器 16ms（≈60fps），排序完成或无变化时停止触发

#### 1.3 颗粒引擎优化

```cpp
// 对象池复用，避免 new/delete 抖动
class GrainVoicePool {
    std::vector<std::unique_ptr<GrainVoice>> pool;
    size_t activeCount = 0;
public:
    GrainVoice* acquire();  // 从池中取空闲实例
    void release(GrainVoice*);  // 标记闲置，不 delete
};

// 音频加载限制
static constexpr size_t MAX_FILE_BYTES = 100 * 1024 * 1024; // 100MB
// 超限 → AlertWindow + 降级为前 100MB 切片
```

### 2. 流程层：强化阶段验证

#### 2.1 量化验证标准

| 阶段 | 验证标准 | 通过条件 |
|---|---|---|
| Phase 0 | 骨架编译 | CMake 无警告，生成 .vst3 |
| Phase 1 | audio 可用 | pluginval 音频测试通过率 100%；SNR ≥ 96dB；切片位置断言全通过 |
| Phase 2 | 排序正确 | 9 种算法输出全排序；512 grains 单声道 CPU ≤ 10% |
| Phase 3 | UI 可用 | foleys XML 加载无报错；所有控件绑定 APVTS |
| Phase 4 | 可视化 | OpenGL 60fps 稳定；柱状图高度 100% 准确 |
| Phase 5 | 全项通过 | pluginval --strict 全绿；512×4 voices CPU ≤ 25% @48kHz；边缘场景无崩溃 |

#### 2.2 依赖版本固化

```cmake
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(
    foleys_gui_magic
    GIT_REPOSITORY https://github.com/ffAudio/foleys_gui_magic.git
    GIT_TAG        v1.4.0
)
# MELT: header-only, git submodule @ commit a3f2b1e

# 本地缓存（网络故障回退）
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.deps_cache")
```

#### 2.3 测试体系

- **单元测试**：`Source/Tests/` 目录，基于 `juce::UnitTest`，覆盖 GrainEngine 切片正确性、SortingAlgorithms 排序准确性
- **性能监控**：CI 中加入 `pluginval --perf` 模式，CPU 超标 → 阻断构建
- **兼容性矩阵**：GitHub Actions 矩阵测试 Win10/11 + macOS 12/13/14、Ableton 12 / Logic 11 / Cubase 13

### 3. 开发效率层

#### 3.1 边缘场景提前处理

```cpp
// GrainEngine 防御性代码
enum class LoadResult { OK, FileNotFound, UnsupportedFormat, TooLarge, SampleRateMismatch };
LoadResult loadFile(const juce::File& f) {
    if (!f.exists()) return LoadResult::FileNotFound;
    if (f.getSize() > MAX_FILE_BYTES) return LoadResult::TooLarge;
    // ... format check ...
    return LoadResult::OK;
}
```

APVTS 参数范围内置约束：`NormalisableRange<float>(0.1f, 10.0f, 0.01f)` 自动钳位

#### 3.2 模块化与调试

```
Source/
├── Core/           # PluginProcessor, GrainEngine, SortingAlgorithms
├── UI/             # PluginEditor, SortVisualizer, Theme
├── Utils/          # LockFreeQueue, AudioMath, Logger
└── Tests/          # 单元测试
```

- **XML/CSS 实时重载**：`juce::FileWatcher` 监听 Resources/ 目录，变化时自动 `reloadGUI()`，无需重启宿主
- **调试面板**（Ctrl+Shift+D 触发）：显示活跃 voice 数、当前步骤索引、CPU 占用、FPS

### 4. 可维护性层

- `.clang-format`：基于 LLVM 风格 + 4 空格缩进 + 120 列宽
- Doxygen 注释标记线程安全要求：
  ```cpp
  /** @threadsafe 仅音频线程调用 */
  float getNextSample();
  /** @threadsafe 任意线程，原子操作 */
  void setActiveIndices(const std::pair<int,int>&);
  ```
- 版本 Tag：`v0.1-skeleton` → `v0.2-audio` → `v0.3-sort` → `v0.4-ui` → `v0.5-viz` → `v1.0-mvp`
- `docs/PITFALLS.md`：记录每个阶段的关键问题和解决方案
