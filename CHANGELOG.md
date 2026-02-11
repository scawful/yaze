# Changelog

High-level release summary. For detailed notes, see
`docs/public/reference/changelog.md`.

## 0.6.0 (February 10, 2026)
- Unified UndoManager with per-editor undo/redo (overworld, dungeon, graphics, music, message).
- SNES priority compositing in dungeon renderer with coverage masks.
- Custom collision editor with JSON import/export.
- Water fill zone authoring with brush support.
- Entity drag-drop and selection inspector in dungeon editor.
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
