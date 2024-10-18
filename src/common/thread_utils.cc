// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/util/thread_utils.h"

#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#include <versionhelpers.h>
#endif
#include <thread>


namespace onnxruntime {
namespace concurrency {

static std::unique_ptr<ThreadPool>
CreateThreadPoolHelper(Env* env, OrtThreadPoolParams options) {
  ThreadOptions to;
  if (options.thread_pool_size <= 0) {  // default
    if (options.auto_set_affinity) {
#ifdef _WIN32
      // Only set thread affinity on Server with auto affinity.
      // On client best to let OS scheduler handle.
      // On big (P-Core) / little (E-Core) CPU designs affinity overrides QoS and has high power usage
      if (IsWindowsServer()) {
        auto default_affinities = Env::Default().GetDefaultThreadAffinities();
        if (default_affinities.size() <= 1) {
          return nullptr;
        }
        options.thread_pool_size = static_cast<int>(default_affinities.size());
        to.affinities = std::move(default_affinities);
      } else {
        options.thread_pool_size = Env::Default().GetNumPhysicalCpuCores();
      }
#else
      auto default_affinities = Env::Default().GetDefaultThreadAffinities();
      if (default_affinities.size() <= 1) {
        return nullptr;
      }
      options.thread_pool_size = static_cast<int>(default_affinities.size());
      to.affinities = std::move(default_affinities);
#endif
    } else {
      options.thread_pool_size = Env::Default().GetNumPhysicalCpuCores();
    }
  }
  if (options.thread_pool_size <= 1) {
    return nullptr;
  }

  to.set_denormal_as_zero = options.set_denormal_as_zero;

  return std::make_unique<ThreadPool>(env, to, options.name, options.thread_pool_size,
                                      options.allow_spinning);
}

std::unique_ptr<ThreadPool>
CreateThreadPool(Env* env, OrtThreadPoolParams options, ThreadPoolType tpool_type) {
  // If openmp is enabled we don't want to create any additional threadpools for sequential execution.
  // However, parallel execution relies on the existence of a separate threadpool. Hence we allow eigen threadpools
  // to be created for parallel execution.
  ORT_UNUSED_PARAMETER(tpool_type);
  return CreateThreadPoolHelper(env, options);
}

}  // namespace concurrency
}  // namespace onnxruntime
