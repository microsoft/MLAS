// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// See docs\c_cxx\README.md on generating the Doxygen documentation from this file

/** \mainpage ONNX Runtime
 *
 * ONNX Runtime is a high-performance inference and training graph execution engine for deep learning models.
 *
 * ONNX Runtime's C, C++ APIs offer an easy to use interface to onboard and execute onnx models.
 * - \subpage c_cpp_api "Core C, C++ APIs"
 * - \subpage training_c_cpp_api "Training C, C++ APIs for on-device training"
 *
 * \page c_cpp_api Core C, C++ APIs
 * <h1>C</h1>
 *
 * ::OrtApi - Click here to go to the structure with all C API functions.
 *
 * <h1>C++</h1>
 *
 * ::Ort - Click here to go to the namespace holding all of the C++ wrapper classes
 *
 * It is a set of header only wrapper classes around the C API. The goal is to turn the C style return value error codes into C++ exceptions, and to
 * automate memory management through standard C++ RAII principles.
 *
 * \addtogroup Global
 * ONNX Runtime C API
 * @{
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>



//! @}
// SAL2 Definitions
#ifndef _MSC_VER
#define _In_
#define _In_z_
#define _In_opt_
#define _In_opt_z_
#define _Out_
#define _Outptr_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_
#define _Frees_ptr_opt_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Check_return_
#define _Outptr_result_maybenull_
#define _In_reads_(X)
#define _Inout_updates_(X)
#define _Out_writes_(X)
#define _Inout_updates_all_(X)
#define _Out_writes_bytes_all_(X)
#define _Out_writes_all_(X)
#define _Success_(X)
#define _Outptr_result_buffer_maybenull_(X)
#define ORT_ALL_ARGS_NONNULL __attribute__((nonnull))
#else
#include <specstrings.h>
#define ORT_ALL_ARGS_NONNULL
#endif

#ifdef _WIN32
// Define ORT_DLL_IMPORT if your program is dynamically linked to Ort.
// dllexport is not used, we use a .def file.
#ifdef ORT_DLL_IMPORT
#define ORT_EXPORT __declspec(dllimport)
#else
#define ORT_EXPORT
#endif
#define ORT_API_CALL _stdcall
#define ORT_MUST_USE_RESULT
#define ORTCHAR_T wchar_t
#else
// To make symbols visible on macOS/iOS
#ifdef __APPLE__
#define ORT_EXPORT __attribute__((visibility("default")))
#else
#define ORT_EXPORT
#endif
#define ORT_API_CALL
#define ORT_MUST_USE_RESULT __attribute__((warn_unused_result))
#define ORTCHAR_T char
#endif

/// ORTCHAR_T, ORT_TSTR are reserved specifically for path handling.
/// All other strings are UTF-8 encoded, use char and std::string
#ifndef ORT_TSTR
#ifdef _WIN32
#define ORT_TSTR(X) L##X
// When X is a macro, L##X is not defined. In this case, we need to use ORT_TSTR_ON_MACRO.
#define ORT_TSTR_ON_MACRO(X) L"" X
#else
#define ORT_TSTR(X) X
#define ORT_TSTR_ON_MACRO(X) X
#endif
#endif

/// @}
