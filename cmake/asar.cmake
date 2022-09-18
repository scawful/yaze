get_target_property(ASAR_INCLUDE_DIR asar-static INCLUDE_DIRECTORIES)
target_include_directories(asar-static PRIVATE ${ASAR_INCLUDE_DIR})
set(ASAR_GEN_EXE OFF)
set(ASAR_GEN_DLL ON)
set(ASAR_GEN_LIB ON)
set(ASAR_GEN_EXE_TEST OFF)
set(ASAR_GEN_DLL_TEST OFF)

set(ASAR_STATIC_SRC
  "../src/lib/asar/src/asar/interface-lib.cpp"
  "../src/lib/asar/src/asar/addr2line.cpp"
  "../src/lib/asar/src/asar/arch-65816.cpp"
  "../src/lib/asar/src/asar/arch-spc700.cpp"
  "../src/lib/asar/src/asar/arch-superfx.cpp"
  "../src/lib/asar/src/asar/assembleblock.cpp"
  "../src/lib/asar/src/asar/crc32.cpp"
  "../src/lib/asar/src/asar/libcon.cpp"
  "../src/lib/asar/src/asar/libsmw.cpp"
  "../src/lib/asar/src/asar/libstr.cpp"
  "../src/lib/asar/src/asar/macro.cpp"
  "../src/lib/asar/src/asar/main.cpp"
  "../src/lib/asar/src/asar/asar_math.cpp"
  "../src/lib/asar/src/asar/virtualfile.cpp"
  "../src/lib/asar/src/asar/warnings.cpp"
  "../src/lib/asar/src/asar/errors.cpp"
  "../src/lib/asar/src/asar/platform/file-helpers.cpp"
)

if(WIN32 OR MINGW)
  list(APPEND ASAR_STATIC_SRC "../src/lib/asar/src/asar/platform/windows/file-helpers-win32.cpp")
else()
  list(APPEND ASAR_STATIC_SRC "../src/lib/asar/src/asar/platform/linux/file-helpers-linux.cpp")
endif()