@echo off
echo === Sieve VST3 Build Script ===
echo.

REM PITFALL: never redirect vcvars64.bat output - it corrupts env vars
call D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat
if %ERRORLEVEL% neq 0 (
    echo ERROR: vcvars64.bat failed
    exit /b 1
)

cd /d "%~dp0"

echo === Configuring CMake ===
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 exit /b 1

echo.
echo === Building Release ===
cmake --build build --config Release
if %ERRORLEVEL% neq 0 exit /b 1

echo.
echo === Build complete ===
echo VST3: build\Source\Sieve_artefacts\Release\VST3\Sieve.vst3
