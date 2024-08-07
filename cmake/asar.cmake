get_target_property(ASAR_INCLUDE_DIR asar-static INCLUDE_DIRECTORIES)
target_include_directories(asar-static PRIVATE ${ASAR_INCLUDE_DIR})
set(ASAR_GEN_EXE OFF)
set(ASAR_GEN_DLL ON)
set(ASAR_GEN_LIB ON)
set(ASAR_GEN_EXE_TEST OFF)
set(ASAR_GEN_DLL_TEST OFF)

set(ASAR_STATIC_SRC
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/interface-lib.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/addr2line.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/arch-65816.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/arch-spc700.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/arch-superfx.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/assembleblock.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/crc32.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/libcon.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/libsmw.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/libstr.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/macro.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/main.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/asar_math.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/virtualfile.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/warnings.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/errors.cpp"
  "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/platform/file-helpers.cpp"
)

if(WIN32 OR MINGW)
  list(APPEND ASAR_STATIC_SRC "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/platform/windows/file-helpers-win32.cpp")
else()
  list(APPEND ASAR_STATIC_SRC "${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar/platform/linux/file-helpers-linux.cpp")
endif()