# Changelog

High-level release summary. For detailed notes, see
`docs/public/reference/changelog.md`.

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
