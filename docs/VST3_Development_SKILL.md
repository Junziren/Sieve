# VST3 Plugin Development Skill

## Purpose

Guide the development of VST3 audio plugins using JUCE/C++ on Windows. This skill encodes the full collaboration model, environment configuration, build pipeline, coding patterns, and pitfall database derived from the Sieve project.

## Trigger

Use this skill whenever the user asks to:
- Create a new VST3/AU plugin with JUCE
- Modify or build an existing JUCE-based audio plugin
- Debug Windows C++ build issues involving CMake / MSVC / Ninja
- Package or release a VST3 plugin

## Collaboration Model

### When to delegate to sub-agents

Long-running development benefits from keeping the main dialogue focused on decisions, not mechanics.

| Task type | Where | Why |
|-----------|-------|-----|
| Architecture / design discussion | Main dialogue | Needs context and back-and-forth |
| Multi-file code changes | sub-agent (worker) | Bulk edits are mechanical |
| Build + install cycle | sub-agent (worker) | Repetitive and error-prone |
| Long-form document writing | sub-agent (worker) | README, toolkits, etc. |
| Testing / validation | sub-agent (worker) | Can run in parallel |
| Reviewing sub-agent output | Main dialogue | Quality gate |

### Effective sub-agent dispatch

A good sub-agent task should be:
- **End-to-end**: include all steps, don't split into micro-tasks
- **Self-verifying**: ask agent to confirm its own output
- **Disjoint**: no overlapping write scopes with other agents

Example (good):
> Modify PluginProcessor.h and PluginEditor.cpp: rename "Sift" → "Sieve", change subtitle to Chinese, add /utf-8 flag. Build Release, install to VST3 dir. Return changed files and build status.

Example (bad):
> 1. Rename Sift to Sieve. 2. Change subtitle. 3. Add /utf-8. 4. Build. 5. Install.

### Sandbox note

