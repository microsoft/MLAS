#pragma once

#include <mutex>
#include <condition_variable>

namespace onnxruntime{
	using OrtMutex = std::mutex;
	using OrtCondVar = std::condition_variable;
}