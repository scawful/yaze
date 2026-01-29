# Changelog

High-level release summary. For detailed notes, see
`docs/public/reference/changelog.md`.

## 0.5.4 (January 25, 2026)
- Mesen2 debug panel + CLI tools and bridge updates.
- HTTP API CORS + error handling, model registry caching with refresh support.
- CLI ROM/debug subcommands with sandbox mode for safe ROM edits.
- gRPC stability: SNES Read/Write, auto-init for SaveState/LoadState, Null audio fallback.
- Build/packaging: public headers moved to `inc/`, ccache setup in agent scripts.

## 0.5.5 (January 28, 2026)
- EditorManager refactored for better testability and architecture.
- New test suites: `EditorManagerTest` and `AsarCompilerTest`.
- Robust graphics sheet loading with grayscale fallback.
- Build system improvements: `yaze_core_lib` added, main entry point cleaned up.
