/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
// Portions Copyright (c) Microsoft Corporation

#include "core/platform/windows/env.h"

#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <climits>
#include <process.h>
#include <fcntl.h>
#include <io.h>

#include <gsl/gsl>
#include "core/common/logging/logging.h"
#include "core/common/narrow.h"
#include "core/common/span_utils.h"
#include "core/platform/env.h"
#include "core/platform/scoped_resource.h"

#include <unsupported/Eigen/CXX11/ThreadPool>
#include <wil/Resource.h>

#include "core/platform/path_lib.h"  // for LoopDir()

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace onnxruntime {


std::wstring Basename(const std::wstring& path) {
  auto basename_index = path.find_last_of(L"/\\") + 1;  // results in 0 if no separator is found
  return path.substr(basename_index);
}

class WindowsThread : public EnvThread {
 private:
  struct Param {
    const ORTCHAR_T* name_prefix;
    int index;
    unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param);
    Eigen::ThreadPoolInterface* param;
    std::optional<LogicalProcessors> affinity;
    Param(const ORTCHAR_T* name_prefix1,
          int index1,
          unsigned (*start_address1)(int id, Eigen::ThreadPoolInterface* param),
          Eigen::ThreadPoolInterface* param1)
        : name_prefix(name_prefix1),
          index(index1),
          start_address(start_address1),
          param(param1) {}
  };

 public:
  WindowsThread(const ORTCHAR_T* name_prefix, int index,
                unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param), Eigen::ThreadPoolInterface* param,
                const ThreadOptions& thread_options) {
    ORT_ENFORCE(index >= 0, "Negative thread index is not allowed");
   
    std::unique_ptr<Param> local_param = std::make_unique<Param>(name_prefix, index, start_address, param);
    if (narrow<size_t>(index) < thread_options.affinities.size()) {
      local_param->affinity = thread_options.affinities[index];
    }

    {
      _set_errno(0);
      _set_doserrno(0);
      auto th_handle = _beginthreadex(nullptr, thread_options.stack_size, ThreadMain,
                                      local_param.get(), 0,
                                      &threadID);
      if (th_handle == 0) {
        auto dos_error = _doserrno;
        auto [err, msg] = GetErrnoInfo();
        ORT_THROW("WindowThread:_beginthreadex failed with errno:", err, " message:", msg,
                  " doserrno:", dos_error);
      }
      local_param.release();
      hThread.reset(reinterpret_cast<HANDLE>(th_handle));
      // Do not throw beyond this point so we do not lose thread handle and then not being able to join it.
    }
  }

  ~WindowsThread() {
    {
      DWORD waitStatus = WaitForSingleObject(hThread.get(), INFINITE);
      FAIL_FAST_LAST_ERROR_IF(waitStatus == WAIT_FAILED);
    }
  }

 private:
  typedef HRESULT(WINAPI* SetThreadDescriptionFunc)(HANDLE hThread, PCWSTR lpThreadDescription);

#pragma warning(push)
#pragma warning(disable : 6387)
  static unsigned __stdcall ThreadMain(void* param) {
    std::unique_ptr<Param> p(static_cast<Param*>(param));

    // Not all machines have kernel32.dll and/or SetThreadDescription (e.g. Azure App Service sandbox)
    // so we need to ensure it's available before calling.
    HMODULE kernelModule = GetModuleHandle(TEXT("kernel32.dll"));
    if (kernelModule != nullptr) {
      auto setThreadDescriptionFn = (SetThreadDescriptionFunc)GetProcAddress(kernelModule, "SetThreadDescription");
      if (setThreadDescriptionFn != nullptr) {
        const ORTCHAR_T* name_prefix = (p->name_prefix == nullptr || wcslen(p->name_prefix) == 0) ? L"onnxruntime"
                                                                                                  : p->name_prefix;
        std::wostringstream oss;
        oss << name_prefix << "-" << p->index;
        // Ignore any errors
        (void)(setThreadDescriptionFn)(GetCurrentThread(), oss.str().c_str());
      }
    }

    unsigned ret = 0;
    ORT_TRY {
      if (p->affinity.has_value() && !p->affinity->empty()) {
        int group_id = -1;
        KAFFINITY mask = 0;
        constexpr KAFFINITY bit = 1;
        const WindowsEnv& env = WindowsEnv::Instance();
        for (auto global_processor_id : *p->affinity) {
          auto processor_info = env.GetProcessorAffinityMask(global_processor_id);
          if (processor_info.local_processor_id > -1 &&
              processor_info.local_processor_id < sizeof(KAFFINITY) * CHAR_BIT) {
            mask |= bit << processor_info.local_processor_id;
          } else {
            // Logical processor id starts from 0 internally, but in ort API, it starts from 1,
            // that's why id need to increase by 1 when logging.
            std::cout << "Cannot set affinity for thread " << GetCurrentThreadId()
                                << ", processor " << global_processor_id + 1 << " does not exist";
            group_id = -1;
            mask = 0;
            break;
          }
          if (group_id == -1) {
            group_id = processor_info.group_id;
          } else if (group_id != processor_info.group_id) {
            std::cout << "Cannot set cross-group affinity for thread "
                                << GetCurrentThreadId() << ", first on group "
                                << group_id << ", then on " << processor_info.group_id;
            group_id = -1;
            mask = 0;
            break;
          }
        }  // for
        if (group_id > -1 && mask) {
          GROUP_AFFINITY thread_affinity = {};
          thread_affinity.Group = static_cast<WORD>(group_id);
          thread_affinity.Mask = mask;
          if (SetThreadGroupAffinity(GetCurrentThread(), &thread_affinity, nullptr)) {
            std::cout << "SetThreadAffinityMask done for thread: " << GetCurrentThreadId()
                                  << ", group_id: " << thread_affinity.Group
                                  << ", mask: " << thread_affinity.Mask;
          } else {
            const auto error_code = GetLastError();
            std::cout << "SetThreadAffinityMask failed for thread: " << GetCurrentThreadId()
                                << ", index: " << p->index
                                << ", mask: " << *p->affinity
                                << ", error code: " << error_code
                                << ", error msg: " << std::system_category().message(error_code)
                                << ". Specify the number of threads explicitly so the affinity is not set.";
          }
        }
      }

      ret = p->start_address(p->index, p->param);
    }
    ORT_CATCH(...) {
      p->param->Cancel();
      ret = 1;
    }
    return ret;
  }
