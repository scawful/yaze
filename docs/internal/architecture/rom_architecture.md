# ROM Architecture

This document describes the decoupled ROM architecture that separates generic SNES ROM handling from Zelda3-specific game data.

## Overview

The ROM system is split into two main components:

1. **`src/rom/`** - Generic SNES ROM container (game-agnostic)
2. **`src/zelda3/game_data.h`** - Zelda3-specific data structures

This separation enables:
- Cleaner code organization with single-responsibility modules
- Easier testing with mock ROMs
- Future support for other SNES games
- Better encapsulation of game-specific logic

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│  (EditorManager, Editors, CLI)                              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    zelda3::GameData                          │
│  - palette_groups (dungeon, overworld, sprites, etc.)       │
│  - graphics_buffer (raw graphics data)                      │
│  - gfx_bitmaps (rendered graphics sheets)                   │
│  - blockset/spriteset/paletteset IDs                        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                         Rom                                  │
│  - LoadFromFile() / LoadFromData()                          │
│  - ReadByte() / WriteByte() / ReadWord() / WriteByte()      │
│  - ReadTransaction() / WriteTransaction()                    │
│  - Save()                                                    │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### Rom Class (`src/rom/rom.h`)

The `Rom` class is a generic SNES ROM container with no game-specific logic:

```cpp
class Rom {
 public:
  // Loading
  absl::Status LoadFromFile(const std::string& filename);
  absl::Status LoadFromData(const std::vector<uint8_t>& data);

  // Byte-level access
  absl::StatusOr<uint8_t> ReadByte(size_t offset) const;
  absl::Status WriteByte(size_t offset, uint8_t value);
  absl::StatusOr<uint16_t> ReadWord(size_t offset) const;
  absl::Status WriteWord(size_t offset, uint16_t value);

  // Transactional access (RAII pattern)
  absl::StatusOr<ReadTransaction> ReadTransaction(size_t offset, size_t size);
  absl::StatusOr<WriteTransaction> WriteTransaction(size_t offset, size_t size);

  // Persistence
  absl::Status Save(const SaveSettings& settings);

  // Properties
  size_t size() const;
  const uint8_t* data() const;
  bool is_loaded() const;
};
```

### GameData Struct (`src/zelda3/game_data.h`)

The `GameData` struct holds all Zelda3-specific data:

```cpp
namespace zelda3 {

struct GameData {
  // ROM reference (non-owning)
  Rom* rom() const;
  void set_rom(Rom* rom);

  // Version info
  zelda3_version version = zelda3_version::US;
  std::string title;

  // Graphics Resources
  std::vector<uint8_t> graphics_buffer;
  std::array<std::vector<uint8_t>, kNumGfxSheets> raw_gfx_sheets;
  std::array<gfx::Bitmap, kNumGfxSheets> gfx_bitmaps;
  std::array<gfx::Bitmap, kNumLinkSheets> link_graphics;
  gfx::Bitmap font_graphics;

  // Palette Groups
  gfx::PaletteGroupMap palette_groups;

  // Blockset/Spriteset/Paletteset IDs
  std::array<std::array<uint8_t, 8>, kNumMainBlocksets> main_blockset_ids;
  std::array<std::array<uint8_t, 4>, kNumRoomBlocksets> room_blockset_ids;
  std::array<std::array<uint8_t, 4>, kNumSpritesets> spriteset_ids;
  std::array<std::array<uint8_t, 4>, kNumPalettesets> paletteset_ids;
};

// Loading/Saving functions
absl::Status LoadGameData(Rom& rom, GameData& data, const LoadOptions& options = {});
absl::Status SaveGameData(Rom& rom, GameData& data);

}  // namespace zelda3
```

### Transaction Classes (`src/rom/transaction.h`)

RAII wrappers for safe ROM access:

```cpp
class ReadTransaction {
 public:
  const uint8_t* data() const;
  size_t size() const;
  // Automatically validates bounds on construction
};

class WriteTransaction {
 public:
  uint8_t* data();
  size_t size();
  // Changes written on destruction or explicit commit
};
```

## Editor Integration

### EditorDependencies

Editors receive both `Rom*` and `GameData*` through the `EditorDependencies` struct:

