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

# Install tools to bin directory
install(TARGETS overworld_golden_data_extractor extract_vanilla_values
  DESTINATION bin
)
