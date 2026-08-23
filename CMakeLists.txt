cmake_minimum_required(VERSION 3.22)
project(Sieve VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)

set(SIEVE_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH
    "Path to the JUCE source tree")

if(NOT EXISTS "${SIEVE_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE was not found. Set SIEVE_JUCE_DIR to a JUCE checkout (8.x is expected).")
endif()

add_subdirectory("${SIEVE_JUCE_DIR}" ${CMAKE_BINARY_DIR}/juce_build)

# WebView2 is a Windows build dependency only. Apple builds use JUCE's WKWebView
# backend and must not require a Windows SDK or a user-local NuGet package.
if(WIN32)
    list(APPEND CMAKE_MODULE_PATH "${SIEVE_JUCE_DIR}/extras/Build/CMake")
    set(JUCE_WEBVIEW2_PACKAGE_LOCATION "" CACHE PATH
        "Directory containing the Microsoft.Web.WebView2 NuGet package")
    find_package(WebView2 REQUIRED)
endif()

add_subdirectory(Source)
