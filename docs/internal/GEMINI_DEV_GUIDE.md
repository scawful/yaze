# GEMINI Development Guide

A comprehensive guide for Gemini AI to work effectively with the YAZE codebase. This document provides explicit patterns, anti-patterns, workflows, and decision trees to follow when implementing features.

**Status**: Living document - Updated as project evolves
**Last Updated**: 2025-11-21
**Audience**: Gemini AI contributing code to YAZE
**Related Documents**: [CLAUDE.md](../../CLAUDE.md), [GEMINI.md](../../GEMINI.md), [CLAUDE Code Style](./CODE_STYLE.md)

---

## Table of Contents

1. [Core Workflows](#core-workflows)
2. [CMake Dependency Management](#cmake-dependency-management)
3. [Include Paths and Headers](#include-paths-and-headers)
4. [Build and Test Workflow](#build-and-test-workflow)
5. [Code Organization Patterns](#code-organization-patterns)
6. [Common Mistakes to Avoid](#common-mistakes-to-avoid)
7. [Collaboration with Claude](#collaboration-with-claude)
8. [Decision Trees](#decision-trees)
9. [Quick Reference](#quick-reference)

---

## Core Workflows

### Workflow 1: Adding a New Feature to the Dungeon Editor

**Goal**: Add a new interactive feature to the dungeon room editor UI

**Step-by-Step Process**:

1. **Check Coordination Board**
   ```bash
   # Before starting, read the shared protocol
   cat /Users/scawful/Code/yaze/docs/internal/agents/coordination-board.md
   ```
   - Look for any `REQUEST` entries addressed to you
   - Acknowledge if you find pending blockers
   - Update with your work status

2. **Identify Code Location**
   - Feature affects dungeon data model? → `src/zelda3/dungeon/`
   - Feature is UI rendering? → Look for UI classes in `src/app/editor/dungeon/`
   - Feature affects graphics? → `src/app/gfx/`
   - Feature affects ROM I/O? → `src/app/rom.cc` or transaction classes

3. **Examine Existing Pattern**
   ```bash
   # Find similar existing features
   grep -r "SelectRoom" /Users/scawful/Code/yaze/src --include="*.h" | head -5

   # Study the implementation
   cat /Users/scawful/Code/yaze/src/zelda3/dungeon/room.h | head -50
   ```

4. **Make Code Changes**
   - Create new files in appropriate directory
   - Add includes following the project pattern (see Section 3)
   - Implement functionality following architectural patterns from CLAUDE.md
   - Use `AgentUITheme` for all colors (never hardcode)

5. **Update CMakeLists.txt**
   - See Section 2 for detailed instructions

6. **Test Your Changes**
   - Unit tests: `./build/bin/yaze_test --unit` (10-30 seconds)
   - Manual testing: `./build/bin/yaze --rom_file zelda3.sfc --editor Dungeon`
   - Format check: `cmake --build build --target format-check`

7. **Commit with Proper Message**
   ```bash
   git add src/zelda3/dungeon/my_feature.cc src/zelda3/dungeon/my_feature.h
   git commit -m "feat(dungeon): add X feature for Y reason"
   ```

### Workflow 2: Fixing a Build Error

**Goal**: Resolve compilation errors when adding new code

**Decision Tree**:

```
Is the error about missing includes?
├─ YES: See Section 3 "Include Paths and Headers"
│  ├─ Check include syntax (use quotes, not angle brackets for project files)
│  ├─ Verify header is in CMakeLists.txt if it's the only source file
│  └─ Rebuild: cmake --build build --target yaze
│
├─ NO: Is it about undefined symbols?
│  ├─ YES: Library not linked in CMakeLists.txt
│  │  ├─ Find the library: grep -r "YourSymbol" /Users/scawful/Code/yaze/src
│  │  ├─ Add target_link_libraries() in CMakeLists.txt
│  │  └─ See Section 2 for examples
│  │
│  └─ NO: Is it about wrong file path?
│     ├─ YES: File path in CMakeLists.txt uses wrong directory
│     │  ├─ Files must be relative to CMakeLists.txt location
│     │  ├─ Example: If CMakeLists.txt is in src/, path should be "file.cc" not "../src/file.cc"
│     │  └─ Check directory structure: ls -la /path/to/file
│     │
│     └─ NO: Check git status for untracked files
│        └─ cmake --build build --target yaze (full rebuild)
```

### Workflow 3: Integrating with Graphics System

**Goal**: Properly load and render graphics assets

**Pattern**:

```cpp
// 1. Include the graphics arena
#include "app/gfx/resource/arena.h"

// 2. Get reference to graphics sheet
auto& sheet = gfx::Arena::Get().mutable_gfx_sheet(sheet_index);

// 3. Modify graphics data
sheet.SetPixel(x, y, palette_index);

// 4. Notify the arena
gfx::Arena::Get().NotifySheetModified(sheet_index);

// 5. Changes auto-propagate to all editors
```

**Key Points**:
- Never modify `gfx_sheet` directly without `NotifySheetModified()`
- Use `LoadAreaGraphics()` methods to read from ROM
- Use `RenderBitmap()` for immediate visual updates (not `UpdateBitmap()`)
- See CLAUDE.md section "Graphics Sheet Management" for complete details

---

## CMake Dependency Management

### Section 2.1: Adding a New Library Dependency

**Scenario**: You need to use a third-party library like `nlohmann/json`

**Steps**:

1. **Check if dependency already exists**
   ```bash
   # Search for existing usage
   grep -r "nlohmann" /Users/scawful/Code/yaze/cmake --include="*.cmake"
   grep -r "json.hpp" /Users/scawful/Code/yaze/src --include="*.h"
   ```

2. **Locate or create the dependency CMake file**
   - If already exists: Skip to Step 4
   - If new: Create `/Users/scawful/Code/yaze/cmake/dependencies/my_lib.cmake`

3. **Add to dependencies.cmake**
   - Open `/Users/scawful/Code/yaze/cmake/dependencies.cmake`
   - Add include statement:
     ```cmake
     if(YAZE_ENABLE_JSON)
       include(cmake/dependencies/json.cmake)
       list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_JSON_TARGETS})
     endif()
     ```
   - This respects optional feature flags

4. **Link to your target**
   - Open the CMake file where your target is defined (e.g., `src/zelda3/zelda3_library.cmake`)
   - Add to `target_link_libraries()`:
     ```cmake
     target_link_libraries(yaze_zelda3 PUBLIC
       yaze_gfx
       yaze_util
       nlohmann_json::nlohmann_json    # Add this line
       ${ABSL_TARGETS}
     )
     ```

5. **Verify the dependency is available**
   - Open `/Users/scawful/Code/yaze/cmake/dependencies/json.cmake`
   - Ensure it sets `YAZE_JSON_TARGETS` variable

### Section 2.2: Example - Linking Multiple Libraries

**Common Pattern in YAZE**:

```cmake
# In src/zelda3/zelda3_library.cmake
add_library(yaze_zelda3 STATIC ${YAZE_APP_ZELDA3_SRC})

target_include_directories(yaze_zelda3 PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/incl
  ${CMAKE_SOURCE_DIR}/ext/json/include
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_zelda3 PUBLIC
  yaze_gfx           # Core graphics library
  yaze_util          # Utility functions
  yaze_common        # Shared constants
  ${ABSL_TARGETS}    # Google Abseil (status, strings, etc)
)
```

**Key Patterns**:
- Public dependencies go in `PUBLIC` (used by consumers of this library)
- Private dependencies go in `PRIVATE` (only this library uses them)
- Never hardcode library names - use variables like `${ABSL_TARGETS}`
- Always include directories before linking

### Section 2.3: Source File Organization

**Where to add new source files**:

```
src/
├── zelda3/
│   ├── dungeon/              ← Dungeon game logic
│   │   ├── room.h
│   │   ├── room.cc
│   │   ├── dungeon_object_editor.h
│   │   └── dungeon_object_editor.cc
│   ├── overworld/            ← Overworld game logic
│   └── zelda3_library.cmake  ← Defines yaze_zelda3 library
│
├── app/
│   ├── gfx/                  ← Graphics system
│   │   ├── snes_tile.h
│   │   ├── snes_tile.cc
│   │   └── gfx_library.cmake
│   ├── editor/               ← GUI editor system
│   │   └── dungeon/          ← Dungeon UI components
│   │       ├── dungeon_graphics_panel.h
│   │       └── dungeon_graphics_panel.cc
│   └── app.cmake             ← Main app library definition
│
└── CMakeLists.txt            ← Root CMakeLists
```

**When adding a new file**:

1. Create file in correct directory based on its purpose
2. Add filename to the appropriate source list in CMakeLists.txt or .cmake file:
   ```cmake
   set(YAZE_APP_ZELDA3_SRC
     zelda3/dungeon/dungeon_object_editor.cc  # ← Your file here
     zelda3/dungeon/room.cc
     # ... other files
   )
   ```
3. Rebuild to verify it compiles: `cmake --build build --target yaze`

### Section 2.4: Rebuilding After CMake Changes

**When you modify CMakeLists.txt or add/remove files**:

```bash
# Option 1: Quick rebuild (if sure about changes)
cmake --build build --target yaze

# Option 2: Explicit reconfigure (safer)
cmake --preset mac-dbg  # or lin-dbg / win-dbg
cmake --build build --target yaze

# Option 3: Full clean rebuild (most thorough)
rm -rf build
cmake --preset mac-dbg
cmake --build build --target yaze
```

**How to know which option**:
- Added/removed source file? → Option 1 (quick)
- Modified target_link_libraries? → Option 1 (quick)
- Modified CMakeLists.txt structure? → Option 2 (explicit reconfigure)
- If build still fails? → Option 3 (full clean)

---

## Include Paths and Headers

### Section 3.1: Correct Include Syntax

**RULE**: Use quotes `"..."` for project files, angle brackets `<...>` for external libraries.

```cpp
// CORRECT: Project files use quotes
#include "app/gui/core/icons.h"
#include "zelda3/dungeon/room.h"
#include "app/rom.h"

// CORRECT: External libraries use angle brackets
#include <imgui/imgui.h>
#include <SDL2/SDL.h>
#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

// WRONG: Never use angle brackets for project files
#include <app/gui/core/icons.h>  // ❌ WRONG

// WRONG: Never use quotes for external libraries
#include "imgui/imgui.h"  // ❌ WRONG
```

**Why**: This follows the Google C++ Style Guide and ensures:
- Project files resolve relative to `src/` directory
- External libraries resolve via CMake include paths
- IDE code completion works correctly

### Section 3.2: Material Design Icons

**Location**: `src/app/gui/core/icons.h`

**Usage Pattern**:

```cpp
#include "app/gui/core/icons.h"

// In ImGui code
ImGui::Button(ICON_MD_ARROW_DOWNWARD " Download");
ImGui::MenuItem(ICON_MD_SETTINGS " Settings");
ImGui::Text(ICON_MD_ERROR " Error occurred");
```

**Common Icons**:
- `ICON_MD_ARROW_DOWNWARD` - Down arrow
- `ICON_MD_ARROW_UPWARD` - Up arrow
- `ICON_MD_SETTINGS` - Settings gear
- `ICON_MD_SAVE` - Save/disk
- `ICON_MD_DELETE` - Trash can
- `ICON_MD_EDIT` - Pencil
- `ICON_MD_ERROR` - Exclamation mark
- `ICON_MD_CHECK` - Checkmark
- `ICON_MD_CLOSE` - X mark

**Find all icons**:
```bash
grep "ICON_MD_" /Users/scawful/Code/yaze/src/app/gui/core/icons.h | wc -l
# Returns all available icons
```

**Common Mistake**:
```cpp
// WRONG: Icon name doesn't exist
ImGui::Button(ICON_MD_ARROW_DOWN);  // ❌ Doesn't exist

// CORRECT: Use exact name from icons.h
ImGui::Button(ICON_MD_ARROW_DOWNWARD);  // ✅ Correct
```

### Section 3.3: Header Organization Best Practices

**In Your .h File**:

```cpp
#pragma once  // Include guard (preferred over #ifndef)

#include <vector>        // Standard library (sorted alphabetically)
#include <string>
#include <memory>

#include "absl/status/status.h"           // External dependencies (alphabetical)
#include "absl/status/statusor.h"
#include "imgui/imgui.h"

#include "app/gfx/bitmap.h"               // Project dependencies (alphabetical)
#include "app/rom.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace app {
namespace gui {

class MyComponent {
  // ...
};

}  // namespace gui
}  // namespace app
}  // namespace yaze
```

**Order**:
1. Standard library headers (lowest level)
2. External library headers (middle level)
3. Project headers (application level)

**Why**:
- Prevents circular includes
- Makes dependency flow clear
- Matches Google C++ Style Guide

### Section 3.4: Including Generated Headers

**For auto-generated config headers**:

```cpp
#include "yaze_config.h"  // Auto-generated from yaze_config.h.in

// This provides build-time configuration like:
// YAZE_PLATFORM_MACOS, YAZE_PLATFORM_LINUX, YAZE_PLATFORM_WINDOWS
// YAZE_VERSION_MAJOR, YAZE_VERSION_MINOR
```

**Location**: `${PROJECT_BINARY_DIR}/yaze_config.h` (not in source tree)

---

## Build and Test Workflow

### Section 4.1: Daily Development Workflow

**Typical Session**:

```bash
# 1. Start with configured build directory (from GEMINI.md)
# Already have: cmake --preset mac-dbg (done once)

# 2. Make code changes
# Edit: src/zelda3/dungeon/my_feature.cc

# 3. Build only changed target (FAST: 10-30 seconds)
cmake --build build --target yaze

# 4. Run targeted unit tests (FAST: 5-10 seconds)
./build/bin/yaze_test --unit

# 5. Manual testing (if applicable)
./build/bin/yaze --rom_file zelda3.sfc --editor Dungeon

# 6. Check code format
cmake --build build --target format-check

# 7. If format check fails, auto-fix
cmake --build build --target format

# 8. Final validation before commit
./build/bin/yaze_test --unit
cmake --build build --target format-check

# 9. Commit
git add src/zelda3/dungeon/my_feature.cc src/zelda3/dungeon/my_feature.h
git commit -m "feat(dungeon): description"
```

**Time Breakdown**:
- Build: 10-30 seconds
- Unit tests: 5-10 seconds
- Format check: 5 seconds
- Total: ~1-2 minutes per change

### Section 4.2: Testing Strategy Decision Tree

**Question**: What should I test?

```
Did I change game logic (dungeon/overworld/sprite data)?
├─ YES: Add unit test
│  └─ File: test/unit/my_feature_test.cc
│
├─ NO: Did I change editor UI rendering?
│  ├─ YES: Add manual testing steps
│  │  └─ Run: ./build/bin/yaze --editor Dungeon --rom_file zelda3.sfc
│  │
│  └─ NO: Did I change ROM I/O or data persistence?
│     ├─ YES: Add integration test
│     │  └─ File: test/integration/my_feature_rom_test.cc
│     │
│     └─ NO: Did I change something else?
│        └─ Add unit test to be safe
```

### Section 4.3: Common Test Patterns

**Unit Test Template**:

```cpp
#include <gtest/gtest.h>
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

class RoomTest : public ::testing::Test {
 protected:
  Room room;
};

TEST_F(RoomTest, LoadsRoomDataCorrectly) {
  // Arrange
  uint16_t room_id = 0x00;

  // Act
  auto status = room.LoadFromRom(nullptr, room_id);

  // Assert
  EXPECT_TRUE(status.ok());
}

}  // namespace zelda3
}  // namespace yaze
```

**Where to put tests**:
- `test/unit/` - Fast tests, no ROM required
- `test/integration/` - Multi-component tests, may need ROM
- `test/e2e/` - Full GUI workflows (advanced)

### Section 4.4: Format Checking and Auto-Fixing

**Before every commit**:

```bash
# Check format (doesn't modify files)
cmake --build build --target format-check

# Auto-fix format issues
cmake --build build --target format

# Verify fix
cmake --build build --target format-check
```

**Format follows**:
- Google C++ Style Guide (enforced by clang-format)
- Line length: 80 characters (preferred)
- Indentation: 2 spaces
- Brace style: K&R with modifications

**Common formatting issues**:
- Long lines (>80 chars) - clang-format will split
- Missing spaces around operators - clang-format adds
- Inconsistent brace placement - clang-format corrects

---

## Code Organization Patterns

### Section 5.1: Dungeon Editor System

**When adding dungeon-related code**:

**Model Layer** (`src/zelda3/dungeon/`):
- Game logic and data structures
- Files: `room.h/cc`, `room_object.h/cc`, `dungeon_object_editor.h/cc`
- No UI code here - only pure data and business logic

**Example**:
```cpp
// In src/zelda3/dungeon/room.h
#pragma once
#include "absl/status/status.h"

namespace yaze {
namespace zelda3 {

class Room {
 public:
  absl::Status LoadFromRom(Rom* rom, uint16_t room_id);
  absl::Status SaveToRom(Rom* rom);

  const std::vector<RoomObject>& objects() const { return objects_; }

 private:
  std::vector<RoomObject> objects_;
};

}  // namespace zelda3
}  // namespace yaze
```

**Renderer Layer** (if needed, `src/app/editor/dungeon/`):
- ImGui rendering code
- Visual representation of model data
- Example: `DungeonObjectRenderer` renders room objects on canvas

### Section 5.2: Adding UI Components

**When adding new ImGui UI**:

**Step 1**: Define in header with proper namespace

```cpp
// In src/app/editor/dungeon/my_panel.h
#pragma once
#include <functional>
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

class MyPanel {
 public:
  explicit MyPanel(Rom* rom) : rom_(rom) {}

  // ImGui rendering function
  void Draw();

  // Callbacks for parent coordination
  void SetRefreshCallback(std::function<void()> callback) {
    refresh_callback_ = callback;
  }

 private:
  Rom* rom_;
  std::function<void()> refresh_callback_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze
```

**Step 2**: Implement with ImGui code

```cpp
// In src/app/editor/dungeon/my_panel.cc
#include "my_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/agent_ui.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

void MyPanel::Draw() {
  if (ImGui::Begin("My Panel")) {
    // Use semantic colors from theme
    const auto& theme = AgentUI::GetTheme();

    ImGui::TextColored(theme.status_success, "Success");
    ImGui::TextColored(theme.status_error, "Error");

    if (ImGui::Button(ICON_MD_SAVE " Save")) {
      // Handle save logic
      if (refresh_callback_) {
        refresh_callback_();
      }
    }
  }
  ImGui::End();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
```

**Step 3**: Register in CMakeLists.txt

```cmake
# In src/app/editor/dungeon/CMakeLists.txt or relevant .cmake file
set(YAZE_DUNGEON_EDITOR_SRC
  dungeon_editor.cc
  my_panel.cc  # ← Add your file
)
```

### Section 5.3: Following YAZE Naming Conventions

**From CLAUDE.md - Always follow these**:

| Verb | Meaning | Example |
|------|---------|---------|
| **Load** | Reading data from ROM into memory | `LoadFromRom()`, `LoadAreaGraphics()` |
| **Render** | Processing graphics data into bitmaps (CPU) | `RenderTile()`, `RenderBitmap()` |
| **Draw** | Displaying textures/shapes on canvas (GPU) | `DrawTile()`, `ImGui::Button()` |
| **Update** | UI state changes, property updates | `UpdateSelection()`, `UpdateUI()` |

**CORRECT Usage**:
```cpp
// Load: reading from ROM
auto status = room.LoadFromRom(rom, room_id);

// Render: processing to bitmap
auto bitmap = renderer.RenderBitmap(tile_data);

// Draw: displaying on canvas
canvas.DrawBitmap(bitmap, x, y);

// Update: changing state
editor.UpdateSelection(new_object);
```

**WRONG Usage**:
```cpp
// ❌ Confusing verb usage
room.DrawFromRom();        // Is this loading or displaying?
canvas.RenderImage();      // Is this processing or displaying?
bitmap.UpdatePixels();     // What kind of update?
```

### Section 5.4: Callback Pattern for Coordination

**Problem**: Parent editor needs to know when child component changes

**Solution Pattern**:

```cpp
// In child component header
class ChildPanel {
 public:
  using RefreshCallback = std::function<void()>;

  void SetRefreshCallback(RefreshCallback callback) {
    refresh_callback_ = callback;
  }

  void OnDataChanged() {
    if (refresh_callback_) {
      refresh_callback_();
    }
  }

 private:
  RefreshCallback refresh_callback_;
};

// In parent editor
class ParentEditor {
  void InitializeUI() {
    child_panel_.SetRefreshCallback([this]() {
      this->RefreshAllViews();
    });
  }

  void RefreshAllViews() {
    // Parent updates based on child changes
  }
};
```

**Why**: Avoids circular dependencies and keeps components loosely coupled

---

## Common Mistakes to Avoid

### Mistake 1: Adding Includes Without Checking Build System Linkage

**Problem**:
```cpp
// In my_file.cc
#include "nlohmann/json.hpp"  // Compiles fine locally

// But linker fails: undefined reference to nlohmann_json::...
```

**Root Cause**: The library isn't linked in CMakeLists.txt

**Solution**:
```cmake
# In the .cmake file where your library is defined
target_link_libraries(my_library PUBLIC
  nlohmann_json::nlohmann_json  # ← Add this line
  other_libs
)
```

**Prevention Checklist**:
- [ ] Added include to source file
- [ ] Added dependency to CMakeLists.txt / .cmake file
- [ ] Ran `cmake --build build --target yaze` to verify linking
- [ ] No "undefined reference" errors in build output

### Mistake 2: Using Wrong Icon Constant Names

**Problem**:
```cpp
// Icon doesn't exist
ImGui::Button(ICON_MD_ARROW_DOWN);  // ❌ Compiles but wrong symbol!

// Runtime: Icon displays as garbage character
```

**Solution**:
```bash
# Check exact icon name before using
grep "ARROW" /Users/scawful/Code/yaze/src/app/gui/core/icons.h
# Output: ICON_MD_ARROW_DOWNWARD (not ARROW_DOWN)

// Use correct name
ImGui::Button(ICON_MD_ARROW_DOWNWARD);  // ✅ Correct
```

**Prevention**:
- Always search `icons.h` for available icons
- Copy exact name from header file
- Don't guess at icon names

### Mistake 3: Creating New Files Without Adding to CMakeLists.txt

**Problem**:
```bash
# Create new file
touch src/zelda3/dungeon/my_feature.cc

# Forget to add to CMakeLists.txt
# Build succeeds but new file never compiles!
```

**Solution**:
1. Edit relevant .cmake file
2. Add filename to source list:
   ```cmake
   set(YAZE_APP_ZELDA3_SRC
     zelda3/dungeon/my_feature.cc  # ← Add here
     zelda3/dungeon/room.cc
   )
   ```
3. Rebuild: `cmake --build build --target yaze`

**Prevention Checklist**:
- [ ] Created new .cc/.h file
- [ ] Added .cc filename to appropriate CMakeLists.txt or .cmake file
- [ ] Did NOT add .h file (headers are included, not compiled)
- [ ] Verified with: `cmake --build build --target yaze` (no "unused file" warning)

### Mistake 4: Hardcoding Colors Instead of Using AgentUITheme

**Problem**:
```cpp
// Hardcoded color - wrong in light/dark themes
ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error");
```

**Solution**:
```cpp
#include "app/gui/core/agent_ui.h"

// Use semantic colors that respect theme
const auto& theme = AgentUI::GetTheme();
ImGui::TextColored(theme.status_error, "Error");
ImGui::TextColored(theme.status_success, "Success");
ImGui::TextColored(theme.panel_bg_color, "Background");
```

**Prevention**:
- [ ] Never use raw `ImVec4()` for colors
- [ ] Always include `agent_ui.h` and fetch theme
- [ ] Use semantic color names: `status_error`, `status_success`, `panel_bg_color`, etc.

### Mistake 5: Not Testing Builds Before Committing

**Problem**:
```bash
# Make changes
git add src/zelda3/dungeon/my_feature.cc
git commit -m "feat: add feature"
# Push to CI

# CI fails: unknown include path, undefined symbol
# Blame: "But it compiled locally!"
```

**Root Cause**: Didn't actually rebuild with CMake

**Solution**:
```bash
# Before EVERY commit
cmake --build build --target yaze  # Verify compiles
./build/bin/yaze_test --unit       # Verify tests pass
cmake --build build --target format-check  # Verify format
```

**Checklist Before Commit**:
- [ ] `cmake --build build --target yaze` - No errors
- [ ] `./build/bin/yaze_test --unit` - All pass
- [ ] `cmake --build build --target format-check` - Passes
- [ ] No untracked files left behind
- [ ] Commit message follows Conventional Commits format

### Mistake 6: Forgetting ROM State Checks

**Problem**:
```cpp
// No check if ROM is loaded
auto byte = rom_->ReadByte(address);  // Crashes if rom_ is nullptr!
```

**Solution**:
```cpp
// Always check ROM state first
if (!rom_->is_loaded()) {
  return absl::FailedPreconditionError("ROM not loaded");
}

auto byte = rom_->ReadByte(address);  // Safe
```

**Safe Pattern**:
```cpp
// Check at operation start
if (!rom_ || !rom_->is_loaded()) {
  return absl::FailedPreconditionError("ROM must be loaded first");
}

// Now proceed safely
auto status = rom_->ReadTransaction(address, data);
RETURN_IF_ERROR(status);  // Error handling macro from absl
```

### Mistake 7: Blocking on Texture Loads

**Problem**:
```cpp
// Synchronous load freezes UI
for (int i = 0; i < 1000; ++i) {
  auto bitmap = LoadTile(i);  // Blocks for 10+ seconds!
}
```

**Solution**:
```cpp
#include "app/gfx/resource/arena.h"

// Queue textures asynchronously with priority
for (int i = 0; i < 1000; ++i) {
  gfx::Arena::Get().QueueDeferredTexture(
    my_bitmap,
    gfx::TexturePriority::kLow  // Low priority, processed in background
  );
}

// In main render loop
auto [high, low] = gfx::Arena::Get().GetNextDeferredTextureBatch(10, 50);
// Process batches without blocking
```

**Key Points**:
- Never call expensive Load functions in UI code
- Always use `gfx::Arena::Get().QueueDeferredTexture()` for deferred loading
- Process texture queue in Update() with reasonable batch sizes

---

## Collaboration with Claude

### Section 7.1: Role Separation

**Gemini Focus Areas**:
- Feature implementation (game logic and UI)
- Bug fixes in non-critical systems
- Test writing and quality assurance
- Documentation updates for features you implement

**Claude (CLAUDE_DOCS) Focus Areas**:
- Release builds and optimization
- CI/CD pipeline and build system fixes
- Cross-cutting refactoring and architecture changes
- Documentation architecture and organization
- Critical bug fixes affecting multiple systems

### Section 7.2: Coordination Protocol

**Before Starting Work**:
```bash
# 1. Read coordination board
cat /Users/scawful/Code/yaze/docs/internal/agents/coordination-board.md

# 2. Check for blocking issues
grep -i "request" docs/internal/agents/coordination-board.md
grep -i "blocker" docs/internal/agents/coordination-board.md

# 3. If you find an entry for GEMINI_* persona, acknowledge it
```

**When Blocked**:
```bash
# Document the blocker in coordination board
# Add entry like:

# BLOCKER: GEMINI needs CMake help
# - Added new feature files but get linking errors
# - Tried: linking nlohmann_json to target
# - Issue: undefined references to json library
# - Blocked by: Need CMake expert to verify dependency setup

# Then wait for Claude response
```

**When Handing Off**:
```bash
# Update coordination board with status
# Add entry like:

# HANDOFF: Feature X implementation complete
# - Files: src/zelda3/dungeon/my_feature.cc/h
# - Tests: test/unit/my_feature_test.cc
# - Status: Ready for code review
# - Next: Needs documentation update + release notes

git push origin branch_name
```

### Section 7.3: Information to Include in Handoff

When passing work to Claude, include:

1. **File List**
   ```
   Files Modified:
   - src/zelda3/dungeon/my_feature.cc/h (+250 lines)
   - test/unit/my_feature_test.cc (new, 75 lines)
   - src/zelda3/zelda3_library.cmake (1 line added to source list)
   ```

2. **Build Status**
   ```
   Build Verification:
   - ✓ cmake --build build --target yaze (successful)
   - ✓ ./build/bin/yaze_test --unit (all passed)
   - ✓ cmake --build build --target format-check (passed)
   ```

3. **Architecture Decisions**
   ```
   Design Decisions:
   - Used callback pattern for parent-child coordination
   - Game logic in src/zelda3/dungeon/ (pure data)
   - UI rendering in src/app/editor/dungeon/ (ImGui)
   - Followed naming conventions from CLAUDE.md
   ```

4. **Testing Performed**
   ```
   Manual Testing:
   - Loaded ROM with --rom_file zelda3.sfc
   - Opened Dungeon editor with --editor Dungeon
   - Verified feature works with actual ROM data
   - Tested edge cases: null ROM, empty room data, etc.
   ```

---

## Decision Trees

### Should I Use a Header-Only Library?

```
Do you need this code in multiple places?
├─ NO: Keep in .cc file, no header needed
│
└─ YES: Need header file
   │
   └─ Is it a template class?
      ├─ YES: May use header-only implementation
      │  └─ Put all code in .h file with inline keyword
      │
      └─ NO: Use regular header/implementation split
         ├─ Declarations in .h file
         └─ Implementation in .cc file
```

### Should I Create a New Library vs Adding to Existing?

```
Is this code related to game logic (room, sprite, etc)?
├─ YES: Add to yaze_zelda3 library
│  └─ File goes in src/zelda3/
│
└─ NO: Is it graphics-related?
   ├─ YES: Add to yaze_gfx library
   │  └─ File goes in src/app/gfx/
   │
   └─ NO: Is it editor UI?
      ├─ YES: Add to editor targets in src/app/
      │  └─ File goes in src/app/editor/
      │
      └─ NO: Is it utility/helper code?
         └─ Add to yaze_util library
            └─ File goes in src/util/
```

### How to Handle ROM Data Changes?

```
Do I need to modify ROM data?
├─ NO: Just read it
│  └─ Use rom_->ReadByte() or ReadTransaction()
│
└─ YES: Need to write back
   │
   ├─ Single byte? Use rom_->WriteByte()
   │
   └─ Multiple bytes? Use rom_->WriteTransaction()
      │
      └─ Affects graphics? Call Renderer::Get().RenderBitmap()
```

---

## Quick Reference

### Command Cheat Sheet

```bash
# === Configuration (one-time per project) ===
cmake --preset mac-dbg        # Configure for macOS debug
cmake --preset lin-dbg        # Configure for Linux debug
cmake --preset win-dbg        # Configure for Windows debug

# === Building (after code changes) ===
cmake --build build --target yaze              # Build GUI app
cmake --build build --target yaze_test         # Build test suite
cmake --build build --target yaze_test_stable  # Build stable tests
cmake --build build --target format            # Auto-format code
cmake --build build --target format-check      # Check format

# === Testing (verification before commit) ===
./build/bin/yaze_test --unit                   # Fast unit tests
./build/bin/yaze_test --integration            # Integration tests
./build/bin/yaze_test "*DungeonObject*"        # Specific pattern
./build/bin/yaze_test --list-tests             # List all tests

# === Running (manual testing) ===
./build/bin/yaze                               # Launch GUI
./build/bin/yaze --rom_file zelda3.sfc         # Load ROM
./build/bin/yaze --editor Dungeon              # Open specific editor
./build/bin/yaze --rom_file zelda3.sfc --editor Dungeon --cards="Room 0"

# === Cleaning ===
cmake --build build --target clean             # Clean build artifacts
rm -rf build                                   # Full clean (need reconfigure)

# === Development utilities ===
cmake --list-presets                           # Show available presets
git status                                     # Check changed files
grep -r "YOUR_SYMBOL" src --include="*.h"     # Find usage
```

### File Location Reference

| Purpose | Location | Example |
|---------|----------|---------|
| Game logic - Dungeon | `src/zelda3/dungeon/` | `room.h`, `room.cc` |
| Game logic - Overworld | `src/zelda3/overworld/` | `overworld.h`, `overworld.cc` |
| Graphics system | `src/app/gfx/` | `bitmap.h`, `snes_tile.cc` |
| Editor UI | `src/app/editor/` | `dungeon_editor.h`, `dungeon_panel.cc` |
| ROM I/O | `src/app/rom.h` + `rom.cc` | Core ROM class |
| CMake build | `src/zelda3/zelda3_library.cmake` | Library definitions |
| Tests - Unit | `test/unit/` | `room_test.cc` |
| Tests - Integration | `test/integration/` | `room_rom_test.cc` |
| Configuration | `/CMakeLists.txt` | Root build config |

### Build Time Expectations

| Change | Rebuild Time | Notes |
|--------|--------------|-------|
| Single .cc file | 10-30 seconds | Fast incremental |
| Single .h file | 1-3 minutes | Recompiles dependents |
| CMakeLists.txt | 5-10 minutes | Need reconfigure |
| First build (cold) | 10-20 minutes | Platform dependent |
| Full clean rebuild | 15-25 minutes | Worst case |

### Common Problems and Quick Fixes

| Problem | Quick Fix |
|---------|-----------|
| "No preset found" | Use full preset: `mac-dbg` not `dbg` |
| "Build directory outdated" | `rm -rf build && cmake --preset mac-dbg` |
| "Undefined reference to symbol" | Add `target_link_libraries()` in CMake |
| "Include not found" | Check path uses quotes `"app/..."` not angle brackets |
| "Icon displays as garbage" | Check exact icon name in `icons.h` |
| "Format check fails" | Run `cmake --build build --target format` |
| "Tests fail without ROM" | Use `--unit` flag or provide `--rom-path` |
| "Slow incremental build" | Use `--target yaze` not full build |

---

## Additional Resources

- **[CLAUDE.md](../../CLAUDE.md)** - Overall project architecture and patterns
- **[GEMINI.md](../../GEMINI.md)** - Build and test command reference
- **[docs/public/build/quick-reference.md](../../docs/public/build/quick-reference.md)** - Build reference
- **[docs/internal/agents/coordination-board.md](../agents/coordination-board.md)** - Shared coordination protocol
- **[docs/internal/agents/personas.md](../agents/personas.md)** - Agent role definitions

### Architecture Documentation
- **[Dungeon Editor System](architecture/dungeon_editor_system.md)** - Architecture of the dungeon editor components
- **[Graphics System](architecture/graphics_system.md)** - Details on Arena, Bitmaps, and resource management
- **[Undo/Redo System](architecture/undo_redo_system.md)** - Implementation of the undo/redo stack
- **[Room Data Persistence](architecture/room_data_persistence.md)** - How room data is loaded/saved to ROM
- **[Overworld Editor System](architecture/overworld_editor_system.md)** - Architecture of the overworld editor
- **[Overworld Map Data](architecture/overworld_map_data.md)** - Internal structure of overworld maps
- **[ZSCustomOverworld Integration](architecture/zscustomoverworld_integration.md)** - Details on ZSO support
- **[Graphics System Architecture](architecture/graphics_system_architecture.md)** - Overview of the graphics pipeline and editors
- **[Graphics Improvement Plan](plans/graphics_system_improvement_plan.md)** - Roadmap for graphics system enhancements

---

## Document History

| Date | Change | Author |
|------|--------|--------|
| 2025-11-21 | Initial creation | Claude (Docs) |
| | Covered CMake, includes, workflows | |
| | Added decision trees and checklists | |
| | | |

---

**Status**: This is a living document. As you discover new patterns or encounter issues, please update this guide to help future development.

**Last Reviewed**: 2025-11-21
**Next Review**: When major architectural changes occur
