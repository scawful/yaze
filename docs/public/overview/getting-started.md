# Getting Started with YAZE

YAZE is a ROM editor for "The Legend of Zelda: A Link to the Past" (US and JP versions). It provides a full-featured GUI editor, integrated SNES emulator, and AI-powered command-line tools.

---

## Quick Start

1. **Download** the latest release for your platform from the [GitHub Releases page](https://github.com/scawful/yaze/releases)
2. **Launch** the application and load your ROM via `File > Open ROM`
3. **Choose an Editor** from the toolbar (Overworld, Dungeon, Graphics, etc.)
4. **Edit** your ROM and save your changes

> **Building from source?** See the [Build and Test Quick Reference](../build/quick-reference.md).

---

## Tips

- **Backups**: Automatic backups are enabled by default. Each save creates a timestamped backup.
- **Experiment Flags**: Try new features via `File > Options > Experiment Flags`.
- **Extensions**: Load custom tools from the `Extensions` menu (plugin system under development).

---

## Editor Status

| Editor | Status | Notes |
|--------|--------|-------|
| Overworld | Stable | Full support for vanilla and ZSCustomOverworld v2/v3 |
| Dungeon | Stable | Room editing, objects, sprites, palettes |
| Palette | Stable | Reference implementation for palette utilities |
| Message | Stable | Text and dialogue editing |
| Hex | Stable | Direct ROM byte editing |
| Asar Patching | Stable | Integrated Asar assembler |
| Graphics | Stable | Tile and sprite graphics editing |
| Sprite | Stable | Vanilla and custom sprite editing |
| Music | Experimental | Tracker and instrument editing |

---

## Command-Line Interface (z3ed)

The `z3ed` CLI provides scriptable access to ROM editing capabilities.

### AI Chat

```bash
z3ed agent simple-chat --rom=zelda3.sfc
```
Example prompt: "What sprites are in room 1?"

### ROM Inspection

```bash
# List sprites in a dungeon room (room 1 = Eastern Palace)
z3ed dungeon-list-sprites --room=1 --rom=zelda3.sfc

# Describe overworld map
z3ed overworld-describe-map --map=80 --rom=zelda3.sfc

# Search messages
z3ed message-search --query="Master Sword" --rom=zelda3.sfc
```

For more details, see the [z3ed CLI Guide](../usage/z3ed-cli.md).

---

## Next Steps

- **[Dungeon Editor Guide](../usage/dungeon-editor.md)** - Learn dungeon room editing
- **[z3ed CLI Guide](../usage/z3ed-cli.md)** - Master the command-line interface
- **[Architecture Overview](../developer/architecture.md)** - Understand the codebase
