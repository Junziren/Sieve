# Sieve 项目交接文档

> 更新日期：2026-08-23
> 仓库：https://github.com/Junziren/Sieve（私有）
> 工作目录：`E:\Sieve Source Code`

---

## 一、项目概况

**Sieve** 是一款基于排序算法的粒子（granular）合成器 VST3 插件：把音频切片成颗粒，MIDI 触发排序算法将打乱的颗粒排回原序，每次比较/交换触发一个颗粒播放。JUCE 8 / C++20 / CMake + Ninja / MSVC 2022 / Windows x64。

- **技术栈**：JUCE 8.0.6（本地 `JUCE/` 目录，117MB，不入 git）、CMake 4.3.2、Ninja、WebView2（插件 UI）
- **构建**：`build_sieve.bat`（配置+编译，位置无关）或手动：
  ```
  cmd /c "call D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build"
  ```
- **产物**：`build\Source\Sieve_artefacts\Release\VST3\Sieve.vst3`
- **踩坑记录**：`docs/PITFALLS.md`（Phase 7 DSP、Phase 8 WebView，含所有环境坑）

### 关键目录结构
```
Source/Core/          GrainEngine / GrainVoice / SortingAlgorithms（DSP 核心）
Source/PluginProcessor.*  音频主逻辑 + APVTS 参数
Source/PluginEditor.*     WebView 宿主（当前）——旧 JUCE UI 组件仍在编译但未使用
Source/UI/            旧 JUCE UI（SortVisualizer/WaveformDisplay/SortSynthLAF，回退用）
Resources/web/        WebView 界面三件套 index.html / app.css / app.js
E:\Sieve-restore\     旧提交 3cef388 的 git worktree（DSP 验收版的独立构建副本，可删）
```

### 提交历史（本阶段）
| 提交 | 内容 |
|---|---|
| `00ced1b` | 基线：发布的 v1.0.0 原样快照 |
| `2497dc9` | DSP 修复①：颗粒尾部自动淡出、池空闲优先、音高钳制 ±24 半音、juce::dsp::Limiter、note-off 按音符归属 + velocity 生效 |
| `84d678f` | DSP 修复②：duration 相关电流声（池扩容、限抢占） |
| `82703a9` | DSP 修复③：抢占淡出基准电平 bug、Catmull-Rom 插值 |
| `3cef388` | DSP 修复④（**用户验收满意**）：彻底取消抢占，只用空闲槽，池 48，全忙跳过该步触发 |
| `ff7919f` | WebView UI 初版（Current 风格，未连 DSP） |
| `1ad6ffc` | UI 控件集与插件 1:1 对齐（删预设 chip/假导航/输出表，补状态条/About 彩蛋） |
| `0ce243e` | 切片数改回下拉（8 档：4/8/16/32/64/128/256/512，与原版一致） |

---

## 二、已完成

### 1. DSP 音质修复（已验收 ✅）
用户确认满意。四轮修复消除了所有已知的爆音/电流声：
- 颗粒尾部按 `min(颗粒长×35%, 用户release)` 线性淡出，≥8×pitchRate 源样本
- 颗粒池（48 个 GrainVoice）**只复用完全空闲槽**，饱和时跳过该步音频触发（密度上限）
- 移调钳制 ±24 半音（noteToRate）；采样 Catmull-Rom 插值（边缘 2 样本回退线性）
- juce::dsp::Limiter（-1dB / 100ms）替换手写峰值钳制
- note-off 只释放所属音符的颗粒；MIDI velocity 生效
- 详见 PITFALLS.md Phase 7，其中记录了核心教训：**包络连续 ≠ 无爆音，波形连续才是判据**

