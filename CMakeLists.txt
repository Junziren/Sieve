cmake_minimum_required(VERSION 3.22)
project(Sieve VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)

add_subdirectory(JUCE ${CMAKE_BINARY_DIR}/juce_build)

# WebView2 SDK (headers + static loader) for the WebView plugin GUI
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/extras/Build/CMake")
find_package(WebView2 REQUIRED)

add_subdirectory(Source)