# cmake/toolchains/homebrew-llvm.toolchain.cmake
#
# CMake Toolchain File for using the Homebrew LLVM/Clang installation on macOS. 
# This ensures that the main project and all dependencies (like gRPC) use the
# correct compiler and header search paths.

# 1. Set the target system (macOS)
set(CMAKE_SYSTEM_NAME Darwin)

# Ensure a non-empty system version for third-party CMake logic.
if(NOT CMAKE_SYSTEM_VERSION)
  execute_process(
    COMMAND sw_vers -productVersion
    OUTPUT_VARIABLE _yaze_macos_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(_yaze_macos_version)
    set(CMAKE_SYSTEM_VERSION "${_yaze_macos_version}")
  else()
    set(CMAKE_SYSTEM_VERSION "0")
  endif()
endif()

# 2. Find the Homebrew LLVM installation path
# We use execute_process to make this portable across machine architectures.
set(_yaze_llvm_candidates llvm@21 llvm@20 llvm@19 llvm@18 llvm)
foreach(_yaze_llvm_candidate IN LISTS _yaze_llvm_candidates)
  execute_process(
    COMMAND brew --prefix ${_yaze_llvm_candidate}
    OUTPUT_VARIABLE _yaze_llvm_prefix
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _yaze_llvm_result
  )
  if(_yaze_llvm_result EQUAL 0 AND EXISTS "${_yaze_llvm_prefix}")
    set(HOMEBREW_LLVM_PREFIX "${_yaze_llvm_prefix}")
    break()
  endif()
endforeach()

if(NOT EXISTS "${HOMEBREW_LLVM_PREFIX}")
  message(FATAL_ERROR "Homebrew LLVM not found. Please run 'brew install llvm'.")
endif()

# Cache this variable so it's available in the main CMakeLists.txt
set(HOMEBREW_LLVM_PREFIX "${HOMEBREW_LLVM_PREFIX}" CACHE PATH "Path to Homebrew LLVM installation")

message(STATUS "Using Homebrew LLVM from: ${HOMEBREW_LLVM_PREFIX}")

# 3. Set the C and C++ compilers
set(CMAKE_C_COMPILER "${HOMEBREW_LLVM_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${HOMEBREW_LLVM_PREFIX}/bin/clang++")

# 3.5 Find and configure clang-tidy
find_program(CLANG_TIDY_EXE 
  NAMES clang-tidy 
  HINTS "${HOMEBREW_LLVM_PREFIX}/bin" 
  NO_DEFAULT_PATH
)

if(CLANG_TIDY_EXE)
  message(STATUS "Found Homebrew clang-tidy: ${CLANG_TIDY_EXE}")
  set(YAZE_CLANG_TIDY_EXE "${CLANG_TIDY_EXE}" CACHE FILEPATH "Path to clang-tidy executable")
else()
  message(WARNING "clang-tidy not found in ${HOMEBREW_LLVM_PREFIX}/bin")
endif()

# 4. Set the system root (sysroot) to the macOS SDK
# This correctly points to the system-level headers and libraries.
execute_process(
  COMMAND xcrun --show-sdk-path
  OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_SYSROOT "${CMAKE_OSX_SYSROOT}")
message(STATUS "Using macOS SDK at: ${CMAKE_SYSROOT}")

# 5. Ensure Homebrew libc++ + Clang resource headers are searched before SDK headers.
execute_process(
  COMMAND "${HOMEBREW_LLVM_PREFIX}/bin/clang++" -print-resource-dir
  OUTPUT_VARIABLE HOMEBREW_LLVM_RESOURCE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(HOMEBREW_LLVM_RESOURCE_INCLUDE "${HOMEBREW_LLVM_RESOURCE_DIR}/include")

if(EXISTS "${HOMEBREW_LLVM_PREFIX}/include/c++/v1")
  set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
  include_directories(BEFORE SYSTEM "${HOMEBREW_LLVM_PREFIX}/include/c++/v1")
endif()
if(EXISTS "${HOMEBREW_LLVM_RESOURCE_INCLUDE}")
  include_directories(BEFORE SYSTEM "${HOMEBREW_LLVM_RESOURCE_INCLUDE}")
endif()

# 5.5 Ensure Homebrew libc++ is linked to avoid mixing ABI with system libc++.
set(_yaze_llvm_lib_dir "${HOMEBREW_LLVM_PREFIX}/lib")
set(_yaze_llvm_libcxx_dir "${HOMEBREW_LLVM_PREFIX}/lib/c++")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -L${_yaze_llvm_lib_dir} -Wl,-rpath,${_yaze_llvm_lib_dir} -L${_yaze_llvm_libcxx_dir} -Wl,-rpath,${_yaze_llvm_libcxx_dir}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} -L${_yaze_llvm_lib_dir} -Wl,-rpath,${_yaze_llvm_lib_dir} -L${_yaze_llvm_libcxx_dir} -Wl,-rpath,${_yaze_llvm_libcxx_dir}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} -L${_yaze_llvm_lib_dir} -Wl,-rpath,${_yaze_llvm_lib_dir} -L${_yaze_llvm_libcxx_dir} -Wl,-rpath,${_yaze_llvm_libcxx_dir}")

# 6. Set the default installation path for macOS frameworks
set(CMAKE_FIND_FRAMEWORK FIRST)
