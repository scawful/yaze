# Changelog

High-level release summary. For detailed notes, see
`docs/public/reference/changelog.md`.

## 0.7.2 (in development)
- Development version. See `docs/internal/roadmap.md` for the current focus.

## 0.7.1 (April 2026)
- **Welcome Screen & Project Startup**:
  - Added a guided New Project flow, async ROM metadata scanning, and recent-project pin/rename/notes actions.
  - Surfaced welcome/startup actions through the command palette and added undo for recent-project removal.
- **Dungeon Editor Parity & Polish**:
  - Restored BG1/BG2 layout routing parity, preserved pit masks, and accepted single-tile `0x034` payloads.
  - Added ROM-backed parity tests/snapshots, replay-geometry selection bounds, and simplified workbench inspector/navigation flows.
- **Editor Memory & Startup Footprint**:
  - Added lazy session-editor construction, deferred hidden full-mode asset loads, overworld eager bitmap trimming, and room-buffer texture release.
  - Split render-target texture creation to keep backend behavior cleaner across platforms.
- **CLI / CI Hardening**:
  - Fixed the WASM dungeon tile-row build path, validated overworld map IDs before ROM access, and hardened Linux GUI smoke-path handling.
- **Deferred to 0.8.0**:
  - z3dk integration planning now covers embedded assembly/lint/LSP workflows, shared Mesen2 plumbing, and `.mlb` export.

## 0.7.0 (March 2026)
- **iOS Remote Control & Review**:
  - Bonjour discovery (`_yaze._tcp.`) auto-finds desktop instances on LAN.
  - Remote Room Viewer: browse and render all 296 dungeon rooms with overlay toggles and metadata.
  - Remote Command Runner: execute z3ed CLI commands from iPad with autocomplete and `--write` safety.
  - Annotation Review Mode: browse, create, edit, and delete room annotations with REST-based desktop sync.
  - Desktop Connection view with discovered hosts list, manual IP entry, and connection status pill.
- **Desktop HTTP API Expansion**:
  - Added command execution endpoint (`POST /api/v1/command/execute`) with CommandRegistry integration.
  - Added command catalog endpoint (`GET /api/v1/command/list`) exposing registered z3ed commands.
  - Added annotation CRUD endpoints (`GET/POST/PUT/DELETE /api/v1/annotations`).
  - Added macOS Bonjour service advertisement (`BonjourPublisher`) for desktop discovery.
- **Editor Completion & Workflow Polish**:
  - Sprite Editor undo/redo landed with snapshot-based actions and unit coverage.
  - Screen Editor undo/redo landed for dungeon map edits; toolbar wiring is re-enabled.
  - Message Editor replace/replace-all is now functional.
  - Music tracker stubs completed (Space rest/key-off, range delete, song rename popup).
- **Dungeon Editor Improvements**:
  - Added `DungeonUsageTracker` visual grids for blockset/spriteset/palette usage analysis.
  - Polished workbench/panel flow with clearer workflow status and return affordances.
  - Extended undo/redo coverage into adjacent dungeon map editing flows (screen editor integration).
- **Custom ROM Hack Features**:
  - Added desktop BPS patch export/import with CRC validation for distributable patch workflows.
  - Strengthened overworld hack editing safety via hard-delete semantics, list/filter/sort + duplicate/nudge iteration flow, and undo-backed batch workflows.
- **CLI Expansion**:
  - Added palette command set: `palette-get-colors`, `palette-set-color`, and `palette-analyze`.
- **Themed Widget System**:
  - Added `BeginThemedTabBar`/`EndThemedTabBar` for consistent styled tab bars across editors.
  - Adopted themed widget APIs in dungeon workbench, status bar, pixel editor, and screen editor.
- **Known deferred items (targeting 0.8.0)**:
  - Persistent overworld scratch pad.
  - Overworld eyedropper tool.
  - Graphics editor clipboard flow.
  - Music SPC/MML import workflow.
  - Final dungeon draw-routine registry dedupe/documentation cleanup.

## 0.6.2 (February 2026)
- **Release & Version Consistency**:
  - Synchronized version artifacts and app-facing release metadata to `0.6.2`.
  - Updated documentation current-release markers and coverage report headers.
- **Oracle Workflow & Validation**:
  - Continued hardening for Oracle smoke/preflight workflows and structural gating.
