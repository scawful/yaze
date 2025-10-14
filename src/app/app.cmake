# This file defines the main `yaze` application executable.

if (APPLE)
  add_executable(yaze MACOSX_BUNDLE app/main.cc ${YAZE_RESOURCE_FILES})
  
  set(ICON_FILE "${CMAKE_SOURCE_DIR}/assets/yaze.icns")
  target_sources(yaze PRIVATE ${ICON_FILE})
  set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  
  set_target_properties(yaze PROPERTIES
    MACOSX_BUNDLE_ICON_FILE "yaze.icns"
    MACOSX_BUNDLE_BUNDLE_NAME "Yaze"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.scawful.yaze"
    MACOSX_BUNDLE_INFO_STRING "Yet Another Zelda3 Editor"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  )
else()
  add_executable(yaze app/main.cc)
  if(WIN32 OR UNIX)
    target_sources(yaze PRIVATE ${YAZE_RESOURCE_FILES})
  endif()
endif()

target_include_directories(yaze PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/incl
  ${PROJECT_BINARY_DIR}
)

target_sources(yaze PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h PROPERTIES GENERATED TRUE)

# Link modular libraries
target_link_libraries(yaze PRIVATE 
  yaze_editor 
  yaze_emulator 
  yaze_agent
  absl::failure_signal_handler
  absl::flags
  absl::flags_parse
)
if(YAZE_WITH_GRPC AND YAZE_PROTOBUF_TARGET)
  target_link_libraries(yaze PRIVATE ${YAZE_PROTOBUF_TARGET})
endif()

# Link test support library (yaze_editor needs TestManager)
if(TARGET yaze_test_support)
  target_link_libraries(yaze PRIVATE yaze_test_support)
  message(STATUS "✓ yaze executable linked to yaze_test_support")
else()
  message(WARNING "yaze needs yaze_test_support but TARGET not found")
endif()

# Platform-specific settings
if(WIN32)
  if(MSVC)
    target_link_options(yaze PRIVATE /STACK:8388608 /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  elseif(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(yaze PRIVATE -Wl,--stack,8388608 -Wl,--subsystem,windows -Wl,-emain)
  endif()
endif()

if(APPLE)
  target_link_libraries(yaze PUBLIC "-framework Cocoa")
endif()

# Post-build asset copying for non-macOS platforms
if(NOT APPLE)
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/font $<TARGET_FILE_DIR:yaze>/assets/font
    COMMENT "Copying font assets"
  )
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/themes $<TARGET_FILE_DIR:yaze>/assets/themes
    COMMENT "Copying theme assets"
  )
  if(EXISTS ${CMAKE_SOURCE_DIR}/assets/agent)
    add_custom_command(TARGET yaze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/agent $<TARGET_FILE_DIR:yaze>/assets/agent
      COMMENT "Copying agent assets"
    )
  endif()
endif()
