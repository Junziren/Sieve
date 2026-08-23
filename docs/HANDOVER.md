# Sieve 项目交接文档

> 更新日期：2026-08-23
> 仓库：https://github.com/Junziren/Sieve（私有）
> 工作目录：`E:\Sieve Source Code`

---

## 一、项目概况

**Sieve** 是一款基于排序算法的粒子（granular）合成器插件：把音频切片成颗粒，MIDI 触发排序算法将打乱的颗粒排回原序，每次比较/交换触发一个颗粒播放。JUCE 8 / C++20 / CMake + Ninja/Xcode / MSVC 2022 + Apple Clang / Windows x64 + macOS arm64/x86_64。

- **技术栈**：JUCE 8.0.12（本地 `JUCE/` 目录，117MB，不入 git）、CMake 4.3.2、Ninja/Xcode、WebView2/WKWebView（插件 UI）
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

### 2. WebView2 插件界面（Current 风格保留，已建立 native bridge）
- 设计语言来自 Minimal Audio **Current** 实机截图分析（近黑底 / 深灰面板 / 紫色高亮 / 白色播放头标记），Sieve 青色留给数据状态
- 1100×660 固定窗口，`setResizable(false,false)`
- 资源全部内嵌（`juce_add_binary_data(SieveWebUI)`，离线可用，无外部字体/CDN）
- WebView2 **静态链接** WebView2LoaderStatic.lib（SDK 已下载到
  `%USERPROFILE%\AppData\Local\PackageManagement\NuGet\Packages\Microsoft.Web.WebView2.1.0.3485.44\`，
  由 JUCE 的 FindWebView2.cmake 自动发现）——部署无需带 DLL，但终端用户机器需有 WebView2 运行时（Win11 默认自带）
- **控件集已与插件 1:1 对齐**（范围/默认值逐一对照 `createParameterLayout`）：
  算法下拉（9 种+复杂度角标）、切片数下拉（8 档）、Speed/Duration/ADSR/Gain/Pan 八个旋钮、LOAD/TURBO 按钮、静态采样总览、4 行排序状态、底部状态条（`x/4 voices :: Vn X% :: N grains` 格式）、About 彩蛋（点标题）
- 所有参数和主要动作都有悬停说明；About 已加入 `openFAD` 家族标识
- WebView 通过 JUCE native function 接收 `uiReady`、参数手势和 LOAD；参数写入 APVTS，Turbo 写入 `performanceMode`
- 波形区支持 WAV/AIFF/FLAC/OGG/MP3 拖放：原生编辑器接收文件路径，WebView 的 `FileReader` 通过 `loadFileData` bridge 传递标准 Base64；用户已在安装副本实测拖拽加载成功
- Processor 以约 15Hz 的固定容量 `UiFrame` 快照推送真实 voice 排序状态、静态波形总览和播放位置；前端不再生成排序动画或伪造播放头

### 3. 环境与工程
- git 仓库 + GitHub 私有远程（main 分支，全部已推送）
- 三个构建脚本修复为位置无关（`%~dp0`），移除 vcvars 重定向坑
- `?static` URL 参数禁用入场动画（供无头截图确定性验证）
- GitHub Actions Apple release workflow 已加入：固定 JUCE 8.0.12，在 macOS arm64 和 Intel runner 上构建 VST3/AU/Standalone/AUv3，并上传带 SHA-256 的归档

---

## 三、未完成（按优先级）

### 1. UI ↔ DSP 接线（首版已完成，部分宿主行为待验收）
- 参数手势使用 `begin/change/end` 写入 APVTS；原生状态以约 15Hz 回传，宿主自动化和工程恢复可覆盖前端显示
- LOAD 使用编辑器侧异步 `FileChooser`，加载期间调用 `suspendProcessing()`，随后推送真实波形总览
- 拖放加载已接入并由用户实测成功；标准 Base64 数据先在 native bridge 解码，再复用同一套 DSP 文件加载路径
- 可视化使用固定容量 `UiFrame` SPSC 快照：4 个 voice 的 `currentValues`、进度、暂停/完成状态和真实采样位置
- **仍待验收**：DAW 中旋钮实际听感、工程保存/恢复、宿主自动化、编辑器反复开关和异常 WebView 生命周期

### 2. 版本与发布
- `CMakeLists.txt` 仍为 `VERSION 1.0.0`（UI 已同步为真实版本）——接线完成后再升版本号
- 未打 release tag / 未重新打 zip（`Sieve_v1.0.0.zip` 还是旧发布版）
- 已新增 `docs/BUILD_AND_RELEASE.md`、`package_sieve.ps1`、`LICENSE.md` 和 `THIRD_PARTY_LICENSES.md`；发布前仍需把完整 AGPLv3 文本纳入 release archive

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

**系统已安装**（`C:\Program Files\Common Files\VST3\Sieve.vst3`）：当前 Release WebView 版，来源为
`build\Source\Sieve_artefacts\Release\VST3\Sieve.vst3`。源文件与安装副本的 VST3 binary SHA-256 均为
`77E075485F40D65B51F49D173982C1A4E7ED0B2047CB6EE221BCE98467C9786B`。

**最近一次实测**：2026-08-23，用户在安装副本中将音频文件拖入波形区，成功加载；`DROP AUDIO FILE` 提示在加载后隐藏。

**Apple CI 验证**：GitHub Actions run `32636028333` 于 2026-08-23 成功完成两套构建。产物已核对包含 VST3、AU、Standalone 和 AUv3 app extension：

- `Sieve-v1.0.0-macos-arm64.zip`：`56d2d0b2532997ce748c8def8ea0f2d28dc04eafdca57eff0f47ebb79a15cc69`
- `Sieve-v1.0.0-macos-x86_64.zip`：`7b3d035d38be032cb95b3375a698f906aa5de67822c6cf834b619428df1c8e2c`

这两份是未使用发行证书签名/公证的验证归档，尚未代表 macOS DAW、AUv3 宿主或 iPhone/iPad 真机验收。

**检查 WebView 界面（无需 DAW）**：
```
cd "E:\Sieve Source Code\Resources\web"
python -m http.server 8791 --bind 127.0.0.1
# 浏览器打开 http://127.0.0.1:8791/index.html（加 ?static 禁入场动画）
```
**在 DAW 里试当前 WebView 版**：
确认 DAW 已完全退出后，将 `build\Source\Sieve_artefacts\Release\VST3\Sieve.vst3` 覆盖到
`C:\Program Files\Common Files\VST3\Sieve.vst3`，再启动 DAW 并执行插件重扫。

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
