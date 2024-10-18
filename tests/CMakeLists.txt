include_directories(../src/inc)
include_directories(../include)
#google benchmark doesn't work for Emscripten
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND NOT MLAS_NO_ONNXRUNTIME)
  add_subdirectory(bench)
endif()
add_subdirectory(unittest)
