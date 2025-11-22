# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **Coordination Requirement**  
> Before starting or handing off work, read and update the shared protocol in
> [`docs/internal/agents/coordination-board.md`](docs/internal/agents/coordination-board.md) as
> described in `AGENTS.md`. Always acknowledge pending `REQUEST`/`BLOCKER` entries addressed to the
> Claude persona you are using (`CLAUDE_CORE`, `CLAUDE_AIINF`, or `CLAUDE_DOCS`). See
> [`docs/internal/agents/personas.md`](docs/internal/agents/personas.md) for responsibilities.

## Project Overview

**yaze** (Yet Another Zelda3 Editor) is a modern, cross-platform ROM editor for The Legend of Zelda: A Link to the Past, built with C++23. It features:
- Complete GUI editor for overworld, dungeons, sprites, graphics, and palettes
- Integrated Asar 65816 assembler for ROM patching
- SNES emulator for testing modifications
- AI-powered CLI tool (`z3ed`) for ROM hacking assistance
- ZSCustomOverworld v3 support for enhanced overworld editing

## Build System

- Use the presets defined in `CMakePresets.json` (debug: `mac-dbg` / `lin-dbg` / `win-dbg`, AI:
  `mac-ai` / `win-ai`, release: `*-rel`, etc.). Add `-v` for verbose builds.
- Always run builds from a dedicated directory when acting as an AI agent (`build_ai`, `build_agent`,
  …) so you do not interfere with the user’s `build`/`build_test` trees.
- Treat [`docs/public/build/quick-reference.md`](docs/public/build/quick-reference.md) as the single
  source of truth for commands, presets, testing, and environment prep. Only reference the larger
  troubleshooting docs when you need platform-specific fixes.

### Test Execution

```bash
# Build tests
cmake --build build --target yaze_test

# Run all tests
./build/bin/yaze_test

# Run specific categories
./build/bin/yaze_test --unit              # Unit tests only
./build/bin/yaze_test --integration       # Integration tests
./build/bin/yaze_test --e2e --show-gui    # End-to-end GUI tests

# Run with ROM-dependent tests
./build/bin/yaze_test --rom-dependent --rom-path zelda3.sfc

# Run specific test by name
./build/bin/yaze_test "*Asar*"
```

## Architecture

### Core Components

**ROM Management** (`src/app/rom.h`, `src/app/rom.cc`)
- Central `Rom` class manages all ROM data access
- Provides transaction-based read/write operations
- Handles graphics buffer, palettes, and resource labels
- Key methods: `LoadFromFile()`, `ReadByte()`, `WriteByte()`, `ReadTransaction()`, `WriteTransaction()`

**Editor System** (`src/app/editor/`)
- Base `Editor` class defines interface for all editor types
- Major editors: `OverworldEditor`, `DungeonEditor`, `GraphicsEditor`, `PaletteEditor`
- `EditorManager` coordinates multiple editor instances
- Card-based UI system for dockable editor panels

**Graphics System** (`src/app/gfx/`)
- `gfx::Bitmap`: Core bitmap class with SDL surface integration
- `gfx::Arena`: Centralized singleton for progressive asset loading (priority-based texture queuing)
- `gfx::SnesPalette` and `gfx::SnesColor`: SNES color/palette management
- `gfx::Tile16` and `gfx::SnesTile`: Tile format representations
- Graphics sheets (223 total) loaded from ROM with compression support (LC-LZ2)

**Zelda3-Specific Logic** (`src/zelda3/`)
- `zelda3::Overworld`: Manages 160+ overworld maps (Light World, Dark World, Special World)
- `zelda3::OverworldMap`: Individual map data (tiles, entities, properties)
- `zelda3::Dungeon`: Dungeon room management (296 rooms)
- `zelda3::Sprite`: Sprite and enemy data structures

**Canvas System** (`src/app/gui/canvas.h`)
- `gui::Canvas`: ImGui-based drawable canvas with pan/zoom/grid support
- Context menu system for entity editing
- Automation API for AI agent integration
- Usage tracker for click/interaction statistics

**Asar Integration** (`src/core/asar_wrapper.h`)
- `core::AsarWrapper`: C++ wrapper around Asar assembler library
- Provides patch application, symbol extraction, and error reporting
- Used by CLI tool and GUI for assembly patching

**z3ed CLI Tool** (`src/cli/`)
- AI-powered command-line interface with Ollama and Gemini support
- TUI (Terminal UI) components for interactive editing
- Resource catalog system for ROM data queries
- Test suite generation and execution
- Network collaboration support (experimental)

