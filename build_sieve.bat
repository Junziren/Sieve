@echo off
call D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat
D:\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe -B D:\SortSynth\build -S D:\SortSynth -G Ninja -DCMAKE_BUILD_TYPE=Release
echo EXIT: %ERRORLEVEL%