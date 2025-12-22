# cmake/toolchains/homebrew-llvm.toolchain.cmake
#
# CMake Toolchain File for using the Homebrew LLVM/Clang installation on macOS. 
# This ensures that the main project and all dependencies (like gRPC) use the
# correct compiler and header search paths.

# 1. Set the target system (macOS)
set(CMAKE_SYSTEM_NAME Darwin)

# 2. Find the Homebrew LLVM installation path
# We use execute_process to make this portable across machine architectures.
execute_process(
  COMMAND brew --prefix llvm@18
  OUTPUT_VARIABLE HOMEBREW_LLVM_PREFIX
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT EXISTS "${HOMEBREW_LLVM_PREFIX}")
  message(FATAL_ERROR "Homebrew LLVM not found. Please run 'brew install llvm'. Path: ${HOMEBREW_LLVM_PREFIX}")
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

# 5. **[THE CRITICAL FIX]** Explicitly define the C++ standard library include directory.
# This forces CMake to add Homebrew's libc++ headers to the search path *before*
# any other system paths, resolving the header conflict for the main project
  # and all dependencies.
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${HOMEBREW_LLVM_PREFIX}/include/c++/v1")

# 6. Set the default installation path for macOS frameworks
set(CMAKE_FIND_FRAMEWORK FIRST)