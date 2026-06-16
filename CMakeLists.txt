# ESP-IDF v5.4 Project — TK015F4000 + ESP32-S3
cmake_minimum_required(VERSION 3.16)
# Skip CMake compiler sanity checks (GCC's internal 'as' search broken with MSYS2 xtensa toolchain)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tk015f4000_esp32s3)