### Key Architectural Patterns

**Pattern 1: Modular Editor Design**
- Large editor classes decomposed into smaller, single-responsibility modules
- Separate renderer classes (e.g., `OverworldEntityRenderer`)
- UI panels managed by dedicated classes (e.g., `MapPropertiesSystem`)
- Main editor acts as coordinator, not implementer

**Pattern 2: Callback-Based Communication**
- Child components receive callbacks from parent editors via `SetCallbacks()`
- Avoids circular dependencies between modules
- Example: `MapPropertiesSystem` calls `RefreshCallback` to notify `OverworldEditor`

**Pattern 3: Progressive Loading via `gfx::Arena`**
- All expensive asset loading performed asynchronously
- Queue textures with priority: `gfx::Arena::Get().QueueDeferredTexture(bitmap, priority)`
- Process in batches during `Update()`: `GetNextDeferredTextureBatch(high_count, low_count)`
- Prevents UI freezes during ROM loading

**Pattern 4: Bitmap/Surface Synchronization**
- `Bitmap::data_` (C++ vector) and `surface_->pixels` (SDL buffer) must stay in sync
- Use `set_data()` for bulk replacement (syncs both)
- Use `WriteToPixel()` for single-pixel modifications
- Never assign directly to `mutable_data()` for replacements

## Development Guidelines

### Naming Conventions
- **Load**: Reading data from ROM into memory
- **Render**: Processing graphics data into bitmaps/textures (CPU pixel operations)
- **Draw**: Displaying textures/shapes on canvas via ImGui (GPU rendering)
- **Update**: UI state changes, property updates, input handling

### Graphics Refresh Logic
When a visual property changes:
1. Update the property in the data model
2. Call relevant `Load*()` method (e.g., `map.LoadAreaGraphics()`)
3. Force a redraw: Use `Renderer::Get().RenderBitmap()` for immediate updates (not `UpdateBitmap()`)

### Graphics Sheet Management
When modifying graphics sheets:
1. Get mutable reference: `auto& sheet = Arena::Get().mutable_gfx_sheet(index);`
2. Make modifications
3. Notify Arena: `Arena::Get().NotifySheetModified(index);`
4. Changes propagate automatically to all editors

### UI Theming System
- **Never use hardcoded colors** - Always use `AgentUITheme`
- Fetch theme: `const auto& theme = AgentUI::GetTheme();`
- Use semantic colors: `theme.panel_bg_color`, `theme.status_success`, etc.
- Use helper functions: `AgentUI::PushPanelStyle()`, `AgentUI::RenderSectionHeader()`, `AgentUI::StyledButton()`

### Multi-Area Map Configuration
- **Always use** `zelda3::Overworld::ConfigureMultiAreaMap()` when changing map area size
- Never set `area_size` property directly
- Method handles parent ID assignment and ROM data persistence

### Version-Specific Features
- Check ROM's `asm_version` byte before showing UI for ZSCustomOverworld features
- Display helpful messages for unsupported features (e.g., "Requires ZSCustomOverworld v3+")

### Entity Visibility Standards
Render overworld entities with high-contrast colors at 0.85f alpha:
- **Entrances**: Bright yellow-gold
- **Exits**: Cyan-white
- **Items**: Bright red
- **Sprites**: Bright magenta

### Debugging with Startup Flags
Jump directly to editors for faster development:
```bash
# Open specific editor with ROM
./yaze --rom_file=zelda3.sfc --editor=Dungeon

# Open with specific cards visible
./yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0,Room 1,Object Editor"

# Enable debug logging
./yaze --debug --log_file=debug.log --rom_file=zelda3.sfc --editor=Overworld
```

Available editors: Assembly, Dungeon, Graphics, Music, Overworld, Palette, Screen, Sprite, Message, Hex, Agent, Settings

## Testing Strategy

### Test Organization
```
test/
├── unit/           # Fast, isolated component tests (no ROM required)
├── integration/    # Multi-component tests (may require ROM)
├── e2e/           # Full UI workflow tests (ImGui Test Engine)
├── benchmarks/    # Performance tests
└── mocks/         # Mock objects for isolation
```

### Test Categories
- **Unit Tests**: Fast, self-contained, no external dependencies (primary CI validation)
- **Integration Tests**: Test component interactions, may require ROM files
- **E2E Tests**: Full user workflows driven by ImGui Test Engine (requires GUI)
- **ROM-Dependent Tests**: Any test requiring an actual Zelda3 ROM file

