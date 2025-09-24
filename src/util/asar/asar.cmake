# Asar Integration Library
set(ASAR_INTEGRATION_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/util/asar/asar_integration.cc
)

set(ASAR_INTEGRATION_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/util/asar/asar_integration.h
)

# Create the Asar integration library
add_library(yaze_asar_integration STATIC ${ASAR_INTEGRATION_SRC})

target_include_directories(yaze_asar_integration PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
  ${ASAR_INCLUDE_DIR}
  ${ASAR_DLL_INCLUDE_DIR}
)

target_link_libraries(yaze_asar_integration PUBLIC
  asar-static
  absl::status
  absl::statusor
  absl::strings
)

# Set compile definitions
target_compile_definitions(yaze_asar_integration PRIVATE
  ASAR_STATIC
)

# Set C++ standard
target_compile_features(yaze_asar_integration PRIVATE cxx_std_17)