```cpp
struct EditorDependencies {
  Rom* rom = nullptr;
  zelda3::GameData* game_data = nullptr;  // Zelda3-specific game state
  // ... other dependencies
};
```

### Base Editor Class

The `Editor` base class provides accessors for both:

```cpp
class Editor {
 public:
  // Set GameData for Zelda3-specific data access
  virtual void set_game_data(zelda3::GameData* game_data) {
    dependencies_.game_data = game_data;
  }

  // Accessors
  Rom* rom() const { return dependencies_.rom; }
  zelda3::GameData* game_data() const { return dependencies_.game_data; }
};
```

### GameData Propagation

The `EditorManager` propagates GameData to all editors after loading:

```cpp
// In EditorManager::LoadRom()
RETURN_IF_ERROR(zelda3::LoadGameData(*current_rom, current_session->game_data));

// Propagate to all editors
auto* game_data = &current_session->game_data;
current_editor_set->GetDungeonEditor()->set_game_data(game_data);
current_editor_set->GetOverworldEditor()->set_game_data(game_data);
current_editor_set->GetGraphicsEditor()->set_game_data(game_data);
current_editor_set->GetScreenEditor()->set_game_data(game_data);
current_editor_set->GetPaletteEditor()->set_game_data(game_data);
current_editor_set->GetSpriteEditor()->set_game_data(game_data);
```

## Accessing Game Data

### Before (Old Architecture)

```cpp
// Graphics buffer was on Rom class
auto& gfx_buffer = rom_->graphics_buffer();

// Palettes were on Rom class
const auto& palette = rom_->palette_group().dungeon_main[0];
```

### After (New Architecture)

```cpp
// Graphics buffer is on GameData
auto& gfx_buffer = game_data_->graphics_buffer;

// Palettes are on GameData
const auto& palette = game_data_->palette_groups.dungeon_main[0];
```

## File Structure

```
src/
├── rom/                          # Generic SNES ROM module
│   ├── CMakeLists.txt
│   ├── rom.h                     # Rom class declaration
│   ├── rom.cc                    # Rom class implementation
│   ├── rom_diagnostics.h         # Checksum/validation utilities
│   ├── rom_diagnostics.cc
│   ├── transaction.h             # RAII transaction wrappers
│   └── snes.h                    # SNES hardware constants
│
└── zelda3/
    ├── game_data.h               # GameData struct and loaders
    ├── game_data.cc              # LoadGameData/SaveGameData impl
    └── ...                       # Other Zelda3-specific modules
```

## Migration Guide

When updating code to use the new architecture:

1. **Change includes**: `#include "app/rom.h"` → `#include "rom/rom.h"`

2. **Add GameData include**: `#include "zelda3/game_data.h"`

3. **Update graphics access**:
   ```cpp
   // Old
   rom_->mutable_graphics_buffer()
   // New
   game_data_->graphics_buffer
   ```

4. **Update palette access**:
   ```cpp
   // Old
   rom_->palette_group().dungeon_main
   // New
   game_data_->palette_groups.dungeon_main
   ```

5. **Update LoadFromData calls**:
   ```cpp
   // Old
   rom.LoadFromData(data, false);
   // New
   rom.LoadFromData(data);  // No second parameter
   ```

6. **For classes that need GameData**:
   - Add `zelda3::GameData* game_data_` member
   - Add `void set_game_data(zelda3::GameData*)` method
   - Or use `game_data()` accessor from Editor base class

## Best Practices

1. **Use GameData for Zelda3-specific data**: Never store palettes or graphics on Rom
2. **Use Rom for raw byte access**: Load/save operations, byte reads/writes
3. **Propagate GameData early**: Set game_data before calling Load() on editors
4. **Use transactions for bulk access**: More efficient than individual byte reads
5. **Check game_data() before use**: Return error if null when required

## Related Documents

- [graphics_system_architecture.md](graphics_system_architecture.md) - Graphics loading and Arena system
- [dungeon_editor_system.md](dungeon_editor_system.md) - Dungeon editor architecture
- [overworld_editor_system.md](overworld_editor_system.md) - Overworld editor architecture
