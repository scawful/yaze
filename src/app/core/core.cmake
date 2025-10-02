set(
  YAZE_APP_CORE_SRC
  app/core/controller.cc
  app/emu/emulator.cc
  app/core/project.cc
  app/core/window.cc
  app/core/asar_wrapper.cc
)

if (WIN32 OR MINGW OR (UNIX AND NOT APPLE))
  list(APPEND YAZE_APP_CORE_SRC
    app/core/platform/font_loader.cc
    app/core/platform/file_dialog.cc
  )
endif()

if(APPLE)
    list(APPEND YAZE_APP_CORE_SRC
      app/core/platform/file_dialog.cc
      app/core/platform/file_dialog.mm
      app/core/platform/app_delegate.mm
      app/core/platform/font_loader.cc
      app/core/platform/font_loader.mm
    )

    find_library(COCOA_LIBRARY Cocoa)
    if(NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa not found")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "-framework ServiceManagement -framework Foundation -framework Cocoa")
endif()
