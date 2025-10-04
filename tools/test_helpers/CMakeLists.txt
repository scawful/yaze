# Add golden data extractor tool
add_executable(overworld_golden_data_extractor
  overworld_golden_data_extractor.cc
)

target_link_libraries(overworld_golden_data_extractor
  yaze_core
  ${CMAKE_THREAD_LIBS_INIT}
)

# Add vanilla values extractor tool
add_executable(extract_vanilla_values
  extract_vanilla_values.cc
)

target_link_libraries(extract_vanilla_values
  yaze_core
  ${CMAKE_THREAD_LIBS_INIT}
)

# Add rom_patch_utility tool
add_executable(rom_patch_utility
  rom_patch_utility.cc
)

target_link_libraries(rom_patch_utility
  yaze_core
  ${CMAKE_THREAD_LIBS_INIT}
)

# Windows stack size configuration for helper tools
set(HELPER_TOOLS
  overworld_golden_data_extractor
  extract_vanilla_values
  rom_patch_utility
)

foreach(TOOL ${HELPER_TOOLS})
  if(WIN32)
    if(MSVC)
      target_link_options(${TOOL} PRIVATE /STACK:16777216)
    elseif(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_link_options(${TOOL} PRIVATE -Wl,--stack,16777216)
    else()
      target_link_options(${TOOL} PRIVATE -Wl,--stack,16777216)
    endif()
  endif()
endforeach()

# Install tools to bin directory
install(TARGETS overworld_golden_data_extractor extract_vanilla_values rom_patch_utility
  DESTINATION bin
)
