# Infrastructure Overview

For developers to reference.

## Z3ED cli

| Command | Arg | Params | Status |
|---------|-----|--------|--------|
| Apply BPS Patch | -a | rom_file bps_file | In progress |
| Create BPS Patch | -c | bps_file src_file modified_file | Not started |
| Asar Patch | -asar | asm_file rom_file | In progress |
| Open ROM | -o | rom_file | Complete |
| Backup ROM | -b | rom_file [new_file] | In progress |
| Expand ROM | -x | rom_file file_size | Not started |
| Transfer Tile16 | -t | src_rom dest_rom tile32_id_list(csv) | Complete |
| Export Graphics | -e | rom_file bin_file | In progress |
| Import Graphics | -i | bin_file rom_file | Not started |
| SNES to PC Address | -s | address | Complete |
| PC to SNES Address | -p | address | Complete |

## Targets

- **yaze**: Desktop application for Windows/macOS/Linux
- **z3ed**: Command Line Interface
- **yaze_c**: C Library
- **yaze_py**: Python Module
- **yaze_ext**: Extensions library
- **yaze_test**: Unit test executable

## Directory Structure

- **assets**: Hosts assets like fonts, icons, assembly source, etc.
- **cmake**: Contains CMake configurations.
- **docs**: Contains documentation for users and developers.
- **src**: Contains source files. 
  - **app**:  Contains the GUI editor `yaze`
  - **base**: Contains the base data headers for `yaze_c`
  - **cli**:  Contains the command line interface `z3ed`
  - **ext**:  Contains the extensions library `yaze_ext`
  - **py**:   Contains the Python module `yaze_py`
  - **lib**:  Contains the dependencies as git submodules
- **test**: Contains testing interface `yaze_test`

## Flow of Control

- [app/yaze.cc](../src/app/yaze.cc) 
  - Initializes `absl::FailureSignalHandler` for stack tracing.
  - Runs the `core::Controller` loop.
- [app/core/controller.cc](../src/app/core/controller.cc)
  - Initializes SDLRenderer and SDLWindow
  - Initializes ImGui, fonts, themes, and clipboard.
  - Handles user input from keyboard and mouse.
  - Updates `editor::MasterEditor`
  - Renders the output to the screen.
  - Handles the teardown of SDL and ImGui resources.
- [app/editor/master_editor.cc](../src/app/editor/master_editor.cc)
  - Handles the main menu bar
  - Handles `absl::Status` errors as popups delivered to the user.
  - Update all the editors in a tab view.
    - [app/editor/assembly_editor.cc](../src/app/editor/assembly_editor.cc)
    - [app/editor/dungeon_editor.cc](../src/app/editor/dungeon_editor.cc)
    - [app/editor/graphics_editor.cc](../src/app/editor/graphics_editor.cc)
    - [app/editor/music_editor.cc](../src/app/editor/music_editor.cc)
    - [app/editor/overworld_editor.cc](../src/app/editor/overworld_editor.cc)
    - [app/editor/screen_editor.cc](../src/app/editor/screen_editor.cc)
    - [app/editor/sprite_editor.cc](../src/app/editor/sprite_editor.cc)

## Rom

- [app/rom.cc](../src/app/rom.cc)
- [app/rom.h](../src/app/rom.h)

The `Rom` class provides methods to manipulate and access data from a ROM.

## Bitmap

- [app/gfx/bitmap.cc](../src/app/gfx/bitmap.cc)
- [app/gfx/bitmap.h](../src/app/gfx/bitmap.cc)

This class is responsible for creating, managing, and manipulating bitmap data, which can be displayed on the screen using SDL2 Textures and the ImGui draw list.