### Writing New Tests
- New class `MyClass`? → Add `test/unit/my_class_test.cc`
- Testing with ROM? → Add `test/integration/my_class_rom_test.cc`
- Testing UI workflow? → Add `test/e2e/my_class_workflow_test.cc`

### GUI Test Automation
- E2E framework uses `ImGuiTestEngine` for UI automation
- All major widgets have stable IDs for discovery
- Test helpers in `test/test_utils.h`: `LoadRomInTest()`, `OpenEditorInTest()`
- AI agents can use `z3ed gui discover`, `z3ed gui click` for automation

## Platform-Specific Notes

### Windows
- Requires Visual Studio 2022 with "Desktop development with C++" workload
- Run `scripts\verify-build-environment.ps1` before building
- gRPC builds take 15-20 minutes first time (use vcpkg for faster builds)
- Watch for path length limits: Enable long paths with `git config --global core.longpaths true`

### macOS
- Supports both Apple Silicon (ARM64) and Intel (x86_64)
- Use `mac-uni` preset for universal binaries
- Bundled Abseil used by default to avoid deployment target mismatches
- **ARM64 Note**: gRPC v1.67.1 is the tested stable version (see BUILD-TROUBLESHOOTING.md for details)

### Linux
- Requires GCC 13+ or Clang 16+
- Install dependencies: `libgtk-3-dev`, `libdbus-1-dev`, `pkg-config`

**Platform-specific build issues?** See `docs/BUILD-TROUBLESHOOTING.md`

## CI/CD Pipeline

The project uses GitHub Actions for continuous integration and deployment:

### Workflows
- **ci.yml**: Build and test on Linux, macOS, Windows (runs on push to master/develop, PRs)
- **release.yml**: Build release artifacts and publish GitHub releases
- **code-quality.yml**: clang-format, cppcheck, clang-tidy checks
- **security.yml**: Security scanning and dependency audits

### Composite Actions
Reusable build steps in `.github/actions/`:
- `setup-build` - Configure build environment with caching
- `build-project` - Build with CMake and optimal settings
- `run-tests` - Execute test suites with result uploads

### Key Features
- CPM dependency caching for faster builds
- sccache/ccache for incremental compilation
- Platform-specific test execution (stable, unit, integration)
- Automatic artifact uploads on build/test failures

## Git Workflow

**Current Phase:** Pre-1.0 (Relaxed Rules)

For detailed workflow documentation, see `docs/B4-git-workflow.md`.

### Quick Guidelines
- **Documentation/Small fixes**: Commit directly to `master` or `develop`
- **Experiments/Features**: Use feature branches (`feature/<description>`)
- **Breaking changes**: Use feature branches and document in changelog
- **Commit messages**: Follow Conventional Commits (`feat:`, `fix:`, `docs:`, etc.)

### Planned Workflow (Post-1.0)
When the project reaches v1.0 or has multiple active contributors, we'll transition to formal Git Flow with protected branches, required PR reviews, and release branches.

## Code Style

- Format code with clang-format: `cmake --build build --target format`
- Check format without changes: `cmake --build build --target format-check`
- Style guide: Google C++ Style Guide (enforced via clang-format)
- Use `absl::Status` and `absl::StatusOr<T>` for error handling
- Macros: `RETURN_IF_ERROR()`, `ASSIGN_OR_RETURN()` for status propagation

## Important File Locations

- ROM loading: `src/app/rom.cc:Rom::LoadFromFile()`
- Overworld editor: `src/app/editor/overworld/overworld_editor.cc`
- Dungeon editor: `src/app/editor/dungeon/dungeon_editor.cc`
- Graphics arena: `src/app/gfx/snes_tile.cc` and `src/app/gfx/bitmap.cc`
- Asar wrapper: `src/core/asar_wrapper.cc`
- Main application: `src/yaze.cc`
- CLI tool: `src/cli/z3ed.cc`
- Test runner: `test/yaze_test.cc`

## Common Pitfalls

1. **Bitmap data desync**: Always use `set_data()` for bulk updates, not `mutable_data()` assignment
2. **Missing texture queue processing**: Call `ProcessTextureQueue()` every frame
3. **Incorrect graphics refresh order**: Update model → Load from ROM → Force render
4. **Skipping `ConfigureMultiAreaMap()`**: Always use this method for map size changes
5. **Hardcoded colors**: Use `AgentUITheme` system, never raw `ImVec4` values
6. **Blocking texture loads**: Use `gfx::Arena` deferred loading system
7. **Missing ROM state checks**: Always verify `rom_->is_loaded()` before operations
