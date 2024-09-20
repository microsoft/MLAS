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

set(CMAKE_C_STANDARD 99)
include(CheckCXXCompilerFlag)
include(CheckLanguage)
include(CMakeDependentOption)
include(FetchContent)
include(CheckFunctionExists)
include(GNUInstallDirs) 

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "Build type not set - using RelWithDebInfo")
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose build type: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

find_package(Threads)

set(ONNXRUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ONNXRUNTIME_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

include(cmake/external_deps.cmake)
include_directories(${eigen_SOURCE_DIR})

add_compile_options(-DNSYNC_ATOMIC_CPP11 -DDISABLE_ABSEIL)
set(ONNXRUNTIME_MLAS_LIBS onnxruntime_mlas)
add_subdirectory(src)
if(TARGET onnxruntime_mlas_arm64)
   list(APPEND ONNXRUNTIME_MLAS_LIBS onnxruntime_mlas_arm64)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  add_subdirectory(tests)
endif()