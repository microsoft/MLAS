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

#include "core/platform/env.h"

#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#if !defined(_AIX)
#include <sys/syscall.h>
#endif
#include <unistd.h>

#include <iostream>
#include <optional>
#include <thread>
#include <utility>  // for std::forward
#include <vector>

// We can not use CPUINFO if it is not supported and we do not want to used
// it on certain platforms because of the binary size increase.
// We could use it to find out the number of physical cores for certain supported platforms
#if defined(CPUINFO_SUPPORTED) && !defined(__APPLE__) && !defined(__ANDROID__) && !defined(__wasm__) && !defined(_AIX)
#include <cpuinfo.h>
#define ORT_USE_CPUINFO
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

#include "core/common/common.h"
#include <gsl/gsl>
#include "core/common/logging/logging.h"
#include "core/common/narrow.h"
#include "core/platform/scoped_resource.h"
#include "core/platform/EigenNonBlockingThreadPool.h"

namespace onnxruntime {

namespace {

constexpr int OneMillion = 1000000;

struct FileDescriptorTraits {
  using Handle = int;
  static Handle GetInvalidHandleValue() { return -1; }
  static void CleanUp(Handle h) {
    if (close(h) == -1) {
      auto [err_no, err_msg] = GetErrnoInfo();
      std::cout << "Failed to close file descriptor " << h << " - error code: " << err_no
                          << " error msg: " << err_msg;
    }
  }
};

// Note: File descriptor cleanup may fail but this class doesn't expose a way to check if it failed.
//       If that's important, consider using another cleanup method.
using ScopedFileDescriptor = ScopedResource<FileDescriptorTraits>;

// non-macro equivalent of TEMP_FAILURE_RETRY, described here:
// https://www.gnu.org/software/libc/manual/html_node/Interrupted-Primitives.html
template <typename TFunc, typename... TFuncArgs>
long int TempFailureRetry(TFunc retriable_operation, TFuncArgs&&... args) {
  long int result;
  do {
    result = retriable_operation(std::forward<TFuncArgs>(args)...);
  } while (result == -1 && errno == EINTR);
  return result;
}

template <typename T>
struct Freer {
  void operator()(T* p) { ::free(p); }
};

using MallocdStringPtr = std::unique_ptr<char, Freer<char> >;

class PosixThread : public EnvThread {
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
  PosixThread(const ORTCHAR_T* name_prefix, int index,
              unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param), Eigen::ThreadPoolInterface* param,
              const ThreadOptions& thread_options) {
    ORT_ENFORCE(index >= 0, "Negative thread index is not allowed");
   
    auto param_ptr = std::make_unique<Param>(name_prefix, index, start_address, param);
    if (narrow<size_t>(index) < thread_options.affinities.size()) {
      param_ptr->affinity = thread_options.affinities[index];
    }

    {
      pthread_attr_t attr;
      int s = pthread_attr_init(&attr);
      if (s != 0) {
        auto [err_no, err_msg] = GetErrnoInfo();
        ORT_THROW("pthread_attr_init failed, error code: ", err_no, " error msg: ", err_msg);
      }

      size_t stack_size = thread_options.stack_size;
      if (stack_size > 0) {
        s = pthread_attr_setstacksize(&attr, stack_size);
        if (s != 0) {
          auto [err_no, err_msg] = GetErrnoInfo();
          ORT_THROW("pthread_attr_setstacksize failed, error code: ", err_no, " error msg: ", err_msg);
        }
      }

      s = pthread_create(&hThread, &attr, ThreadMain, param_ptr.get());
      if (s != 0) {
        auto [err_no, err_msg] = GetErrnoInfo();
        ORT_THROW("pthread_create failed, error code: ", err_no, " error msg: ", err_msg);
      }
      param_ptr.release();
      // Do not throw beyond this point so we do not lose thread handle and then not being able to join it.
    }
  }

  ~PosixThread() override {
    {
      void* res;
#ifdef NDEBUG
      pthread_join(hThread, &res);
#else
      int ret = pthread_join(hThread, &res);
      assert(ret == 0);
#endif
    }
  }

