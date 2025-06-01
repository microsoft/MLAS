// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdlib>      // For malloc, free, abort
#include <cstddef>      // For size_t
#include <functional>   // For std::function
#include <algorithm>    // For std::fill_n
#include <new>          // For std::bad_alloc (alternative to abort)

// Include crtdbg.h for _malloc_dbg and _free_dbg on Windows debug builds
#if defined(_WIN32) && !defined(NDEBUG) && defined(_DEBUG)
#include <crtdbg.h>
#endif

template <typename T>
class MatrixGuardBuffer {
public:
    MatrixGuardBuffer() :
        buffer_(nullptr),
        elements_allocated_(0) {
    }

    ~MatrixGuardBuffer() {
        ReleaseBuffer();
    }

    // Disable copy and move semantics for simplicity
    MatrixGuardBuffer(const MatrixGuardBuffer&) = delete;
    MatrixGuardBuffer& operator=(const MatrixGuardBuffer&) = delete;
    MatrixGuardBuffer(MatrixGuardBuffer&&) = delete;
    MatrixGuardBuffer& operator=(MatrixGuardBuffer&&) = delete;

    T* GetFilledBuffer(size_t elements, const std::function<void(T*, size_t)>& fill_func) {
        if (elements == 0) {
            ReleaseBuffer();
            return nullptr;
        }

        if (elements > elements_allocated_) {
            ReleaseBuffer(); 

            size_t bytes_to_allocate = elements * sizeof(T);
            if (elements != 0 && bytes_to_allocate / elements != sizeof(T)) { // Check for overflow before multiplication
                // Handle overflow, e.g., by aborting or throwing
                abort(); 
            }
#ifdef _WIN32
            buffer_ = static_cast<T*>(_aligned_malloc(bytes_to_allocate, 64));
#else
            buffer_ = static_cast<T*>(std::aligned_alloc(64, bytes_to_allocate));
#endif

            if (buffer_ == nullptr) {
                // Consider `throw std::bad_alloc();` for C++ style error handling.
                abort(); 
            }
            elements_allocated_ = elements;
        }

        if (fill_func && buffer_ != nullptr) {
            fill_func(buffer_, elements);
        } else if (buffer_ == nullptr && elements > 0) {
            abort(); // Should not happen if allocation failure aborts
        }
        
        return buffer_;
    }

    T* GetBuffer(size_t elements, bool zero_fill = false) {
        if (zero_fill) {
            return GetFilledBuffer(
                elements,
                [](T* start, size_t count) {
                    if (start && count > 0) {
                        std::fill_n(start, count, T{}); // Value-initialize
                    }
                });
        }

        return GetFilledBuffer(
            elements,
            [](T* start, size_t count) {
              constexpr float offset = -21.f;
              constexpr float range = 43.f;

              // The following value will be used in most GEMM/CONV tests. Because this value is an integer that is
              // small enough, all the floating point operations will generate exact values instead of approximate 
              // values.
              float FillValue = 11.f;
              T* FillAddress = start;
              for (size_t i = 0; i < count; i++) {
                auto itemv = FillValue - offset;
                *FillAddress++ = (T)(itemv);

                FillValue += 7.f;
                FillValue = FillValue >= range ? FillValue - range : FillValue;
              }
            });
    }

    void ReleaseBuffer() {
        if (buffer_ != nullptr) {
        #if defined(_WIN32)
            _aligned_free(buffer_);
        #else
            free(buffer_);
        #endif
            buffer_ = nullptr;
        }
        elements_allocated_ = 0;
    }

private:
    T* buffer_;
    size_t elements_allocated_;
};