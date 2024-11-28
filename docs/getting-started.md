# Getting Started

This software allows you to modify "The Legend of Zelda: A Link to the Past"  (US or JP) ROMs. 

This editor is built to be compatible with ZScream projects and is designed to be cross platform.

Please note that this project is currently a work in progress, and some features may not be fully implemented or may be subject to change.

## General Tips

- Experiment flags determine whether certain features are enabled or not. To change your flags, go to `File` > `Options` > `Experiment Flags` or in the Settings tab.
- Backup files are enabled by default. Each save will produce a timestamped copy of your ROM before you last saved. You can disable this feature in the settings.

## Extending Functionality

In addition to the built-in features, this software provides a pure C library interface and a Python module that can be used for building extensions and custom sprites without assembly. In the editor these can be loaded under the `Extensions` menu.

This feature is still in development and is not yet fully documented.

## Supported Features 

| Feature | Status | Details |
|---------|--------|-------------|
| Overworld Maps | Done | Edit and save tile32 data. |
| Overworld Map Properties | Done | Edit and save map properties. |
| Overworld Entrances | Done | Edit and save entrance data. |
| Overworld Exits | Done | Edit and save exit data. |
| Overworld Sprites | In Progress | Edit sprite positions, add and remove sprites. |
| Tile16 Editing | Todo | Edit and save tile16 data. |
| Dungeon | In Progress | View dungeon room metadata and edit room data. |
| Palette | In Progress | Edit and save palettes, palette groups. |
| Graphics Sheets | In Progress | Edit and save graphics sheets. |
| Graphics Groups | Done | Edit and save graphics groups. |
| Sprite | Todo | View-only sprite data. |
| Custom Sprites | Todo | Edit and create custom sprite data. |
| Music | Todo | Edit music data. |
| Dungeon Maps | Todo | Edit dungeon maps. |
| Scad Format | Done-ish | Open and view scad files (SCR, CGX, COL) |
| Hex Editing | Done | View and edit ROM data in hex. |
| Asar Patching | In Progress | Apply Asar patches to your ROM or Project. |

## Command Line Interface

Included with the editor is a command line interface (CLI) that allows you to perform various operations on your ROMs from the command line. This aims to reduce the need for multiple tools in zelda3 hacking like Zcompress, LunarExpand, LunarAddress, Asar, and others.

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


