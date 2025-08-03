# Infrastructure Overview

For developers to reference.

The goal of yaze is to build a cross platform editor for the Legend of Zelda: A Link to the Past. The project is built using C++20, SDL2, and ImGui. The project is built using CMake and is designed to be modular and extensible. The project is designed to be built on Windows, macOS, iOS, and Linux.

## Targets

- **yaze**: Desktop application for Windows/macOS/Linux
- **z3ed**: Command Line Interface
- **yaze_c**: C Library
- **yaze_test**: Unit test executable
- **yaze_ios**: iOS application

## Directory Structure

- **assets**: Hosts assets like fonts, icons, assembly source, etc.
- **cmake**: Contains CMake configurations.
- **docs**: Contains documentation for users and developers.
- **incl**: Contains the public headers for `yaze_c`
- **src**: Contains source files.
  - **app**:  Contains the GUI editor `yaze`
  - **app/emu**:  Contains a standalone Snes emulator application `yaze_emu`
  - **cli**:  Contains the command line interface `z3ed`
  - **ios**:  Contains the iOS application `yaze_ios`
  - **lib**:  Contains the dependencies as git submodules
  - **test**: Contains testing interface `yaze_test`
  - **win32**: Contains Windows resource file and icon

## Dependencies

See [build-instructions.md](docs/build-instructions.md) for more information.

- **SDL2**: Graphics library
- **ImGui**: GUI library
- **Abseil**: C++ library
- **libpng**: Image library

## Flow of Control

- app/main.cc
  - Initializes `absl::FailureSignalHandler` for stack tracing.
  - Runs the `core::Controller` loop.
- app/core/controller.cc
  - Initializes SDLRenderer and SDLWindow
  - Initializes ImGui, fonts, themes, and clipboard.
  - Handles user input from keyboard and mouse.
  - Renders the output to the screen.
  - Handles the teardown of SDL and ImGui resources.
- app/editor/editor_manager.cc
  - Handles the main menu bar
  - Handles `absl::Status` errors as popups delivered to the user.
  - Dispatches messages to the various editors.
  - Update all the editors in a tab view.
    - app/editor/code/assembly_editor.cc
    - app/editor/dungeon/dungeon_editor.cc
    - app/editor/graphics/graphics_editor.cc
    - app/editor/graphics/gfx_group_editor.cc
    - app/editor/graphics/palette_editor.cc
    - app/editor/graphics/tile16_editor.cc
    - app/editor/message/message_editor.cc
    - app/editor/music/music_editor.cc
    - app/editor/overworld/overworld_editor.cc
    - app/editor/graphics/screen_editor.cc
    - app/editor/sprite/sprite_editor.cc
    - app/editor/system/settings_editor.cc

## Rom

- app/rom.cc
- app/rom.h

The Rom class provides methods to manipulate and access data from a ROM.

## Bitmap

- app/gfx/bitmap.cc
- app/gfx/bitmap.h

This class is responsible for creating, managing, and manipulating bitmap data, which can be displayed on the screen using SDL2 Textures and the ImGui draw list. It also provides functions for exporting these bitmaps to the clipboard in PNG format using libpng.
