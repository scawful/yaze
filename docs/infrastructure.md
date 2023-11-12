# YAZE Infrastructure Overview

For developers to reference.

## Directory Structure

- **.github/workflows**: Contains `yaze_test` workflow config.
- **assets**: Hosts assets like fonts.
- **cmake**: Contains CMake configurations.
- **docs**: Contains documentation for users and developers.
  - [Getting Started](./getting-started.md)
  - [LC_LZ2 Compression](./compression.md)
- **src**: Contains source files. 
  - **app**: Contains the GUI editor `yaze`
  - **cli**: Contains the command line interface `z3ed`
  - **lib**: Contains git submodule dependencies.
    - Abseil-cpp
    - Asar
    - ImGui
      - ImGuiFileDialog
      - ImGuiColorTextEdit
      - imgui_memory_editor
    - SDL2
- **test**: Contains testing interface `yaze_test`

## App Organization

- **Core Namespace**:
    - Contains fundamental functionalities.
      - [Common](../src/app/core/common.h)
      - [Constants](../src/app/core/constants.h)
      - [Controller](../src/app/core/controller.h)
      - [Editor](../src/app/core/editor.h)
      - [Pipeline](../src/app/core/pipeline.h)
- **Editor Namespace**:
  - Editors are responsible for representing the GUI view and handling user input.
  - These classes are all controlled by [MasterEditor](../src/app/editor/master_editor.h)
    - [DungeonEditor](../src/app/editor/dungeon_editor.h)
    - [GraphicsEditor](../src/app/editor/graphics_editor.h)
    - [MusicEditor](../src/app/editor/music_editor.h)
    - [OverworldEditor](../src/app/editor/overworld_editor.h)
    - [ScreenEditor](../src/app/editor/screen_editor.h)
    - [SpriteEditor](../src/app/editor/sprite_editor.h)
    - **Modules**
      - [AssemblyEditor](../src/app/editor/modules/assembly_editor.h)
      - [GfxGroupEditor](../src/app/editor/modules/gfx_group_editor.h)
      - [Tile16Editor](../src/app/editor/modules/tile16_editor.h)
- **Emu Namespace**:
    - Contains business logic for `core::emulator`
      - [Audio](../src/app/emu/audio/)
      - [Debug](../src/app/emu/debug/)
      - [Memory](../src/app/emu/memory/)
      - [Video](../src/app/emu/video/)
      - [Emulator](../src/app/emu/emulator.h)
- **Gfx Namespace**:
    - Handles graphics related tasks.
      - [Bitmap](../src/app/gfx/bitmap.h)
      - [Compression](../src/app/gfx/compression.h)
      - [SCAD Format](../src/app/gfx/scad_format.h)
      - [SNES Palette](../src/app/gfx/snes_palette.h)
      - [SNES Tile](../src/app/gfx/snes_tile.h)
- **Gui Namespace**:
    - Manages GUI elements.
      - [Canvas](../src/app/gui/canvas.h)
      - [Color](../src/app/gui/color.h)
      - [Icons](../src/app/gui/icons.h)
      - [Input](../src/app/gui/input.h)
      - [Style](../src/app/gui/style.h)
      - [Widgets](../src/app/gui/widgets.h)
- **Zelda3 Namespace**:
    - Holds business logic specific to Zelda3.
      - [Dungeon](../src/app/zelda3/dungeon/)
      - [Music](../src/app/zelda3/music/)
      - [Screen](../src/app/zelda3/screen/)
      - [Sprite](../src/app/zelda3/sprite/)
      - [OverworldMap](../src/app/zelda3/overworld_map.h)
      - [Overworld](../src/app/zelda3/overworld.h)

