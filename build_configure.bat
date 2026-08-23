@echo off
echo === SortSynth VST3 Build Script ===
echo.
call D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: vcvars64.bat failed
    pause
    exit /b 1
)

REM Use J: drive to avoid Unicode path issues
subst J: "D:\???" >nul 2>&1

cd /d J:\sortsynth_-algorithmic-granular-synthesizer

echo.
echo === Configuring CMake ===
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 exit /b 1

echo.
echo === Building Release ===
cmake --build build --config Release
if %ERRORLEVEL% neq 0 exit /b 1

echo.
echo === Copying VST3 ===
xcopy /Y /E "build\Source\SortSynth_artefacts\Release\VST3\SortSynth.vst3" "SortSynth.vst3\"
echo.
echo === Build complete! ===
echo VST3: SortSynth.vst3
echo.
pause