 private:
  static void* ThreadMain(void* param) {
    std::unique_ptr<Param> p(static_cast<Param*>(param));
    ORT_TRY {
#if !defined(__APPLE__) && !defined(__ANDROID__) && !defined(__wasm__) && !defined(_AIX)
      if (p->affinity.has_value() && !p->affinity->empty()) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (auto id : *p->affinity) {
          if (id > -1 && id < CPU_SETSIZE) {
            CPU_SET(id, &cpuset);
          } else {
            // Logical processor id starts from 0 internally, but in ort API, it starts from 1,
            // that's why id need to increase by 1 when logging.
            std::cout << "cpu " << id + 1 << " does not exist, skipping it for affinity setting";
          }
        }
        auto ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (0 == ret) {
          std::cout << "pthread_setaffinity_np succeed for thread: " << syscall(SYS_gettid)
                                << ", index: " << p->index
                                << ", mask: " << *p->affinity;
        } else {
          errno = ret;
          auto [err_no, err_msg] = GetErrnoInfo();
#if !defined(USE_MIGRAPHX)
          std::cout << "pthread_setaffinity_np failed for thread: " << syscall(SYS_gettid)
                              << ", index: " << p->index
                              << ", mask: " << *p->affinity
                              << ", error code: " << err_no << " error msg: " << err_msg
                              << ". Specify the number of threads explicitly so the affinity is not set.";
#endif
        }
      }
#endif
      // Ignore the returned value for now
      p->start_address(p->index, p->param);
    }
    ORT_CATCH(...) {
      // Ignore exceptions
    }
    return nullptr;
  }
  static void CustomThreadMain(void* param) {
    ThreadMain(param);
  }
  pthread_t hThread;
};

class PosixEnv : public Env {
 public:
  static PosixEnv& Instance() {
    static PosixEnv default_env;
    return default_env;
  }

  EnvThread* CreateThread(const ORTCHAR_T* name_prefix, int index,
                          unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param),
                          Eigen::ThreadPoolInterface* param, const ThreadOptions& thread_options) override {
    return new PosixThread(name_prefix, index, start_address, param, thread_options);
  }

  // we are guessing the number of phys cores based on a popular HT case (2 logical proc per core)
  static int DefaultNumCores() {
    return std::max(1, static_cast<int>(std::thread::hardware_concurrency() / 2));
  }

  // Return the number of physical cores
  int GetNumPhysicalCpuCores() const override {
#ifdef ORT_USE_CPUINFO
    if (cpuinfo_available_) {
      return narrow<int>(cpuinfo_get_cores_count());
    }
#endif  // ORT_USE_CPUINFO
    return DefaultNumCores();
  }

  std::vector<LogicalProcessors> GetDefaultThreadAffinities() const override {
    std::vector<LogicalProcessors> ret;
#ifdef ORT_USE_CPUINFO
    if (cpuinfo_available_) {
      auto num_phys_cores = cpuinfo_get_cores_count();
      ret.reserve(num_phys_cores);
      for (uint32_t i = 0; i < num_phys_cores; ++i) {
        const auto* core = cpuinfo_get_core(i);
        LogicalProcessors th_aff;
        th_aff.reserve(core->processor_count);
        auto log_proc_idx = core->processor_start;
        for (uint32_t count = 0; count < core->processor_count; count++, ++log_proc_idx) {
          const auto* log_proc = cpuinfo_get_processor(log_proc_idx);
          th_aff.push_back(log_proc->linux_id);
        }
        ret.push_back(std::move(th_aff));
      }
    }
#endif
    // Just the size of the thread-pool
    if (ret.empty()) {
      ret.resize(GetNumPhysicalCpuCores());
    }
    return ret;
  }

  int GetL2CacheSize() const override {
#ifdef _SC_LEVEL2_CACHE_SIZE
    return static_cast<int>(sysconf(_SC_LEVEL2_CACHE_SIZE));
#else
    int value = 0;  // unknown
#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)) && defined(HW_L2CACHESIZE)
    int mib[2] = {CTL_HW, HW_L2CACHESIZE};
    size_t len = sizeof(value);
    if (sysctl(mib, 2, &value, &len, NULL, 0) < 0) {
      return -1;  // error
    }
#endif
    return value;
#endif
  }


  PIDType GetSelfPid() const override {
    return getpid();
  }

 

  static common::Status ReportSystemError(const char* operation_name, const std::string& path) {
    auto [err_no, err_msg] = GetErrnoInfo();
    std::ostringstream oss;
    oss << operation_name << " file \"" << path << "\" failed: " << err_msg;
    return common::Status(common::SYSTEM, err_no, oss.str());
  }


  // \brief returns a value for the queried variable name (var_name)
  std::string GetEnvironmentVar(const std::string& var_name) const override {
    char* val = getenv(var_name.c_str());
    return val == NULL ? std::string() : std::string(val);
  }

 private:
#ifdef ORT_USE_CPUINFO
  PosixEnv() {
    cpuinfo_available_ = cpuinfo_initialize();
    if (!cpuinfo_available_) {
      std::cout << "cpuinfo_initialize failed";
    }
  }
  bool cpuinfo_available_{false};
#endif  // ORT_USE_CPUINFO
};

}  // namespace

// REGISTER_FILE_SYSTEM("", PosixFileSystem);
// REGISTER_FILE_SYSTEM("file", LocalPosixFileSystem);
Env& Env::Default() {
  return PosixEnv::Instance();
}

}  // namespace onnxruntime
