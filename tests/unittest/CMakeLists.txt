add_executable(onnxruntime_mlas_test test_activation.cpp
test_blkq8.cpp
test_blockq4.cpp
test_conv2d.cpp
test_conv2d_nchwc.cpp
test_exp.cpp
test_fgemm.cpp
test_fp16_activation.cpp
test_halfgemm.cpp
test_main.cpp
test_minmax.cpp
test_pool2d.cpp
test_pool2d_nchwc.cpp
test_pool3d.cpp
test_q4gemm.cpp
test_q4qdq.cpp
test_q8q4gemm.cpp
test_qgemm.cpp
test_qlinear_binaryop.cpp
test_qlinear_gavgpool.cpp
test_quantizelinear.cpp
test_reorder_output.cpp
test_sbgemm.cpp
test_scaleoutput.cpp
test_softmax.cpp
test_sqnbitgemm.cpp
test_symm_qgemm.cpp
test_transpose.cpp)
if(MSVC)
  target_compile_options(onnxruntime_mlas_test PRIVATE "$<$<COMPILE_LANGUAGE:CUDA>:SHELL:--compiler-options /wd26409>"
			  "$<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:/wd26409>")
  target_compile_options(onnxruntime_mlas_test PRIVATE "$<$<COMPILE_LANGUAGE:CUDA>:SHELL:--compiler-options /utf-8>"
		  "$<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:/utf-8>")
  target_compile_options(onnxruntime_mlas_test PRIVATE "$<$<COMPILE_LANGUAGE:CUDA>:SHELL:--compiler-options /wd6326>"
			  "$<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:/wd6326>")
  target_compile_options(onnxruntime_mlas_test PRIVATE "$<$<COMPILE_LANGUAGE:CUDA>:SHELL:--compiler-options /wd26426>"
			  "$<$<NOT:$<COMPILE_LANGUAGE:CUDA>>:/wd26426>")
endif()
if(IOS)
  set_target_properties(onnxruntime_mlas_test PROPERTIES
	XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
  )
endif()
target_include_directories(onnxruntime_mlas_test PRIVATE ${ONNXRUNTIME_ROOT}/core/mlas/inc ${ONNXRUNTIME_ROOT}
		${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(onnxruntime_mlas_test PRIVATE GTest::gtest GTest::gmock ${ONNXRUNTIME_MLAS_LIBS} onnxruntime_common)
if (CPUINFO_SUPPORTED AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_link_libraries(onnxruntime_mlas_test PRIVATE cpuinfo)
endif()
if(NOT WIN32)
  target_link_libraries(onnxruntime_mlas_test PRIVATE ${CMAKE_DL_LIBS})
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_link_libraries(onnxruntime_mlas_test PRIVATE ${android_shared_libs})
endif()
if(WIN32)
  target_link_libraries(onnxruntime_mlas_test PRIVATE debug Dbghelp Advapi32)
endif()
if (onnxruntime_LINK_LIBATOMIC)
  target_link_libraries(onnxruntime_mlas_test PRIVATE atomic)
endif()
target_link_libraries(onnxruntime_mlas_test PRIVATE Threads::Threads)
set_target_properties(onnxruntime_mlas_test PROPERTIES FOLDER "ONNXRuntimeTest")
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  if (onnxruntime_ENABLE_WEBASSEMBLY_THREADS)
	set_target_properties(onnxruntime_mlas_test PROPERTIES LINK_FLAGS "-s ALLOW_MEMORY_GROWTH=1 -s PROXY_TO_PTHREAD=1 -s EXIT_RUNTIME=1")
  else()
	set_target_properties(onnxruntime_mlas_test PROPERTIES LINK_FLAGS "-s ALLOW_MEMORY_GROWTH=1")
  endif()
endif()