# YAZE Infrastructure Overview

For developers to reference.

## Directory Structure

- **.github/workflows**: Contains workflow configuration for running yaze_test.
- **assets**: Hosts assets like fonts.
- **cmake**: Contains CMake configurations.
- **docs**: Stores documentation.
- **src**: Source files.
- **test**: Contains test files and configurations.

## Main Components

- `app` Namespace: Represents the GUI editor YAZE.
- `cli` Namespace: Represents the command line interface Z3ED.

## YAZE app

- **Core Namespace**:
    - Contains fundamental functionalities.
- **Editor Namespace**:
    - Represents the GUI view.
    - Contains a class holding objects such as `zelda3::Overworld` and `gfx::Bitmap` for rendering and user input handling.
- **Gfx Namespace**:
    - Handles graphics-related tasks.
- **Gui Namespace**:
    - Manages GUI elements.
- **Zelda3 Namespace**:
    - Holds business logic specific to Zelda3.

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
