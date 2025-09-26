# Getting Started

This software allows you to modify "The Legend of Zelda: A Link to the Past" (US or JP) ROMs. Built for compatibility with ZScream projects and designed to be cross-platform.

## Quick Start

1. **Download** the latest release for your platform
2. **Load ROM** via File > Open ROM
3. **Select Editor** from the toolbar (Overworld, Dungeon, Graphics, etc.)
4. **Make Changes** and save your project

## General Tips

- **Experiment Flags**: Enable/disable features in File > Options > Experiment Flags
- **Backup Files**: Enabled by default - each save creates a timestamped backup
- **Extensions**: Load custom tools via the Extensions menu (C library and Python module support)

## Supported Features

| Feature | Status | Details |
|---------|--------|---------|
| Overworld Maps | ✅ Complete | Edit and save tile32 data |
| OW Map Properties | ✅ Complete | Edit and save map properties |
| OW Entrances | ✅ Complete | Edit and save entrance data |
| OW Exits | ✅ Complete | Edit and save exit data |
| OW Sprites | 🔄 In Progress | Edit sprite positions, add/remove sprites |
| Dungeon Editor | 🔄 In Progress | View room metadata and edit room data |
| Palette Editor | 🔄 In Progress | Edit and save palettes, palette groups |
| Graphics Sheets | 🔄 In Progress | Edit and save graphics sheets |
| Graphics Groups | ✅ Complete | Edit and save graphics groups |
| Hex Editor | ✅ Complete | View and edit ROM data in hex |
| Asar Patching | ✅ Complete | Apply Asar 65816 assembly patches to ROM |

## Command Line Interface

The `z3ed` CLI tool provides ROM operations:

```bash
# Apply Asar assembly patch
z3ed asar patch.asm --rom=zelda3.sfc

# Extract symbols from assembly
z3ed extract patch.asm

# Validate assembly syntax
z3ed validate patch.asm

# Launch interactive TUI
z3ed --tui
```

## Extending Functionality

YAZE provides a pure C library interface and Python module for building extensions and custom sprites without assembly. Load these under the Extensions menu.

This feature is still in development and not fully documented yet.