### Flow of Control

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
  - Handles the main menu bar.
    - File
      - Open - [app::ROM::LoadFromFile](../src/app/rom.cc#l=90)
      - Save - [app::ROM::SaveToFile](../src/app/rom.cc#l=301)
    - Edit
    - View
      - Emulator
      - HEX Editor
      - ASM Editor
      - Palette Editor
      - Memory Viewer
      - ImGui Demo
      - GUI Tools
        - Runtime Metrics
        - Style Editor
    - Help
  - Handles `absl::Status` errors as popups delivered to the user.
  - Update all the editors in a tab view.
    - [app/editor/assembly_editor.cc](../src/app/editor/assembly_editor.cc)
    - [app/editor/dungeon_editor.cc](../src/app/editor/dungeon_editor.cc)
    - [app/editor/graphics_editor.cc](../src/app/editor/graphics_editor.cc)
    - [app/editor/music_editor.cc](../src/app/editor/music_editor.cc)
    - [app/editor/overworld_editor.cc](../src/app/editor/overworld_editor.cc)
    - [app/editor/screen_editor.cc](../src/app/editor/screen_editor.cc)
    - [app/editor/sprite_editor.cc](../src/app/editor/sprite_editor.cc)

## ROM
- [app/rom.cc](../src/app/rom.cc)
- [app/rom.h](../src/app/rom.h)
---

This `ROM` class provides methods to manipulate and access data from a ROM.

- **Key Methods**:
  - `Load2BppGraphics()`: Loads 2BPP graphics data from specified sheets.
  - `LoadAllGraphicsData()`: Loads all graphics data, both compressed and uncompressed, converting where necessary.
  - `LoadFromFile(const absl::string_view& filename, bool z3_load)`: Loads ROM data from a file. It also handles headers and Zelda 3 specific data if requested.
  - `LoadFromPointer(uchar* data, size_t length)`: Loads ROM data from a provided pointer.
  - `LoadFromBytes(const Bytes& data)`: Loads ROM data from bytes.
  - `LoadAllPalettes()`: Loads all color palettes used in the ROM. This includes palettes for various elements like sprites, shields, swords, etc.
  - `UpdatePaletteColor(...)`: Updates a specific color within a named palette group.

- **Internal Data Structures**:
  - `rom_data_`: A container that holds the ROM data.
  - `graphics_bin_`: Holds the graphics data.
  - `palette_groups_`: A map containing various palette groups, each having its own set of color palettes.

- **Special Notes**:
  - The class interacts with various external functionalities, such as decompression algorithms (`gfx::DecompressV2`) and color conversion (`gfx::SnesTo8bppSheet`).
  - Headers in the ROM data, if present, are identified and removed.
  - Specific Zelda 3 data can be loaded if specified.
  - Palettes are categorized into multiple groups (e.g., `ow_main`, `ow_aux`, `hud`, etc.) and loaded accordingly.


## Overworld

- [app/zelda3/overworld.cc](../src/app/zelda3/overworld.cc)
- [app/zelda3/overworld.h](../src/app/zelda3/overworld.h)
- [app/zelda3/overworld_map.cc](../src/app/zelda3/overworld_map.cc)
- [app/zelda3/overworld_map.h](../src/app/zelda3/overworld_map.h)

---

- **Construction of Tile16 and Tile32**
- **Save and Load Resources**
  - Sprites
  - Entrances
  - Tilemaps

## Bitmap

- [app/gfx/bitmap.cc](../src/app/gfx/bitmap.cc)
- [app/gfx/bitmap.h](../src/app/gfx/bitmap.cc)
---

This class is responsible for creating, managing, and manipulating bitmap data, which can be displayed on the screen using the ImGui library.

### Key Attributes:

1. **Width, Height, Depth, and Data Size**: These represent the dimensions and data size of the bitmap. 
2. **Pixel Data**: Points to the raw data of the bitmap.
3. **Texture and Surface**: Use SDL to manage the graphical representation of the bitmap data. Both these attributes have custom deleters, ensuring proper resource management.

### Main Functions:

1. **Constructors**: Multiple constructors allow for different ways to create a Bitmap instance, like specifying width, height, depth, and data.
2. **Create**: This set of overloaded functions provides ways to create a bitmap from different data sources.
3. **CreateFromSurface**: Allows for the creation of a bitmap from an SDL_Surface.
4. **Apply**: Changes the bitmap's data to a new set of Bytes.
5. **Texture Operations**:
   - **CreateTexture**: Creates an SDL_Texture from the bitmap's data for rendering.
   - **UpdateTexture**: Updates the SDL_Texture with the latest bitmap data.
6. **SaveSurfaceToFile**: Saves the SDL_Surface to a file.
7. **SetSurface**: Assigns a new SDL_Surface to the bitmap.
8. **Palette Functions**:
   - **ApplyPalette (Overloaded)**: This allows for the application of a SNESPalette or a standard SDL_Color palette to the bitmap.
9. **WriteToPixel**: Directly writes a value to a specified position in the pixel data.

## Z3ED cli

| Command | Arg | Params | Status |
|---------|-----|--------|--------|
| Apply BPS Patch | -a | rom_file bps_file | In progress |
| Create BPS Patch | -c | bps_file src_file modified_file | Not started |
| Open ROM | -o | rom_file | Complete |
| Backup ROM | -b | rom_file [new_file] | In progress |
| Expand ROM | -x | rom_file file_size | Not started |
| Transfer Tile16 | -t | src_rom dest_rom tile32_id_list(csv) | Complete |
| Export Graphics | -e | rom_file bin_file | In progress |
| Import Graphics | -i | bin_file rom_file | Not started |
| SNES to PC Address | -s | address | Complete |
| PC to SNES Address | -p | address | Complete |


## Further Development Ideas
- Extend `zelda3` namespace with additional functionalities.
- Optimize program performance.
- Introduce new features in the GUI editor.
