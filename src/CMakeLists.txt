if(NOT MLAS_NO_ONNXRUNTIME AND NOT TARGET onnxruntime_common)
    set(onnxruntime_common_src_patterns
        "${ONNXRUNTIME_INCLUDE_DIR}/core/common/*.h"
        "${ONNXRUNTIME_INCLUDE_DIR}/core/common/logging/*.h"
        "${ONNXRUNTIME_INCLUDE_DIR}/core/platform/*.h"
        "common/*.h"
        "common/*.cc"
        "common/logging/*.h"
        "common/logging/*.cc"
        "core/platform/env.h"
        "core/platform/env.cc"
        "core/platform/env_time.h"
        "core/platform/env_time.cc"
        "core/platform/path_lib.h"
        "core/platform/path_lib.cc"
        "core/platform/scoped_resource.h"
        "core/platform/telemetry.h"
        "core/platform/telemetry.cc"
        )

    if(WIN32)
        list(APPEND onnxruntime_common_src_patterns
                "core/platform/windows/*.h"
                "core/platform/windows/*.cc"
                "core/platform/windows/logging/*.h"
                "core/platform/windows/logging/*.cc"
                )
    else()
        list(APPEND onnxruntime_common_src_patterns
                "core/platform/posix/*.h"
                "core/platform/posix/*.cc"
                )

        if (onnxruntime_USE_SYSLOG)
            list(APPEND onnxruntime_common_src_patterns
                    "core/platform/posix/logging/*.h"
                    "core/platform/posix/logging/*.cc"
                    )
        endif()

        if (CMAKE_SYSTEM_NAME STREQUAL "Android")
            list(APPEND onnxruntime_common_src_patterns
                    "core/platform/android/logging/*.h"
                    "core/platform/android/logging/*.cc"
                    )
        endif()

        if (APPLE)
            list(APPEND onnxruntime_common_src_patterns
                    "core/platform/apple/logging/*.h"
                    "core/platform/apple/logging/*.mm"
                    )
        endif()
    endif()
    file(GLOB onnxruntime_common_src CONFIGURE_DEPENDS
            ${onnxruntime_common_src_patterns}
            )

    add_library(onnxruntime_common STATIC ${onnxruntime_common_src})

    target_include_directories(onnxruntime_common
            PRIVATE ${ONNXRUNTIME_INCLUDE_DIR} ${ONNXRUNTIME_ROOT} ${eigen_SOURCE_DIR})
    if(WIN32)
      target_compile_definitions(onnxruntime_common PRIVATE NOMINMAX)
    endif()
    target_link_libraries(onnxruntime_common PUBLIC Microsoft.GSL::GSL ${WIL_TARGET})
endif()
add_subdirectory(lib)