Sub-agents need `require_escalated` permission for commands writing outside the workspace (e.g., installing to `C:\Program Files\Common Files\VST3\`). Always include justification when dispatching.

---

## Environment

### Required components

```
Visual Studio Build Tools 2022  →  D:\VS2022BuildTools
Windows SDK 10.0.26100          →  C:\Program Files (x86)\Windows Kits\10
JUCE 8.x                        →  D:\JUCE (or project-local)
CMake ≥ 3.22 (recommend 4.3+)  →  pip install cmake
Ninja                           →  pip install ninja
```

### Environment setup (PowerShell)

```powershell
$msvc = "D:\VS2022BuildTools\VC\Tools\MSVC\14.44.35207"
$sdk  = "C:\Program Files (x86)\Windows Kits\10"
$ver  = "10.0.26100.0"

$env:PATH    = "$msvc\bin\Hostx64\x64;$sdk\bin\$ver\x64;$env:PATH"
$env:INCLUDE = "$msvc\include;$sdk\Include\$ver\ucrt;$sdk\Include\$ver\um;$sdk\Include\$ver\shared"
$env:LIB     = "$msvc\lib\x64;$sdk\Lib\$ver\ucrt\x64;$sdk\Lib\$ver\um\x64"
```

> `$sdk\bin\$ver\x64` in PATH is **mandatory** — missing this causes `rc.exe`/`mt.exe` not found at link time.

---

## Project Skeleton

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(PluginName VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)

add_subdirectory("D:/JUCE" ${CMAKE_BINARY_DIR}/juce_build)
add_subdirectory(Source)
```

### Source/CMakeLists.txt

```cmake
juce_add_plugin(MyPlugin
    COMPANY_NAME           "Company"
    PLUGIN_MANUFACTURER_CODE XXXX    # exactly 4 uppercase chars
    PLUGIN_CODE            XXXX     # exactly 4 chars
    FORMATS                VST3
    PRODUCT_NAME           "MyPlugin"
    VERSION                "1.0.0"
    BUNDLE_ID              "com.company.myplugin"
    IS_SYNTH               TRUE
    NEEDS_MIDI_INPUT       TRUE
)

target_sources(MyPlugin PRIVATE ...)

# Required for Chinese/Unicode source files
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

### Directory layout

```
Project/
├── CMakeLists.txt
├── README.md
├── Source/
│   ├── CMakeLists.txt
│   ├── PluginProcessor.h/cpp
│   ├── PluginEditor.h/cpp
│   ├── Core/           # audio engine
│   ├── UI/             # visual components
│   ├── Utils/          # helpers
│   └── Tests/          # unit tests
└── docs/
    └── PITFALLS.md     # development log
```

---

## Build Pipeline

### Reliable build (.bat — never redirect vcvars output)

```batch
@echo off
call D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat

cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 exit /b 1

cmake --build build --config Release
```

### PowerShell build (for CI)

```powershell
$env:PATH    = "$msvc\bin\Hostx64\x64;$sdk\bin\$ver\x64;$env:PATH"
$env:INCLUDE = "$msvc\include;$sdk\Include\$ver\ucrt;$sdk\Include\$ver\um;$sdk\Include\$ver\shared"
$env:LIB     = "$msvc\lib\x64;$sdk\Lib\$ver\ucrt\x64;$sdk\Lib\$ver\um\x64"

cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Install

```powershell
Copy-Item -Recurse -Force `
    "build\Source\*_artefacts\Release\VST3\Plugin.vst3" `
    "C:\Program Files\Common Files\VST3\Plugin.vst3"
```

---

## Key Code Patterns

### GrainVoice (don't use juce::Synthesiser)

```cpp
class GrainVoice {
    const float* sourceData = nullptr;  // shared read-only buffer
    double playbackPos = 0.0;           // double for long-duration accuracy
    EnvPhase phase;                     // custom ADSR state machine
    std::atomic<float> pitchRate{1.0f}; // thread-safe parameter reads
    std::atomic<bool>  active{false};
public:
    void setGrain(const Grain&, const float* buf, int len, float rate, float vel);
    void renderNextBlock(juce::AudioBuffer<float>&, int start, int n);
};
```

Object pool: 16 instances, cycled via ring-buffer index. No `new`/`delete` in audio path.

### Sorting as pre-computed step sequences

```cpp
// Generate ALL steps at voice activation time (not in audio callback)
std::vector<SortStep> steps = SortingAlgorithms::generateSteps(algo, arr);

// Timer-driven stepping: O(1) per tick
void advanceVoice(int voiceId) {
    if (vs.stepIndex < vs.steps.size()) {
        playGrain(vs.steps[vs.stepIndex]);
        vs.stepIndex++;
    }
}
```

### Pause/resume (voice state machine)

```cpp
struct VoiceState {
    bool paused = false;     // note-off mid-sort → preserve state
    bool completed = false;  // sort finished → next press reshuffles
};

// Note-off → paused=true (advanceVoice skips)
// Note-on  → if paused voice exists, resume; else new voice
// Sort end → reset + completed=true (no auto-reshuffle)
```

### Thread safety

```
MIDI input → SPSC lock-free queue → processVoiceMessages()
                                          ↓
                                   VoiceState[] (atomic flags)
                                          ↓
                                   advanceVoice() → trigger GrainVoice
```

- Audio thread: only reads atomics, no allocation
- GUI: APVTS attachments auto-sync
- Sort step generation: done at voice activation, outside audio path

### About dialog (subclass Component — never lambda paint)

```cpp
struct AboutContent : public juce::Component {
    void paint(juce::Graphics& g) override { /* render info */ }
    void mouseDown(const juce::MouseEvent&) override { /* open URL */ }
};

// Launch:
juce::DialogWindow::LaunchOptions opts;
opts.content.setOwned(new AboutContent(...));
opts.launchAsync();
```

### JUCE 8 API quick reference

| JUCE 7 / old pattern | JUCE 8 replacement |
|---|---|
| `juce::Font(24.0f, Font::bold)` | `juce::Font(FontOptions().withHeight(24).withStyle("Bold"))` |
| `LookAndFeel_V4` | `LookAndFeel_V3` |
| `browseForFileToOpen()` | `launchAsync(openMode, callback)` |
| `PathStrokeType::rounded` | `PathStrokeType::curved` |
| `setLookAndFeel(&laf)` | Must `setLookAndFeel(nullptr)` in destructor |
| `juce_add_plugin(... SOURCES ...)` | Use `target_sources()` instead |

---

## Pitfall Database (ranked by severity)

### 🔴 Fatal (crashes / won't build)

| # | Symptom | Cause | Fix |
|---|---------|-------|-----|
| F1 | cmake `STATUS_STACK_BUFFER_OVERRUN` | pip cmake 3.30.5 bug | `pip install cmake --force-reinstall` → 4.3+ |
| F2 | cmake silent crash, no log | `vcvars64.bat >nul 2>&1` redirects break env | **Never redirect** vcvars output |
| F3 | Link: `no such file or directory` for rc.exe | Windows SDK bin not in PATH | Add `$sdk\bin\$ver\x64` to PATH |
| F4 | cmake crash with Chinese path | Path like `下载的` overflows buffer | Avoid Chinese in paths; use `subst` as last resort |

### 🟠 Severe (wrong behavior / compile errors)

| # | Symptom | Cause | Fix |
|---|---------|-------|-----|
| S1 | Chinese text shows as garbled | MSVC interprets UTF-8 as GBK (936) | `target_compile_options(... PRIVATE /utf-8)` |
| S2 | `error C2659: "="` on lambda | `Component::paint` is virtual, not assignable | Subclass + override |
| S3 | Unresolved external for static sort functions | Missing `ClassName::` prefix in .cpp | Always qualify static method definitions |
| S4 | Incomplete type error with `std::array<VoiceState>` | Forward-declared incomplete type | `#include` full header |

### 🟡 Moderate (API / compatibility)

| # | Symptom | Cause | Fix |
|---|---------|-------|-----|
| M1 | Sources not compiled | `juce_add_plugin(SOURCES ...)` broken in JUCE 8 | Use `target_sources()` |
| M2 | File dialog doesn't appear | VST3 blocks modal loops | `launchAsync()` callback pattern |
| M3 | Stack overflow with QuickSort > 1000 elements | Recursive depth | Use `std::stack` iterative approach |
| M4 | `constexpr` init error for `inline static const` array | MSVC constexpr limitation | C-style array without constexpr |

### 🟢 Minor (tooling / workflow)

| # | Issue | Fix |
|---|-------|-----|
| W1 | PowerShell `` `n `` in `-replace` not parsed | Use `@"..."@` heredoc |
| W2 | `LookAndFeel` leak on editor close | Always `setLookAndFeel(nullptr)` in destructor |
| W3 | `getName()` mismatch with DAW display | Keep consistent with CMake `PRODUCT_NAME` |
| W4 | JUCE free edition license assumed MIT | **Must be GPLv3** (JUCE dual-license) |

---

## Release Checklist

- [ ] Release build (not Debug) compiled
- [ ] `moduleinfo.json` present in VST3 bundle
- [ ] ZIP package contains `Plugin.vst3/` folder structure + `README.md`
- [ ] Version matches across: CMake → `PRODUCT_NAME` → `getName()` → ZIP filename
- [ ] License correctly declared as GPLv3 (JUCE free edition)
- [ ] About dialog accessible and accurate
- [ ] No hardcoded absolute paths in source
- [ ] Tested in target DAW (Ableton/FL/Cubase/Reaper)