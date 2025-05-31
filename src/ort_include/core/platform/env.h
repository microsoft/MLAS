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

#pragma once

#include <iosfwd>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <gsl/gsl>

#include "core/common/common.h"
#include "core/common/path_string.h"
#include "core/framework/callback.h"
#include "core/session/onnxruntime_c_api.h"

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif
namespace Eigen {
class ThreadPoolInterface;
}
namespace onnxruntime {
namespace concurrency {
  inline void SpinPause(){}
}
#ifdef _WIN32
using PIDType = unsigned long;
using FileOffsetType = int64_t;
#else
using PIDType = pid_t;
using FileOffsetType = off_t;
#endif

class EnvThread {
 public:
  virtual ~EnvThread() = default;
};

/// Type that holds a collection of logical processors IDs used for setting affinities.
using LogicalProcessors = std::vector<int>;

// Parameters that are required to create a set of threads for a thread pool
struct ThreadOptions {
  // Stack size for a new thread. If it is 0, the operating system uses the same value as the stack that's specified for
  // the main thread, which is usually set in the main executable(not controlled by onnxruntime.dll).
  unsigned int stack_size = 0;

  // Thread affinity means a thread can only run on the logical processor(s) that the thread is allowed to run on.
  // If the vector is not empty, then set the affinity of each thread to logical cpus ids within the LogicalProcessors.
  // For example, the first thread in the pool will be bound to the logical processors contained in affinity[0].
  // If the vector is empty, the thread can run on all the processors its process can run on.
  // NOTE: When hyperthreading is enabled, for example, on a 4 cores we would have 8 logical processors,
  // processor group [0,1,2,3] may only occupy up some of the physical cores. There might be more than 2 logical
  // processor per physical core on a given computer. Physical cores assigned to a given VM may contain
  // logical processor indices that do not start with 0 and possibly go beyond the number of bits in an integer.
  //
  // If the size of the TP is not specified, ORT creates thread pools with a number of threads that are equal
  // to the number of visible physical cores. The threads affinities are set to all of the logical processors
  // that are contained in a given physical core with the same index as the thread. ORT does not set any affinity
  // to the thread that is considered main (the thread that initiates the creation of the TP).
  // The process that owns the thread may consider setting its affinity.
  std::vector<LogicalProcessors> affinities;

  // Set or unset denormal as zero.
  bool set_denormal_as_zero = false;
};

std::ostream& operator<<(std::ostream& os, const LogicalProcessors&);
std::ostream& operator<<(std::ostream& os, gsl::span<const LogicalProcessors>);

/// <summary>
/// Get errno and the corresponding error message.
/// </summary>
/// <returns>errno and the error message string if errno indicates an error.</returns>
std::pair<int, std::string> GetErrnoInfo();

/// \brief An interface used by the onnxruntime implementation to
/// access operating system functionality like the filesystem etc.
///
/// Callers may wish to provide a custom Env object to get fine grain
/// control.
///
/// All Env implementations are safe for concurrent access from
/// multiple threads without any external synchronization.
class Env {
 public:
  using EnvThread = onnxruntime::EnvThread;
  virtual ~Env() = default;
  // clang-format off
  /**
   * Start a new thread for a thread pool
   * \param name_prefix A human-readable string for debugging purpose, can be NULL
   * \param index The index value of the thread, for each thread pool instance, the index should start from 0 and be continuous.
   * \param start_address The entry point of thread
   * \param threadpool The thread pool that the new thread belongs to
   * \param thread_options options to create the thread
   *
   * Caller is responsible for deleting the returned value
   */
  // clang-format on
  virtual EnvThread* CreateThread(_In_opt_z_ const ORTCHAR_T* name_prefix, int index,
                                  _In_ unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param),
                                  Eigen::ThreadPoolInterface* threadpool, const ThreadOptions& thread_options) = 0;

  /// \brief Returns a default environment suitable for the current operating
  /// system.
  ///
  /// Sophisticated users may wish to provide their own Env
  /// implementation instead of relying on this default environment.
  ///
  /// The result of Default() belongs to this library and must never be deleted.
  static Env& Default();

  /// <summary>
  /// The API returns the number of different physical cores on the system
  /// </summary>
  /// <returns>Number of physical cores</returns>
  virtual int GetNumPhysicalCpuCores() const = 0;

  virtual std::vector<LogicalProcessors> GetDefaultThreadAffinities() const = 0;

  virtual int GetL2CacheSize() const = 0;




  // This functions is always successful. It can't fail.
  virtual PIDType GetSelfPid() const = 0;


  // \brief returns a value for the queried variable name (var_name)
  //
  // Returns the corresponding value stored in the environment variable if available
  // Returns empty string if there is no such environment variable available
  virtual std::string GetEnvironmentVar(const std::string& var_name) const = 0;

 protected:
  Env();

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(Env);
};

}  // namespace onnxruntime
