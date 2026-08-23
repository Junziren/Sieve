cmake_minimum_required(VERSION 3.22)
project(Sieve VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)

add_subdirectory(JUCE ${CMAKE_BINARY_DIR}/juce_build)

add_subdirectory(Source)