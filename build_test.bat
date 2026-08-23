@echo off
REM Configure-only variant (build_sieve.bat configures and builds)
call D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat
cd /d "%~dp0"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
echo EXIT: %ERRORLEVEL%
