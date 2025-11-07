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

| Feature | State | Notes |
|---|---|---|
| Overworld Editor | Stable | Supports vanilla and ZSCustomOverworld v2/v3 projects. |
| Dungeon Editor | Experimental | Requires extensive manual testing before production use. |
| Tile16 Editor | Experimental | Palette and tile workflows are still being tuned. |
| Palette Editor | Stable | Reference implementation for palette utilities. |
| Graphics Editor | Experimental | Rendering pipeline under active refactor. |
| Sprite Editor | Experimental | Card/UI patterns mid-migration. |
| Message Editor | Stable | Re-test after recent palette fixes. |
| Hex Editor | Stable | Direct ROM editing utility. |
| Asar Patching | Stable | Wraps the bundled Asar assembler. |

## Command-Line Interface (`z3ed`)

`z3ed` provides scripted access to the same ROM editors.

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

YAZE exports a C API that is still evolving. Treat it as experimental and expect breaking changes while the plugin system is built out.