#pragma warning(pop)

  static void CustomThreadMain(void* param) {
    std::unique_ptr<Param> p(static_cast<Param*>(param));
    ORT_TRY {
      p->start_address(p->index, p->param);
    }
    ORT_CATCH(...) {
      p->param->Cancel();
    }
  }
  unsigned threadID = 0;
  wil::unique_handle hThread;
};

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable : 26409)
#endif
EnvThread* WindowsEnv::CreateThread(_In_opt_z_ const ORTCHAR_T* name_prefix, int index,
                                    unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param),
                                    Eigen::ThreadPoolInterface* param, const ThreadOptions& thread_options) {
  return new WindowsThread(name_prefix, index, start_address, param, thread_options);
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

Env& Env::Default() {
  return WindowsEnv::Instance();
}


// EIGEN_NO_CPUID is not defined in any C/C++ source code. It is a compile option.
#if defined(_M_X64) && !defined(_M_ARM64EC) && !defined(EIGEN_NO_CPUID)
static constexpr std::array<int, 3> kVendorID_Intel = {0x756e6547, 0x6c65746e, 0x49656e69};  // "GenuntelineI"
#endif
int WindowsEnv::DefaultNumCores() {
  return std::max(1, static_cast<int>(std::thread::hardware_concurrency() / 2));
}

int WindowsEnv::GetNumPhysicalCpuCores() const {
  return cores_.empty() ? DefaultNumCores() : static_cast<int>(cores_.size());
}

std::vector<LogicalProcessors> WindowsEnv::GetDefaultThreadAffinities() const {
  return cores_.empty() ? std::vector<LogicalProcessors>(DefaultNumCores(), LogicalProcessors{}) : cores_;
}

int WindowsEnv::GetL2CacheSize() const {
  return l2_cache_size_;
}

WindowsEnv& WindowsEnv::Instance() {
  static WindowsEnv default_env;
  return default_env;
}

PIDType WindowsEnv::GetSelfPid() const {
  return GetCurrentProcessId();
}

// \brief returns a value for the queried variable name (var_name)
std::string WindowsEnv::GetEnvironmentVar(const std::string& var_name) const {
  // Why getenv() should be avoided on Windows:
  // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/getenv-wgetenv
  // Instead use the Win32 API: GetEnvironmentVariableA()

  // Max limit of an environment variable on Windows including the null-terminating character
  constexpr DWORD kBufferSize = 32767;

  // Create buffer to hold the result
  std::string buffer(kBufferSize, '\0');

  // The last argument is the size of the buffer pointed to by the lpBuffer parameter, including the null-terminating character, in characters.
  // If the function succeeds, the return value is the number of characters stored in the buffer pointed to by lpBuffer, not including the terminating null character.
  // Therefore, If the function succeeds, kBufferSize should be larger than char_count.
  auto char_count = GetEnvironmentVariableA(var_name.c_str(), buffer.data(), kBufferSize);

  if (kBufferSize > char_count) {
    buffer.resize(char_count);
    return buffer;
  }

  // Else either the call was failed, or the buffer wasn't large enough.
  // TODO: Understand the reason for failure by calling GetLastError().
  // If it is due to the specified environment variable being found in the environment block,
  // GetLastError() returns ERROR_ENVVAR_NOT_FOUND.
  // For now, we assume that the environment variable is not found.

  return std::string();
}

/*
Read logical processor info from the map.
{-1,-1} stands for failure.
*/
ProcessorInfo WindowsEnv::GetProcessorAffinityMask(int global_processor_id) const {
  if (global_processor_info_map_.count(global_processor_id)) {
    return global_processor_info_map_.at(global_processor_id);
  } else {
    return {-1, -1};
  }
}

WindowsEnv::WindowsEnv() {
  l2_cache_size_ = 0;
  InitializeCpuInfo();
}

/*
Discover all cores in a windows system.
Note - every "id" here, given it be group id, core id, or logical processor id, starts from 0.
*/
void WindowsEnv::InitializeCpuInfo() {
  DWORD returnLength = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &returnLength);
  auto last_error = GetLastError();
  if (last_error != ERROR_INSUFFICIENT_BUFFER) {
    const auto error_code = GetLastError();
    if (logging::LoggingManager::HasDefaultLogger()) {
      std::cout << "Failed to calculate byte size for saving cpu info on windows"
                          << ", error code: " << error_code
                          << ", error msg: " << std::system_category().message(error_code);
    }
    return;
  }

  std::unique_ptr<char[]> allocation = std::make_unique<char[]>(returnLength);
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* processorInfos = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(allocation.get());

  if (!GetLogicalProcessorInformationEx(RelationProcessorCore, processorInfos, &returnLength)) {
    const auto error_code = GetLastError();
    if (logging::LoggingManager::HasDefaultLogger()) {
      std::cout << "Failed to fetch cpu info on windows"
                          << ", error code: " << error_code
                          << ", error msg: " << std::system_category().message(error_code);
    }
    return;
  }

  int core_id = 0;
  int global_processor_id = 0;
  const BYTE* iter = reinterpret_cast<const BYTE*>(processorInfos);
  const BYTE* end = iter + returnLength;
  std::stringstream log_stream;

  while (iter < end) {
    auto processor_info = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(iter);
    auto size = processor_info->Size;

    // Discoverred a phyical core and it belongs exclusively to a single group
    if (processor_info->Relationship == RelationProcessorCore &&
        processor_info->Processor.GroupCount == 1) {
      log_stream << std::endl
                 << "core " << core_id + 1 << " consist of logical processors: ";
      LogicalProcessors core_global_proc_ids;
      constexpr KAFFINITY bit = 1;
      constexpr int id_upper_bound = sizeof(KAFFINITY) * CHAR_BIT;
      const auto& group_mask = processor_info->Processor.GroupMask[0];
      for (int logical_proessor_id = 0; logical_proessor_id < id_upper_bound; ++logical_proessor_id) {
        if (group_mask.Mask & (bit << logical_proessor_id)) {
          log_stream << global_processor_id + 1 << " ";
          core_global_proc_ids.push_back(global_processor_id);
          /*
           * Build up a map between global processor id and local processor id.
           * The map helps to bridge between ort API and windows affinity API -
           * we need local processor id to build an affinity mask for a particular group.
           */
          global_processor_info_map_.insert_or_assign(global_processor_id,
                                                      ProcessorInfo{static_cast<int>(group_mask.Group),
                                                                    logical_proessor_id});
          global_processor_id++;
        }
      }
      cores_.push_back(std::move(core_global_proc_ids));
      core_id++;
    }
    iter += size;
  }

  DWORD newLength = 0;
  GetLogicalProcessorInformationEx(RelationCache, nullptr, &newLength);
  last_error = GetLastError();
  if (last_error != ERROR_INSUFFICIENT_BUFFER) {
    const auto error_code = GetLastError();
    if (logging::LoggingManager::HasDefaultLogger()) {
      std::cout << "Failed to calculate byte size for saving cpu info on windows"
                          << ", error code: " << error_code
                          << ", error msg: " << std::system_category().message(error_code);
    }
    return;
  }

  if (newLength > returnLength) {
    // Re-allocate
    allocation = std::make_unique<char[]>(newLength);
    processorInfos = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(allocation.get());
  }

  if (!GetLogicalProcessorInformationEx(RelationCache, processorInfos, &newLength)) {
    const auto error_code = GetLastError();
    if (logging::LoggingManager::HasDefaultLogger()) {
      std::cout << "Failed to fetch cpu info on windows"
                          << ", error code: " << error_code
                          << ", error msg: " << std::system_category().message(error_code);
    }
    return;
  }

  iter = reinterpret_cast<const BYTE*>(processorInfos);
  end = iter + newLength;

  while (iter < end) {
    auto processor_info = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(iter);
    auto size = processor_info->Size;

    if (processor_info->Relationship == RelationCache &&
        processor_info->Cache.Level == 2) {
      // L2 cache
      l2_cache_size_ = static_cast<int>(processor_info->Cache.CacheSize);
      break;
    }

    iter += size;
  }

  if (logging::LoggingManager::HasDefaultLogger()) {
    std::cout << "Found total " << cores_.size() << " core(s) from windows system:";
    std::cout << log_stream.str();
    std::cout << "\nDetected L2 cache size: " << l2_cache_size_ << " bytes";
  }
}
}  // namespace onnxruntime
