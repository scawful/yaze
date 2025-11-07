# YAZE Startup Debugging Flags

This guide explains how to use command-line flags to quickly open specific editors and cards during development for faster debugging workflows.

## Basic Usage

```bash
./yaze [flags]
```

## Available Flags

### `--rom_file`
Load a specific ROM file on startup.

```bash
./yaze --rom_file=/path/to/zelda3.sfc
```

### `--debug`
Enable debug logging with verbose output.

```bash
./yaze --debug --log_file=yaze_debug.log
```

### `--editor`
Open a specific editor on startup. This saves time by skipping manual navigation through the UI.

**Available editors:**
- `Assembly` - Assembly code editor
- `Dungeon` - Dungeon/underworld editor
- `Graphics` - Graphics and tile editor
- `Music` - Music and sound editor
- `Overworld` - Overworld map editor
- `Palette` - Palette editor
- `Screen` - Screen editor
- `Sprite` - Sprite editor
- `Message` - Message/text editor
- `Hex` - Hex/memory editor
- `Agent` - AI agent interface
- `Settings` - Settings editor

**Example:**
```bash
./yaze --rom_file=zelda3.sfc --editor=Dungeon
```

### `--cards`
Open specific cards/panels within an editor. Most useful with the Dungeon editor.

**Dungeon Editor Cards:**
- `Rooms List` - Shows the list of all dungeon rooms
- `Room Matrix` - Shows the dungeon room layout matrix
- `Entrances List` - Shows dungeon entrance configurations
- `Room Graphics` - Shows room graphics settings
- `Object Editor` - Shows the object placement editor
- `Palette Editor` - Shows the palette editor
- `Room N` - Opens a specific room by ID (e.g., `Room 0`, `Room 105`)

**Example:**
```bash
./yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Rooms List,Room 0"
```

## Common Debugging Scenarios

### 1. Quick Dungeon Room Testing
Open a specific dungeon room for testing:

```bash
./yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0,Room Graphics"
```

### 2. Multiple Room Comparison
Compare multiple rooms side-by-side:

```bash
./yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0,Room 1,Room 105"
```

### 3. Full Dungeon Editor Workspace
Open all dungeon editor tools:

```bash
./yaze --rom_file=zelda3.sfc --editor=Dungeon \
  --cards="Rooms List,Room Matrix,Room Graphics,Object Editor,Palette Editor"
```

### 4. Debug Mode with Logging
Enable full debug output while working:

```bash
./yaze --rom_file=zelda3.sfc --debug --log_file=debug.log \
  --editor=Dungeon --cards="Room 0"
```

### 5. Quick Overworld Editing
Jump straight to overworld editing:

```bash
./yaze --rom_file=zelda3.sfc --editor=Overworld
```

## gRPC Test Harness (Developer Feature)

If compiled with `YAZE_WITH_GRPC=ON`, you can enable automated GUI testing:

```bash
./yaze --enable_test_harness --test_harness_port=50051
```

This allows remote control via gRPC for automated testing and AI agent interaction.

## Combining Flags

All flags can be combined for powerful debugging setups:

```bash
# Full debugging setup for room 105
./yaze \
  --rom_file=/path/to/zelda3.sfc \
  --debug \
  --log_file=room_105_debug.log \
  --editor=Dungeon \
  --cards="Room 105,Room Graphics,Palette Editor,Object Editor"
```

## Notes

- Card names are case-sensitive and must match exactly
- Use quotes around comma-separated card lists
- Invalid editor or card names will be logged as warnings but won't crash the application
- The `--cards` flag is currently only implemented for the Dungeon editor
- Room IDs range from 0-319 in the vanilla game

## Troubleshooting

**Editor doesn't open:**
- Check spelling (case-sensitive)
- Verify ROM loaded successfully
- Check log output with `--debug`

**Cards don't appear:**
- Ensure editor is set (e.g., `--editor=Dungeon`)
- Check card name spelling
- Some cards require a loaded ROM

**Want to add more card support?**
See `EditorManager::OpenEditorAndCardsFromFlags()` in `src/app/editor/editor_manager.cc`

