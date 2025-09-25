# Infrastructure Overview

For developers to reference.

The goal of yaze is to build a cross platform editor for the Legend of Zelda: A Link to the Past. The project is built using C++23, SDL2, ImGui, and Asar 65816 assembler. The project uses modern CMake 3.16+ and is designed to be modular and extensible. The project supports Windows, macOS, and Linux with professional packaging and CI/CD.

## Targets

- **yaze**: Desktop application with GUI docking system (Windows/macOS/Linux)
- **z3ed**: Enhanced command line interface with Asar integration and TUI
- **yaze_c**: C Library for custom tools and extensions
- **yaze_test**: Comprehensive unit test executable with ROM-dependent test separation
- **yaze_emu**: Standalone SNES emulator application
- **yaze_ios**: iOS application (coming in future release)

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

### Core Dependencies
- **SDL2**: Graphics and input library
- **ImGui**: Immediate mode GUI library with docking support
- **Abseil**: Modern C++ utilities library
- **libpng**: Image processing library

### New in v0.3.0
- **Asar**: 65816 assembler for ROM patching and symbol extraction
- **ftxui**: Terminal UI library for enhanced CLI experience
- **GoogleTest/GoogleMock**: Comprehensive testing framework

### Build System
- **CMake 3.16+**: Modern build system with target-based configuration
- **CMakePresets**: Development workflow presets
- **Cross-platform CI/CD**: GitHub Actions for automated builds and testing

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