- **Project Bundle Reliability (`.yazeproj`)**:
  - Improved bundle unpack safety/cleanup behavior and dry-run validation flow.
  - Expanded bundle verify coverage with hash and manifest compatibility checks.
- **Editor UX Polish**:
  - Refined dungeon placement feedback and tile selector interaction ergonomics.

## 0.6.1 (February 2026)
- **Oracle Workflow & Validation**:
  - Added Oracle smoke/preflight command flows in `z3ed` and editor integration for faster structural checks.
  - Added D6 minecart threshold gating (`--min-d6-track-rooms`) to catch track-object regressions.
- **Project Bundle Reliability (`.yazeproj`)**:
  - Added bundle verify/pack/unpack command paths with structured JSON output.
  - Added safer unpack behavior: traversal rejection, failure cleanup by default, and `--keep-partial-output` opt-out.
  - Added `--dry-run` unpack validation and `--check-rom-hash` verification support.
- **Cross-Platform Hashing**:
  - Replaced platform-conditional hash behavior with portable SHA1 output for bundle verification consistency.
- **Dungeon/Overworld UX Improvements**:
  - Added live room limit indicators and improved placement feedback for objects/sprites/doors.
  - Added custom-object overlay visibility controls and D6 quick navigation in workbench.
  - Improved Tile16 selector usability with hover previews, ID jump/filter bar, range validation feedback, and explicit decimal input (`d:<id>`).

## 0.6.0 (February 2026)
- **GUI & Theming Modernization**:
  - Unified themed widget system (`ThemedButton`, `ThemedIconButton`, `SectionHeader`).
  - Standardized semantic color system for all editors (Dungeon, Overworld, Sprite, Graphics).
  - Smooth cross-category editor transitions with global fade effects.
  - Enhanced selection feedback with pulsing borders and animated corner handles.
  - Interactive hover previews in object and sprite selectors.
  - Theme-aware canvas background patterns and adaptive grid rendering.
  - Named workspace presets: Dungeon Master, Overworld Artist, Logic Debugger, Audio Engineer.
- Unified UndoManager with per-editor undo/redo (overworld, dungeon, graphics, music, message).
- SNES priority compositing in dungeon renderer with coverage masks.
- **Dungeon Object Drawing Parity**:
  - 100% vanilla object routine coverage (448/448 objects mapped across all 3 subtypes).
  - Expanded room effects: Moving_Water, Moving_Floor, Torch_Show_Floor, Red_Flashes, Ganon_Room.
  - SNES color math translucent blending for BG2 compositing (`(bg1 + bg2) / 2` with palette-aware nearest-color lookup).
  - 19 parity validation tests covering routine coverage, palette offsets, pit/mask objects, BothBG flags, water objects, room effects, and layer merging.
- Custom collision editor with JSON import/export.
- Water fill zone authoring with brush support.
- Entity drag-drop and selection inspector in dungeon editor.
- Object Tile Editor: visual 8x8 tile composition editor for dungeon objects with trace-based capture, source sheet atlas, per-tile property editing, and write-back to ROM and custom `.bin` files.
- Semantic color system replacing hardcoded ImGui style pushes.
- EventBus migration replacing legacy callback navigation patterns.
- ROM write fences and backup safety infrastructure.
- Viewport-relative sizing helpers for responsive dialogs.
- Dead code cleanup: SessionObserver, PanelManager callbacks, deprecated navigation APIs.

## 0.5.6 (February 5, 2026)
- Dungeon editor minecart overlays: collision tiles, track directions, sprite/stop validation.
- Minecart Track Editor audit: 32 slots, filler detection, missing-start warnings.
- Custom object preview/selection stability and layer-filter-respecting hover.
- Headless ImGui init hardening for editor tests.

## 0.5.5 (January 28, 2026)
- EditorManager refactored for better testability and architecture.
- New test suites: `EditorManagerTest` and `AsarCompilerTest`.
- Robust graphics sheet loading with grayscale fallback.
- Build system improvements: `yaze_core_lib` added, main entry point cleaned up.

## 0.5.4 (January 25, 2026)
- Mesen2 debug panel + CLI tools and bridge updates.
- HTTP API CORS + error handling, model registry caching with refresh support.
- CLI ROM/debug subcommands with sandbox mode for safe ROM edits.
- gRPC stability: SNES Read/Write, auto-init for SaveState/LoadState, Null audio fallback.
- Build/packaging: public headers moved to `inc/`, ccache setup in agent scripts.
