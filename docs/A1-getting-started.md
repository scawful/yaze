# Getting Started

This software allows you to modify "The Legend of Zelda: A Link to the Past" (US or JP) ROMs. It is built for compatibility with ZScream projects and designed to be cross-platform.

## Quick Start

1.  **Download** the latest release for your platform from the [releases page](https://github.com/scawful/yaze/releases).
2.  **Load ROM** via `File > Open ROM`.
3.  **Select an Editor** from the main toolbar (e.g., Overworld, Dungeon, Graphics).
4.  **Make Changes** and save your project.

## General Tips

-   **Experiment Flags**: Enable or disable new features in `File > Options > Experiment Flags`.
-   **Backup Files**: Enabled by default. Each save creates a timestamped backup of your ROM.
-   **Extensions**: Load custom tools via the `Extensions` menu (C library and Python module support is planned).

## Feature Status

| Feature | Status | Details |
|---|---|---|
| Overworld Editor | ✅ Complete | Full support for vanilla and ZSCustomOverworld v2/v3. |
| Dungeon Editor | ✅ Complete | Stable, component-based editor for rooms, objects, and sprites. |
| Tile16 Editor | ✅ Complete | Professional-grade tile editor with advanced palette management. |
| Palette Editor | ✅ Complete | Edit and save all SNES palette groups. |
| Graphics Editor | ✅ Complete | View and edit graphics sheets and groups. |
| Sprite Editor | ✅ Complete | Edit sprite properties and attributes. |
| Message Editor | ✅ Complete | Edit in-game text and dialogue. |
| Hex Editor | ✅ Complete | View and edit raw ROM data. |
| Asar Patching | ✅ Complete | Apply Asar 65816 assembly patches to the ROM. |

## Command-Line Interface (`z3ed`)

`z3ed` is a powerful, AI-driven CLI for inspecting and editing ROMs.

### AI Agent Chat
Chat with an AI to perform edits using natural language.

```bash
# Start an interactive chat session with the AI agent
z3ed agent chat --rom zelda3.sfc
```
> **Prompt:** "What sprites are in dungeon 2?"

### Resource Inspection
Directly query ROM data.

```bash
# List all sprites in the Eastern Palace (dungeon 2)
z3ed dungeon list-sprites --rom zelda3.sfc --dungeon 2

# Get information about a specific overworld map area
z3ed overworld describe-map --rom zelda3.sfc --map 80
```

### Patching
Apply assembly patches using the integrated Asar assembler.
```bash
# Apply an assembly patch to the ROM
z3ed asar patch.asm --rom zelda3.sfc
```

## Extending Functionality

YAZE is designed to be extensible. Future versions will support a full plugin architecture, allowing developers to create custom tools and editors. The C API, while available, is still under development.
