file(STRINGS cmake/deps.txt ONNXRUNTIME_DEPS_LIST)
foreach(ONNXRUNTIME_DEP IN LISTS ONNXRUNTIME_DEPS_LIST)
  # Lines start with "#" are comments
  if(NOT ONNXRUNTIME_DEP MATCHES "^#")
	# The first column is name
	list(POP_FRONT ONNXRUNTIME_DEP ONNXRUNTIME_DEP_NAME)
	# The second column is URL
	# The URL below may be a local file path or an HTTPS URL
	list(POP_FRONT ONNXRUNTIME_DEP ONNXRUNTIME_DEP_URL)
	set(DEP_URL_${ONNXRUNTIME_DEP_NAME} ${ONNXRUNTIME_DEP_URL})
	# The third column is SHA1 hash value
	set(DEP_SHA1_${ONNXRUNTIME_DEP_NAME} ${ONNXRUNTIME_DEP})

	if(ONNXRUNTIME_DEP_URL MATCHES "^https://")
	  # Search a local mirror folder
	  string(REGEX REPLACE "^https://" "${REPO_ROOT}/mirror/" LOCAL_URL "${ONNXRUNTIME_DEP_URL}")

	  if(EXISTS "${LOCAL_URL}")
		cmake_path(ABSOLUTE_PATH LOCAL_URL)
		set(DEP_URL_${ONNXRUNTIME_DEP_NAME} "${LOCAL_URL}")
	  endif()
	endif()
  endif()
endforeach()

message(STATUS "Loading Dependencies ...")
include(FetchContent)
include(cmake/helper_functions.cmake)

onnxruntime_fetchcontent_declare(
	GSL
	URL ${DEP_URL_microsoft_gsl}
	URL_HASH SHA1=${DEP_SHA1_microsoft_gsl}
	EXCLUDE_FROM_ALL
	FIND_PACKAGE_ARGS 4.0 NAMES Microsoft.GSL
)
  
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "")
set(EIGEN_BUILD_DOC OFF CACHE BOOL "")
set(EIGEN_BUILD_DEMOS OFF CACHE BOOL "")

onnxruntime_fetchcontent_declare(
		eigen
		URL ${DEP_URL_eigen}
		URL_HASH SHA1=${DEP_SHA1_eigen}
		EXCLUDE_FROM_ALL
	)

onnxruntime_fetchcontent_makeavailable(GSL eigen)

if(WIN32 AND NOT MLAS_NO_ONNXRUNTIME)
	set(WIL_BUILD_PACKAGING OFF CACHE BOOL "" FORCE)
	set(WIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)

	FetchContent_Declare(
	  microsoft_wil
	  URL ${DEP_URL_microsoft_wil}
	  URL_HASH SHA1=${DEP_SHA1_microsoft_wil}
	  FIND_PACKAGE_ARGS NAMES wil
	)


    onnxruntime_fetchcontent_makeavailable(microsoft_wil)
    set(WIL_TARGET "WIL::WIL")
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  # WebAssembly threading support in Node.js is still an experimental feature and
  # not working properly with googletest suite.
  if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
	set(gtest_disable_pthreads ON)
  endif()
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

  set(GTEST_HAS_ABSL OFF CACHE BOOL "" FORCE)
 
  # gtest and gmock
  FetchContent_Declare(
	googletest
	URL ${DEP_URL_googletest}
	URL_HASH SHA1=${DEP_SHA1_googletest}
	FIND_PACKAGE_ARGS 1.14.0...<2.0.0 NAMES GTest
  )
  FetchContent_MakeAvailable(googletest)
  #google benchmark doesn't work for Emscripten
  if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
      message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
	  # We will not need to test benchmark lib itself.
	  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing as we don't need it.")
	  # We will not need to install benchmark since we link it statically.
	  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable benchmark install to avoid overwriting vendor install.")
	  
	  FetchContent_Declare(
		google_benchmark
		URL ${DEP_URL_google_benchmark}
		URL_HASH SHA1=${DEP_SHA1_google_benchmark}
		FIND_PACKAGE_ARGS NAMES benchmark
	  )
	  onnxruntime_fetchcontent_makeavailable(google_benchmark)
  endif()
endif()