### 2. WebView2 插件界面（视觉完成，未连 DSP ⚠️）
- 设计语言来自 Minimal Audio **Current** 实机截图分析（近黑底 / 深灰面板 / 紫色高亮 / 白色播放头标记），Sieve 青色留给数据状态
- 1100×660 固定窗口，`setResizable(false,false)`
- 资源全部内嵌（`juce_add_binary_data(SieveWebUI)`，离线可用，无外部字体/CDN）
- WebView2 **静态链接** WebView2LoaderStatic.lib（SDK 已下载到
  `%USERPROFILE%\AppData\Local\PackageManagement\NuGet\Packages\Microsoft.Web.WebView2.1.0.3485.44\`，
  由 JUCE 的 FindWebView2.cmake 自动发现）——部署无需带 DLL，但终端用户机器需有 WebView2 运行时（Win11 默认自带）
- **控件集已与插件 1:1 对齐**（范围/默认值逐一对照 `createParameterLayout`）：
  算法下拉（9 种+复杂度角标）、切片数下拉（8 档）、Speed/Duration/ADSR/Gain/Pan 八个旋钮（可拖动/滚轮/双击复位，纯本地状态）、LOAD/SHUFFLE/TURBO 按钮、波形占位区（拖放提示+白色播放头动画）、4 行排序可视化（V1 演示冒泡动画，V2-V4 静默）、底部状态条（`x/4 voices :: Vn X% :: N grains` 格式）、About 彩蛋（点标题）
- 右上角有黄色 "UI PREVIEW · DSP NOT CONNECTED" 徽标标识未接线状态

### 3. 环境与工程
- git 仓库 + GitHub 私有远程（main 分支，全部已推送）
- 三个构建脚本修复为位置无关（`%~dp0`），移除 vcvars 重定向坑
- `?static` URL 参数禁用入场动画（供无头截图确定性验证）

---

## 三、未完成（按优先级）

### 1. UI ↔ DSP 接线（核心待办）
界面所有控件目前是**纯 JS 本地状态，不与 APVTS 通信**。接线方案（JUCE 8 标准路径，参考 `JUCE/examples/Plugins/WebViewPluginDemo.h`）：
- **参数绑定**：`juce::WebSliderRelay` / `WebComboBoxRelay` / `WebToggleButtonRelay` + 对应 `Web*ParameterAttachment`；JS 侧用 `juce` 全局对象（nativeIntegrationEnabled 时注入）的 slider/combo/toggle 控件替代现有自制 Knob
- **动作按钮**（LOAD/SHUFFLE）：`withNativeFunction()` 注册回调（load 需在编辑器侧弹文件对话框，注意 VST3 内必须 launchAsync）
- **可视化数据**：处理器 → 编辑器 → `webView->evaluateJavascript()` 或 `withEventListener` 推送：波形缩略数据（loadFile 后一次）、播放头位置、4 个 voice 的 `currentValues` 数组（约 30fps，原子快照）
- **文件拖放**：`FileDragAndDropTarget` 在编辑器层实现后转发给 `loadFile`
- 接线完成后：移除 "UI PREVIEW" 徽标；8 个旋钮和两个下拉改为 relay 控件（自制 Knob 可保留外观、只换事件层）
- **验收标准**：DAW 中旋动任一控件音效实时变化；保存/恢复工程参数完整往返；可视化反映真实排序状态

### 2. 版本与发布
- `CMakeLists.txt` 仍为 `VERSION 1.0.0`（UI 里 v1.1.0 是占位文字）——接线完成后升版本号
- 未打 release tag / 未重新打 zip（`Sieve_v1.0.0.zip` 还是旧发布版）
- README 的安装/界面章节仍描述旧版 JUCE 界面

### 3. 遗留工程项
- **旧 JUCE UI 文件**（`Source/UI/*`）仍编译未使用——作为回退保留，接线验收后可删
- `Source/Tests/SortingAlgorithmTests.h` 从未接入构建（juce::UnitTest 无 runner）
- pluginval 校验未跑（`pluginval/config.json` 存在，本机无 pluginval 可执行文件）
- `E:\Sieve-restore\` worktree 用完可删（注意先删其中的 JUCE junction 再 `git worktree remove`，避免递归删除主目录的 JUCE）

### 4. 音质深水区（明确搁置，非 bug）
- 高移调下重采样混叠：需 2×/4× 过采样（juce::dsp::Oversampling）才能根治
- Pitch Lock 模式（忽略 MIDI 音高，恒 1×）——对旋律素材友好度大增的候选特性

---

## 四、当前系统状态与验证方法

**系统已安装**（`C:\Program Files\Common Files\VST3\Sieve.vst3`）：DSP 验收版（`3cef388`，原 JUCE 界面，参数全可控）。**WebView 版未安装**，按用户要求"先检查再装"。

**检查 WebView 界面（无需 DAW）**：
```
cd "E:\Sieve Source Code\Resources\web"
python -m http.server 8791 --bind 127.0.0.1
# 浏览器打开 http://127.0.0.1:8791/index.html（加 ?static 禁入场动画）
```
**在 DAW 里试 WebView 版（临时替换，看完恢复）**：
把 `build\Source\Sieve_artefacts\Release\VST3\Sieve.vst3` 拷入 VST3 目录，看完后从 `E:\Sieve-restore\build\...\Sieve.vst3` 拷回验收版。

**截图验证**（无头 Chrome + 视觉模型）：
```
chrome --headless --screenshot=out.png --window-size=1100,660 --hide-scrollbars "http://127.0.0.1:8791/index.html?static"
```

---

## 五、重要约定

1. **任何新版本必须先经用户检查确认，才允许安装到系统 VST3 目录**（2026-08-23 教训，已记录）
2. DAW 打开状态下替换插件文件可能失败（文件占用）——换版本前先确认 DAW 已关闭
3. 本机 GitHub 连接不稳定，push 失败重试几次即可；构建缓存指向旧路径时删整个 `build/` 重配
4. 所有踩坑先查 `docs/PITFALLS.md`，新坑记得补记
