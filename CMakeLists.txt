# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-FileCopyrightText: Copyright 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
# Licensed under the MIT License.

# Minimum CMake required
cmake_minimum_required(VERSION 3.26)

cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

cmake_policy(SET CMP0092 NEW)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0117 NEW)

# Project
project(MLAS C CXX ASM)


include(CheckCXXCompilerFlag)
include(CheckLanguage)
include(CMakeDependentOption)
include(FetchContent)
include(CheckFunctionExists)
include(GNUInstallDirs) 

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()
if(NOT CMAKE_CXX_STANDARD)
  message("Setting C++ standard to 20")
  set(CMAKE_CXX_STANDARD 20)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

find_package(Threads)

set(ONNXRUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ONNXRUNTIME_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
option(MLAS_ENABLE_WEBASSEMBLY_THREADS "Enable this option to create WebAssembly byte codes with multi-threads support" OFF)
option(MLAS_ENABLE_WEBASSEMBLY_BROWSER_TESTS "Build all executables as html files" OFF)
option(MLAS_NO_ONNXRUNTIME "Disable ORT related code" OFF)
option(MLAS_ENABLE_WEBASSEMBLY_EXCEPTION_CATCHING "Enable this option to turn on exception catching" OFF)

if(MLAS_ENABLE_WEBASSEMBLY_BROWSER_TESTS)
  #The variable cannot be set from cmake command line because otherwise emscripten's toolchain file will override it.
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  if(MLAS_ENABLE_WEBASSEMBLY_THREADS)
    add_compile_options("-pthread" "-Wno-pthreads-mem-growth")
    add_link_options("-pthread")
  endif()
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fwasm-exceptions>")
  add_link_options("-fwasm-exceptions")
  if(MLAS_ENABLE_WEBASSEMBLY_EXCEPTION_CATCHING)
    add_link_options("-fwasm-exceptions" "-sNO_DISABLE_EXCEPTION_CATCHING")
  endif()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_link_options("-gsource-map" "-sASSERTIONS=2")
  endif()
endif()

if(NOT TARGET onnxruntime_common)
  # ONNX Runtime already has the dependencies
  include(cmake/external_deps.cmake)
endif()
if(NOT MLAS_NO_ONNXRUNTIME)
  include_directories(${eigen_SOURCE_DIR})
  include_directories(src/ort_include)
endif()

if(MLAS_NO_ONNXRUNTIME)
  add_compile_options(-DBUILD_MLAS_NO_ONNXRUNTIME)
endif()
add_compile_options(-DNSYNC_ATOMIC_CPP11 -DDISABLE_ABSEIL)
set(ONNXRUNTIME_MLAS_LIBS onnxruntime_mlas)
add_subdirectory(src)
if(TARGET onnxruntime_mlas_arm64)
  list(APPEND ONNXRUNTIME_MLAS_LIBS onnxruntime_mlas_arm64)
endif()
if(TARGET onnxruntime_mlas_x86_64)
  list(APPEND ONNXRUNTIME_MLAS_LIBS onnxruntime_mlas_x86_64)
endif()
message("ONNXRUNTIME_MLAS_LIBS: ${ONNXRUNTIME_MLAS_LIBS}")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
