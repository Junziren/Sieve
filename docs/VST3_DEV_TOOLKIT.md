# VST3 插件开发工具包

> 基于 Sieve 项目的完整开发经验：环境配置、最佳实践、踩坑全集

---

## 一、开发环境

### 1.1 必备组件

| 组件 | 版本 | 用途 |
|------|------|------|
| **Visual Studio Build Tools** | 2022 (17.14) | C++ 编译器 (MSVC 19.44) |
| **Windows SDK** | 10.0.26100 | 系统头文件 + rc.exe/mt.exe |
| **CMake** | ≥ 3.22 (推荐 4.3+) | 构建系统 |
| **Ninja** | 任意 | 比 MSBuild 快 5–10 倍 |
| **JUCE** | 8.0.x | 音频插件框架 |
| **PowerShell** | 7.x | 脚本/自动化 |

### 1.2 环境变量

每次构建前必须设置（vcvars64.bat 自动完成，手动备用）：

```powershell
# MSVC
$msvc = "D:\VS2022BuildTools\VC\Tools\MSVC\14.44.35207"
# Windows SDK  
$sdk  = "C:\Program Files (x86)\Windows Kits\10"
$ver  = "10.0.26100.0"

$env:PATH    = "$msvc\bin\Hostx64\x64;$sdk\bin\$ver\x64;$env:PATH"
$env:INCLUDE = "$msvc\include;$sdk\Include\$ver\ucrt;$sdk\Include\$ver\um;$sdk\Include\$ver\shared"
$env:LIB     = "$msvc\lib\x64;$sdk\Lib\$ver\ucrt\x64;$sdk\Lib\$ver\um\x64"
```

> **关键**：`$sdk\bin\$ver\x64` 必须加入 PATH —— 缺少它会导致 `rc.exe` 和 `mt.exe` 找不到，链接失败。

---

## 二、项目骨架

### 2.1 最小 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)

add_subdirectory("D:/path/to/JUCE" ${CMAKE_BINARY_DIR}/juce_build)
add_subdirectory(Source)
```

### 2.2 Source/CMakeLists.txt 标准模板

```cmake
juce_add_plugin(MyPlugin
    COMPANY_NAME           "公司名"
    PLUGIN_MANUFACTURER_CODE 4CHR    # 恰好 4 个大写字符
    PLUGIN_CODE            4CHR    # 恰好 4 个字符
    FORMATS                VST3
    PRODUCT_NAME           "显示名"
    VERSION                "1.0.0"
    BUNDLE_ID              "com.company.plugin"
    IS_SYNTH               TRUE
    NEEDS_MIDI_INPUT       TRUE
)

target_sources(MyPlugin PRIVATE ...)

# ⚠️ 中文源文件必须加这个
target_compile_options(MyPlugin PRIVATE /utf-8)

target_compile_definitions(MyPlugin PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
)

