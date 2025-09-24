# Asar Assembler for 65816 SNES Assembly
# Configure Asar build options
set(ASAR_GEN_EXE OFF)
set(ASAR_GEN_DLL ON)
set(ASAR_GEN_LIB ON)
set(ASAR_GEN_EXE_TEST OFF)
set(ASAR_GEN_DLL_TEST OFF)

# Add Asar subdirectory
add_subdirectory(src/lib/asar/src)

# Set up Asar include directories
set(ASAR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar")
set(ASAR_DLL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c")

# Define Asar static source files for cross-platform compatibility
set(ASAR_STATIC_SRC_DIR "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar")

set(ASAR_STATIC_SRC
  "${ASAR_STATIC_SRC_DIR}/interface-lib.cpp"
  "${ASAR_STATIC_SRC_DIR}/addr2line.cpp"
  "${ASAR_STATIC_SRC_DIR}/arch-65816.cpp"
  "${ASAR_STATIC_SRC_DIR}/arch-spc700.cpp"
  "${ASAR_STATIC_SRC_DIR}/arch-superfx.cpp"
  "${ASAR_STATIC_SRC_DIR}/assembleblock.cpp"
  "${ASAR_STATIC_SRC_DIR}/crc32.cpp"
  "${ASAR_STATIC_SRC_DIR}/libcon.cpp"
  "${ASAR_STATIC_SRC_DIR}/libsmw.cpp"
  "${ASAR_STATIC_SRC_DIR}/libstr.cpp"
  "${ASAR_STATIC_SRC_DIR}/macro.cpp"
  "${ASAR_STATIC_SRC_DIR}/main.cpp"
  "${ASAR_STATIC_SRC_DIR}/asar_math.cpp"
  "${ASAR_STATIC_SRC_DIR}/virtualfile.cpp"
  "${ASAR_STATIC_SRC_DIR}/warnings.cpp"
  "${ASAR_STATIC_SRC_DIR}/errors.cpp"
  "${ASAR_STATIC_SRC_DIR}/platform/file-helpers.cpp"
)

# Add platform-specific source files
if(WIN32 OR MINGW)
  list(APPEND ASAR_STATIC_SRC "${ASAR_STATIC_SRC_DIR}/platform/windows/file-helpers-win32.cpp")
elseif(UNIX)
  list(APPEND ASAR_STATIC_SRC "${ASAR_STATIC_SRC_DIR}/platform/linux/file-helpers-linux.cpp")
else()
  # macOS and other platforms
  list(APPEND ASAR_STATIC_SRC "${ASAR_STATIC_SRC_DIR}/platform/generic/file-helpers-generic.cpp")
endif()

# Set up Asar targets and properties
if(TARGET asar-static)
  target_include_directories(asar-static PUBLIC 
    ${ASAR_INCLUDE_DIR}
    ${ASAR_DLL_INCLUDE_DIR}
  )
  
  # Set compile definitions for cross-platform compatibility
  if(WIN32)
    target_compile_definitions(asar-static PRIVATE "windows")
  elseif(UNIX)
    target_compile_definitions(asar-static PRIVATE "linux")
  endif()
  
  # Set C++ standard
  target_compile_features(asar-static PRIVATE cxx_std_11)
endif()