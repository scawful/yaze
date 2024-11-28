# Asar Assembler for 65816 SNES Assembly
add_subdirectory(src/lib/asar/src)

set(ASAR_GEN_EXE OFF)
set(ASAR_GEN_DLL ON)
set(ASAR_GEN_LIB ON)
set(ASAR_GEN_EXE_TEST OFF)
set(ASAR_GEN_DLL_TEST OFF)
set(ASAR_STATIC_SRC_DIR "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar")

get_target_property(ASAR_INCLUDE_DIR asar-static INCLUDE_DIRECTORIES)
list(APPEND ASAR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/lib/asar/src")
target_include_directories(asar-static PRIVATE ${ASAR_INCLUDE_DIR})

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

if(WIN32 OR MINGW)
  list(APPEND ASAR_STATIC_SRC "${ASAR_STATIC_SRC_DIR}/platform/windows/file-helpers-win32.cpp")
else()
  list(APPEND ASAR_STATIC_SRC "${ASAR_STATIC_SRC_DIR}/platform/linux/file-helpers-linux.cpp")
endif()