target_include_directories(MyPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(MyPlugin
    PRIVATE juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
)
```

### 2.3 目录结构约定

```
Project/
├── CMakeLists.txt
├── README.md
├── docs/
│   └── PITFALLS.md          # 开发日志
├── Source/
│   ├── CMakeLists.txt
│   ├── PluginProcessor.h/cpp
│   ├── PluginEditor.h/cpp
│   ├── Core/                # 音频引擎
│   ├── UI/                  # 界面组件
│   ├── Utils/               # 工具类
│   └── Tests/               # 单元测试
└── Resources/               # 资源文件
```

---

## 三、构建流程

### 3.1 可靠构建脚本 (.bat)

```batch
@echo off
call D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat

cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 exit /b 1

cmake --build build --config Release
```

> ⚠️ **严禁** `call vcvars64.bat >nul 2>&1` —— 重定向 vcvars 输出会破坏环境变量，导致 cmake STATUS_STACK_BUFFER_OVERRUN 崩溃。

### 3.2 PowerShell 构建（推荐用于 CI）

```powershell
$msvc = "D:\VS2022BuildTools\VC\Tools\MSVC\14.44.35207"
$sdk  = "C:\Program Files (x86)\Windows Kits\10"
$ver  = "10.0.26100.0"

$env:PATH    = "$msvc\bin\Hostx64\x64;$sdk\bin\$ver\x64;$env:PATH"
$env:INCLUDE = "$msvc\include;$sdk\Include\$ver\ucrt;$sdk\Include\$ver\um;$sdk\Include\$ver\shared"
$env:LIB     = "$msvc\lib\x64;$sdk\Lib\$ver\ucrt\x64;$sdk\Lib\$ver\um\x64"

cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## 四、成功经验 (Best Practices)

### 4.1 音频引擎

**独立 GrainVoice，不用 juce::Synthesiser**

```cpp
class GrainVoice {
    const float* sourceData;   // 指向已加载音频 buffer（只读共享）
    double playbackPos;        // 双精度避免长时间累积误差
    EnvPhase phase;            // 自定义 ADSR 状态机
};
```

- 比 juce::Synthesiser 更精确的颗粒控制
- 16 实例对象池循环复用，避免 new/delete 抖动
- ADSR 包络增量预计算（`recalcRates()`），per-sample 无除法

### 4.2 排序预计算

```cpp
// 激活 voice 时一次性生成全部步骤
std::vector<SortStep> steps = SortingAlgorithms::generateSteps(algo, arr);

// 定时器驱动步进，无实时计算开销
void advanceVoice(int voiceId) {
    if (vs.stepIndex < vs.steps.size()) {
        playGrain(vs.steps[vs.stepIndex]);  // O(1)
        vs.stepIndex++;
    }
}
```

### 4.3 线程隔离

```
MIDI 输入 → SPSC 无锁队列 → processVoiceMessages()
                                   ↓
                            VoiceState[] (原子标记)
                                   ↓
                            advanceVoice() 触发 GrainVoice
```

- 音频线程只读取原子变量，不分配内存
- GUI 通过 APVTS attachment 自动同步，无需手动加锁
- 排序步骤生成在 voice 激活时完成（非音频线程路径）

### 4.4 暂停/恢复机制

```cpp
struct VoiceState {
    bool paused;    // note-off → 保存 currentValues/stepIndex
    bool completed; // 排序完成 → 下次按键生成新的随机初始序列
};

// 松开键：paused = true → advanceVoice 跳过
// 再次按键：恢复 paused voice，从 stepIndex 继续
```

### 4.5 About 对话框

```cpp
// 内部类方式（paint 是虚函数，不能 lambda 赋值）
struct AboutContent : public juce::Component {
    void paint(juce::Graphics& g) override { /* ... */ }
    void mouseDown(const juce::MouseEvent&) override { /* 打开链接 */ }
};

// 异步启动
juce::DialogWindow::LaunchOptions opts;
opts.content.setOwned(new AboutContent(...));
opts.launchAsync();
```

### 4.6 JUCE 8 API 速查

| 旧写法 | JUCE 8 写法 |
|--------|------------|
| `juce::Font(24.0f, juce::Font::bold)` | `juce::Font(juce::FontOptions().withHeight(24.0f).withStyle("Bold"))` |
| `LookAndFeel_V4` | `LookAndFeel_V3` |
| `FileChooser::browseForFileToOpen()` | `FileChooser::launchAsync(openMode, callback)` |
| `PathStrokeType::rounded` | `PathStrokeType::curved` (EndCapStyle vs JointStyle) |
| `setLookAndFeel(&laf)` | 析构时必须 `setLookAndFeel(nullptr)` |

---

## 五、踩坑全集 (Anti-Patterns)

### 🔴 致命级

| # | 问题 | 症状 | 解法 |
|---|------|------|------|
| 1 | **pip cmake 3.30.5 崩溃** | `STATUS_STACK_BUFFER_OVERRUN` (-1073740791) | `pip install cmake --force-reinstall` → 升级到 4.3.2 |
| 2 | **中文路径** (含 `下载的` 等) | cmake 缓冲区溢出 | 避免项目路径含中文；或用 `subst` 虚拟盘映射 |
| 3 | **vcvars64.bat 重定向** | cmake 静默崩溃，无日志 | `call vcvars64.bat` **不加** `>nul 2>&1` |
| 4 | **缺少 rc.exe/mt.exe** | 链接失败 `no such file or directory` | PATH 加 `$sdk\bin\$ver\x64` |

### 🟠 严重级

| # | 问题 | 症状 | 解法 |
|---|------|------|------|
| 5 | **中文字符编码** | 界面显示乱码 | `target_compile_options(... PRIVATE /utf-8)` + 源文件 UTF-8 编码 |
| 6 | **Component::paint lambda** | `error C2659: "=" 作为左操作数` | paint 是虚函数，必须用子类 override，不能 lambda 赋值 |
| 7 | **排序算法静态函数未限定** | 链接失败 `unresolved external` | .cpp 中函数体前必须加 `SortingAlgorithms::` 前缀 |
| 8 | **forward-declare + std::array** | 编译失败 `incomplete type` | 包含完整头文件，不能仅 forward-declare `VoiceState` |

### 🟡 中等级

| # | 问题 | 症状 | 解法 |
|---|------|------|------|
| 9 | **juce_add_plugin SOURCES 参数** | 源文件未被编译 | 弃用 SOURCES 参数，改用 `target_sources()` |
| 10 | **VST3 模态对话框** | `#if JUCE_MODAL_LOOPS_PERMITTED` 排除 | 用 `launchAsync()` 回调模式 |
| 11 | **QuickSort 递归深度** | n>1000 时栈溢出 | 使用 `std::stack<std::pair<int,int>>` 迭代实现 |
| 12 | **inline static const 数组初始化** | constexpr 错误 | 改用 `inline static const` C-style 数组，不用 constexpr |
| 13 | **FetchContent 网络不可达** | GitHub 连接超时 | 改用 `add_subdirectory("本地路径")` |

### 🟢 提醒级

| # | 问题 | 解法 |
|---|------|------|
| 14 | PowerShell `-replace` 中 `` `n `` 不解析 | 用 `@'...'@` 多行字符串 heredoc 替代 |
| 15 | PluginProcessor.h 中 `getName()` | 始终返回插件显示名，与 CMake PRODUCT_NAME 保持一致 |
| 16 | LookAndFeel 生命周期 | 构造时 `setLookAndFeel(&laf)` → 析构时 `setLookAndFeel(nullptr)` |
| 17 | `juce::String` + 中文 | 用 `CharPointer_UTF8("...")` 确保 UTF-8 字面量 |

---

## 六、调试与验证

### 6.1 快速验证清单

```powershell
# 1. 编译器可用？
cmd /c "call vcvars64.bat && cl.exe"

# 2. CMake 配置
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. 仅编译变更文件
ninja -C build

# 4. 安装到 VST3 目录
xcopy /Y /E build\Source\*_artefacts\Release\VST3\*.vst3 "C:\Program Files\Common Files\VST3\"
```

### 6.2 pluginval 配置

```json
{
    "pluginval": {
        "strict": true,
        "skip_gui_tests": false,
        "timeout_ms": 30000
    },
    "tests": {
        "process_audio": true,
        "midi_input": true,
        "state_save_restore": true
    }
}
```

### 6.3 DAW 兼容性测试矩阵

| DAW | 版本 | 测试项 |
|-----|------|--------|
| Ableton Live | 12 | MIDI 输入、参数自动化、预设保存 |
| FL Studio | 21+ | 插件扫描、多实例 |
| Cubase | 13 | VST3 规范兼容性 |
| Reaper | 7 | 通用兼容性参考 |

---

## 七、发布清单

- [ ] VST3 编译通过（Release x64）
- [ ] 安装包 ZIP 包含 `Plugin.vst3/` + `README.md`
- [ ] `moduleinfo.json` 随 VST3 打包
- [ ] 版本号与 CMake/Bundle ID 一致
- [ ] 许可协议明确标注（JUCE 8 开源路径 → **AGPLv3**；商业构建遵循 JUCE 商业许可）
- [ ] About 对话框可访问
- [ ] 无硬编码绝对路径
- [ ] 中文路径已清理或通过 subst 解决

---

> 最后更新：2026-06-06 · 基于 Sieve v1.0.0 开发过程
