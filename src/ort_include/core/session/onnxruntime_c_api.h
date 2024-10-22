#pragma once

#ifndef _WIN32
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
#else
#include <specstrings.h>
#endif

#ifdef _WIN32
#define ORTCHAR_T wchar_t
#else
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