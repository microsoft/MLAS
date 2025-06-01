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
project(MLAS C CXX)


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
  if(WIN32)
    set(CMAKE_CXX_STANDARD 23)
  else()
    set(CMAKE_CXX_STANDARD 20)
  endif()
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
option(MLAS_NO_ONNXRUNTIME "Disable ONNX Runtime related code as much as possible" OFF)
option(MLAS_ENABLE_WEBASSEMBLY_EXCEPTION_CATCHING "Enable this option to turn on exception catching" OFF)

if (MSVC)
  #  Make sure Visual Studio sets __cplusplus macro correctly: https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")

  if (CMAKE_VS_PLATFORM_NAME)
    # Multi-platform generator
    set(onnxruntime_target_platform ${CMAKE_VS_PLATFORM_NAME})
  else()
    set(onnxruntime_target_platform ${CMAKE_SYSTEM_PROCESSOR})
  endif()
  if (onnxruntime_target_platform STREQUAL "ARM64")
    set(onnxruntime_target_platform "ARM64")
    enable_language(ASM_MARMASM)
  elseif (onnxruntime_target_platform STREQUAL "ARM64EC")
    enable_language(ASM_MARMASM)
  elseif (onnxruntime_target_platform STREQUAL "ARM" OR CMAKE_GENERATOR MATCHES "ARM")
    set(onnxruntime_target_platform "ARM")
    enable_language(ASM_MARMASM)
  elseif (onnxruntime_target_platform STREQUAL "x64" OR onnxruntime_target_platform STREQUAL "x86_64" OR onnxruntime_target_platform STREQUAL "AMD64" OR CMAKE_GENERATOR MATCHES "Win64")
    set(onnxruntime_target_platform "x64")
    enable_language(ASM_MASM)
  elseif (onnxruntime_target_platform STREQUAL "Win32" OR onnxruntime_target_platform STREQUAL "x86" OR onnxruntime_target_platform STREQUAL "i386" OR onnxruntime_target_platform STREQUAL "i686")
    set(onnxruntime_target_platform "x86")
    enable_language(ASM_MASM)
    message("Enabling SAFESEH for x86 build")
    set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /safeseh")
  else()
    message(FATAL_ERROR "Unknown CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
else()
  set(onnxruntime_target_platform ${CMAKE_SYSTEM_PROCESSOR})
  enable_language(ASM)
endif()

function(onnxruntime_configure_target target_name)
    target_compile_options(${target_name} PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/MP>
        $<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C>>:/MP>
    )
    if(WIN32)
        target_compile_options(${target_name} PRIVATE  "$<$<COMPILE_LANGUAGE:ASM_MASM>:/Zi>")        
    endif()
endfunction()

function(onnxruntime_add_executable target_name)
  add_executable(${target_name} ${ARGN})
  onnxruntime_configure_target(${target_name})
endfunction()

function(onnxruntime_add_static_library target_name)
  add_library(${target_name} STATIC ${ARGN})
  onnxruntime_configure_target(${target_name})
endfunction()

if(MLAS_ENABLE_WEBASSEMBLY_BROWSER_TESTS)
  #The variable cannot be set from cmake command line because otherwise emscripten's toolchain file will override it.
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  if (MLAS_ENABLE_WEBASSEMBLY_SIMD)
    add_compile_options("-msimd128")
  endif()

  # Build WebAssembly with multi-threads support.
  if(MLAS_ENABLE_WEBASSEMBLY_THREADS)
    add_compile_options("-pthread" "-Wno-pthreads-mem-growth")
    add_link_options("-pthread")
  endif()
  if(MLAS_ENABLE_WEBASSEMBLY_EXCEPTION_CATCHING)
    add_link_options("-sDISABLE_EXCEPTION_CATCHING=0")
  else()
    add_link_options("-fwasm-exceptions")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fwasm-exceptions>")
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
