# Release Notes

## v0.8.0

**Type:** Dungeon Editor Completion — Rendering Parity + Workbench UX + Save Safety
**Status:** In development
**Date:** pending
**Release SHA:** pending

This is the Dungeon Editor completion milestone. Dungeon rooms now draw from the
ROM's own semantics rather than editor approximations, the workbench keeps the
room canvas as the stable center of the layout, every guarded write either
lands completely or leaves the ROM untouched, and the release gates test what
users actually download.

### 🏰 Dungeon Rendering
- Corrected ROM-driven placement and layer behavior for doors, thin floor and
  wall strips, corners, diagonal walls and ceilings, stairs, moving-floor and
  moving-wall objects, rails, and floor-copy objects. Strip extents, room
  header floor patterns, and lower-level stair routing now follow the
  disassembly instead of editor-side guesses.
- Matched dozens of individual object families against USDASM, including Somaria
  paths, pushable blocks, torches, hammer pegs, light beams, curtains, rupee
  and bombable floors, big key locks, prison cells, the Turtle Rock pipe, and
  bar corners. Canonical payload counts and object sizes were corrected in the
  process, so fixed-size objects no longer offer meaningless resize handles.
- Made the object registry the single authority for BG layer routing and
  dimensions, retiring a hard-coded object-ID heuristic and a duplicate legacy
  dimension switch that could disagree with it.
- Fixed dungeon palette slot mapping and made object previews, sprite previews,
  and the placement ghost use the room's own palette set, so browsing the
  selector shows what the room will actually look like.
- Added ROM-backed parser and renderer checks, independent Mesen fixtures for
  TableRock, BigHole, rails, bombable floors, and a west door, plus structural
  BG2 validation for vanilla HDMA water-control objects.

### 🧰 Dungeon Editor Workflow
- Kept the room canvas vertically stable: transient selection text and dynamic
  room tabs no longer push it around, and specialist tools moved into a
  variable-width **Tools** mode in the right inspector. **Pop out** remains for
  traditional floating panels.
- Made the toolbar, room navigation, Compare controls, Object Selector, and
  palette grid responsive, so a narrower window sheds chrome instead of
  shrinking the room.
- Made issue capture explicitly local and opt-in — only **Save Report** writes
  anything — and gave captured reports the selected object's real room-stream,
  layout, layer, and floor-header context.
- Gave each room presentation its own composite texture so the canvas, room
  matrix, issue report, and dungeon map preview can no longer overwrite one
  another or leak stale rooms into the next dungeon.
- Added editable dedicated spawn points, room event slot conflict warnings, and
  stateful chest classification.

### 🧪 Oracle & Custom Dungeon Assets
- Added a persistent **Custom Assets** mode covering all 21 fixed Oracle runtime
  slots, grouped into Tracks + Props, Ice Props, and Boss Bodies, with tile
  layout editing, room placement, and direct minecart route navigation.
- Removed the incorrect implicit track-corner alias, so ordinary wall corners
  stay wall corners unless a project supplies an exact same-ID override. This
  is what was wrong in Mushroom Grotto.
- Published custom asset and minecart track sources with path confinement,
  exact-source comparison, atomic replacement, rollback, and decoded readback.
  WASM fails closed rather than pretending to publish.
- Added a non-destructive project asset refresh so picking up changed external
  art no longer discards in-progress edits.

### 🛡️ Save Safety
- Required Hack Manifest ownership for dungeon palette, Palette Editor, room
  property, and Object Tile Editor ROM writes, with source provenance and
  shared-source detection so one edit cannot silently rewrite another hack's
  bytes.
- Made room-property, collision JSON, and track-collision writes all-or-nothing,
  and blocked unsafe Screen Editor, graphics sheet, minecart draft, and
  unapplied tile-layout saves instead of writing partial state.
- Included unapplied Object Tile Editor layouts in dirty detection, so Save and
  Apply Room now wait until those edits are applied or discarded.
- Hardened ROM backup restoration, added explicit restored-backup discard, and
  reported project-only session changes separately from ROM changes.

### 🔧 z3ed CLI
- Added `dungeon-get-palette` for the full room palette-set mapping, raw colors,
  and every room sharing the selected global US/OOS palette.
- Added dry-run-first `dungeon-set-palette-color` with mapping/color
  compare-and-swap, manifest ownership, clean-disk baseline, a two-byte write
  fence, required backup, atomic save, whole-ROM diff, and external readback.
  Legacy `palette-set-color --write` is disabled because it could report an
  in-memory change as persisted; its preview still works.
- Added manifest-safe dungeon door and pot-item edits, room object description,
  dedicated spawn-point reporting, idempotent collision imports, and
  ROM-alias-safe collision export.
- Added asset-aware `z3ed --self-test` so a packaged CLI proves it can find its
  runtime assets.

### 🎨 Appearance & Editor Shell
- Added five verified editor themes: Blood Moon, Catppuccin Mocha, Dracula,
  Rosé Pine, and Temple of Time, with upstream MIT notices packaged alongside.
- Made `.theme` files authoritative for the colors they declare. A theme that
  deliberately asks for black text or background keeps it, while genuinely
  omitted fields are still filled in — including the five derived color sources
  that previously handed black to link text, histograms, and highlights.
- Reworked the welcome screen into a compact start card with Start and Recents
  readable without scrolling, and Resume, Prototype Research, and Assembly
  Editor promoted out of a submenu.
- Repaired the editor chooser dashboard: `--startup_dashboard` now works, cards
  are legible on the light themes, Display Density has effect, the advertised
  shortcut matches the real Ctrl+E binding, and the Performance Dashboard no
  longer draws twice per frame.
- Renamed Window Finder to **Find Window…** and surfaced its shortcut through
  Help → **Keyboard Shortcuts**.

### 🖥️ Emulator, iOS & Platform
- Added TCP endpoint support to the Mesen socket client alongside Unix domain
  sockets, so debugging no longer requires a local socket path.
- Restored the iOS device build and the remote desktop and review views.
- Routed the normal macOS Quit menu item through ordered application shutdown.

### 🧱 Release Validation
- Added a dedicated Release-config native test build for Linux, macOS, and
  Windows that fails on zero tests, missing suites, wrong-configuration
  binaries, or empty JUnit results.
- Bounded reusable CI builds to four workers and separated build caches by
  configuration to reduce runner pressure and cache pollution.
- Consolidated pull-request WASM validation into one bounded build/browser
  smoke gate, including public-header changes and exact production cache keys.
- Added portable-package layout, manifest, dependency, executable-version, and
  lifecycle checks: FHS TGZ/DEB payloads, real APT install/purge, relocated
  macOS bundles, and Windows ZIP/NSIS execution.
- Made nightly installs validate before activation: staged directories, both
  executables present and running `--version`, macOS signing verified after
  resource copying, and an atomic `current` symlink swap only on success.

### 📚 Documentation
- Replaced stale feature percentages with one canonical editor readiness matrix
  and an evidence-based tester readiness contract that distinguishes component,
  ROM readback, app-path, GUI smoke, and manual package evidence.
- Generated the ALTTP quick reference and a SNES hardware reference from pinned
  usdasm and jpdasm sources, with a check that fails when they go stale.

### Validation Snapshot
Figures below are per-checkpoint evidence recorded at merge time. A single
full-suite run against the final release head is **pending**.
- ROM parser/drawer parity and room fingerprint tests pass for the covered
  vanilla rooms and objects.
- `z3ed dungeon-object-validate` reports `0` mismatches across `1190` validated
  objects for the canonical vanilla ROM used by the dungeon parity audit.
- The Release test gate executes `3286` discovered stable tests locally on
  macOS instead of accepting an empty test run.
- The most recent full local unit run in this line reported `3610/3610` runnable
  tests passing on macOS, with one profiling test disabled.
- Final tag SHA, packaged-candidate digests, and hosted `Release` workflow run
  links are **pending** until the remaining stabilization pull requests merge.

### Landing Next
These changes are open and reviewed but not yet merged; nothing above depends on
them.
- Portable project bundle compatibility: CRLF descriptors and both the desktop
  `rom_sha1` and iOS `romChecksum` manifest fields (#207).
- `dungeon-remove-object` with exact stream-identity guards (#234).
- Mesen CPU register parsing against the live lowercase socket schema (#235).
- Draw-routine symbology badges and tooltips in the dungeon object selector
  (#236).
- A tracked distributable theme manifest so packages ship only allowlisted
  themes (#237).

### Known Limits
- Full emulator 1:1 parity is not claimed. Static water, ice, bar, remaining
  small-corner objects, and more door families still need independent
  witnesses. Vanilla `0xD8` and `0xDA` remain structural-only because they
  control HDMA.
- The Custom Assets browser exposes only the 21 fixed Oracle slots. Arbitrary
  custom object IDs, the externally DMA-loaded Kydreeok and Manhandla pixel
  graphics, and a general project object catalog are out of scope.
- Oracle custom source publication requires a subsequent Oracle build and
  in-game check. `Save ROM` alone cannot prove published assets reached the
  game. Other ROM hacks need a named compatibility test before being described
  as supported.
- Headless package smoke checks do not replace hands-on GUI launch/quit on each
  desktop platform. Windows signing and macOS universal, Developer ID,
  notarization, and Gatekeeper acceptance remain separate release gates.

---

## v0.7.2

**Type:** Dungeon RC + Fail-Closed Save Safety + Build/CI Hardening
**Date:** 2026-07-17

### 🏰 Dungeon RC Stabilization
- Rendered dungeon object and sprite previews by default so selector browsing
  starts from visual room-context feedback instead of fallback symbols.
- Kept Workbench-local dungeon tools available alongside standalone windows,
  making mixed Workbench/Window editing safer during longer dungeon sessions.
- Persisted entrance and special-object save domains through the dungeon save
  path so `Save ROM` covers the room state needed by real editing workflows.
- Hardened door placement to reject positions outside the ALTTP/USDASM
  12-entry door tables instead of accepting clamped-but-invalid slots.
- Canonicalized room-object names and resource-label export from
  `room_object.h`, including newly named logic-only and rare decor objects.
- Preserved zero-tile `DrawNothing` logic objects as no-payload cases while
  keeping the conservative fallback for uncataloged drawable objects.
- Added editable pit-damage room membership controls and protected the
  pushable-block loader from invalid pointer operands and table-capacity
  overruns.
- Added ROM-backed regression coverage for pit-damage persistence,
  pushable-block boundaries, and a pinned room `0x001` BG1/BG2 object-overlap
  pixel.

### 🌎 Overworld Follow-through
- Added a right-click Tile16 sampling action to the overworld canvas context
  menu, covering the common eyedropper workflow without opening the Tile16
  editor first.

### 🛡️ Fail-Closed Save Safety
- Made multi-editor saves transactional across the complete ROM, so a late
  validation, backup, or file-write failure rolls back the attempted save and
  keeps the user's edits available to retry.
- Preserved the selected Save As destination across ROM-hash, pot-item, and
  ASM-conflict confirmations and rejected stale confirmations after switching
  ROM sessions.
- Made unsupported dungeon object, sprite, pot-item, and chest growth stop
  with an actionable error rather than guessing relocation space or partially
  rewriting shared streams.
- Strengthened expanded-message, custom-overworld, palette, project-path,
  manifest, and hash validation with vanilla and Oracle regression coverage.

### 🧱 Build & CI Stabilization
- Fixed the WASM/browser build after the 0.7.1 editor reorg by excluding the stale `yaze_debug_inspector.cc` path that still referenced removed `PanelManager`-era editor APIs.
- Added `project_graph_tool.cc` back to the WASM AI source list so `ProjectGraphTool` links correctly in browser builds.
- Skipped POSIX-shell-dependent project-action tests on Windows, keeping the Windows Core matrix green while a real cross-platform script-runner rewrite is deferred.
- Stabilized fork pull-request checks, WASM builds, and the memory-sanitizer
  gate used by the release matrix.

### 🌐 Localization & Desktop Reliability
- Added the internationalization catalog pipeline and French localization.
- Initialized native file dialogs explicitly on Linux and Windows.
- Hardened session restore, SDL startup, asset lookup, configuration loading,
  ROM handling, and CLI error paths.

### 🧰 CLI & Agent Tooling
- Updated `scripts/z3ed` to resolve binaries under `build/presets/<preset>/bin/`, which restores `z3ed` discovery for CI presets and preset-specific local builds.

### 📝 Release Metadata & Post-Tag Hygiene
- Advanced trunk version metadata to `0.7.2` after the `v0.7.1` tag.
- Aligned post-release docs metadata so future release notes and generated release bodies stay consistent.

### Deferred Follow-Up
- Complete the 0.8.0 dungeon milestone: remaining rare-object parity, pit/block
  first-class encoders, and deeper BG1/BG2 overlay-stream validation.
- Add manifest-backed copy-on-write allocation for shared dungeon streams so
  safe growth can proceed after the 0.7.2 fail-closed containment release.
- Refresh or remove the stale web debug inspector against the current post-reorg editor APIs.
- Replace POSIX-shell-only project-action test scaffolding with a cross-platform test harness.

---

## v0.7.1 (April 2026)

**Type:** Packaging + Welcome Screen Overhaul + Dungeon Editor Parity/Polish
**Date:** 2026-04-19

### Welcome Screen & Project Startup
- Added a guided New Project flow with clearer startup/project creation affordances.
- Added async ROM metadata scanning for recent projects plus pin/rename/notes actions.
- Added a short undo window for recent-project removal and command-palette entry points for welcome actions.

### Dungeon Editor Parity & Polish
- Restored BG1/BG2 layout routing while preserving pit/mask semantics.
- Fixed real-room `0x034` rendering for single-tile payloads.
- Added replay-geometry selection bounds for more accurate object hitboxes.
- Added ROM-backed parity tests and room render snapshots.
- Simplified workbench inspector/navigation flows, kept hidden room state sparse, and lazily materialized room state.

### Performance & Footprint
- Added lazy session-editor construction.
- Deferred hidden full-mode asset loads.
- Trimmed eager overworld bitmap memory.
- Released unused room buffer textures and split render-target texture creation.

### CLI / Build / CI
- Fixed the WASM dungeon tile-row build path.
- Added overworld map ID validation before ROM access.
- Hardened Linux GUI smoke-run binary path handling.

### Deferred to 0.8.0
- z3dk integration proposal tracks embedded assembler/LSP/lint workflows, unified Mesen2 client work, and `.mlb` export.

---

## v0.7.0 (March 2026)

**Type:** Feature Completion + iOS Remote Control + API Expansion
**Date:** 2026-03-03

### 📱 iOS Remote Control
- Added Bonjour LAN discovery (`_yaze._tcp.`) for auto-detecting desktop instances.
- Added Remote Room Viewer for all 296 dungeon rooms with overlay/metadata controls.
- Added Remote Command Runner with command catalog autocomplete/history and `--write` safety confirmation.
- Added Annotation Review Mode with create/edit/delete workflows synced through desktop REST endpoints.
- Added desktop connection controls with discovered hosts, manual IP fallback, and persistent status pill.

### 🌐 Desktop HTTP API
- Added `POST /api/v1/command/execute` for remote z3ed command execution.
- Added `GET /api/v1/command/list` for command metadata and autocomplete catalogs.
- Added `GET/POST/PUT/DELETE /api/v1/annotations` for annotation CRUD against `annotations.json`.
- Added Bonjour publisher integration for desktop service advertisement on macOS.

### 🧰 Editor Completion (0.7.0 must-ship + should-ship)
- Added Sprite Editor undo/redo with snapshot actions and unit tests.
- Added Screen Editor undo/redo and restored toolbar wiring for map edits.
- Added Message Editor replace + replace-all behavior.
- Completed music tracker stubs (rest/key-off insertion, range delete, song rename popup).

### 🏰 Dungeon Editor Improvements
- Added `DungeonUsageTracker` visual usage grids for blockset/spriteset/palette analysis.
- Polished dungeon workbench/panel workflow (mode clarity, status badges, and return affordances).
- Extended undo/redo coverage into adjacent dungeon map edit workflows.

### 🛠️ Custom ROM Hack Features
- Added desktop BPS patch export/import with CRC validation for patch-based release flows.
- Strengthened overworld hack editing reliability with hard-delete semantics, list/filter/sort + duplicate/nudge iteration flow, and undo-backed item workflows.

### 💻 CLI Expansion
- Added palette command set (`palette-get-colors`, `palette-set-color`, `palette-analyze`) for scripted ROM-hack iteration and automation.

### 🎨 UI and Theming
- Added themed tab-bar primitives (`BeginThemedTabBar`/`EndThemedTabBar`).
- Adopted themed widgets in key editor surfaces (dungeon workbench, status bar, pixel editor, screen editor).

### ✅ Validation Snapshot
- `ctest --preset mac-ai-unit --output-on-failure` passed on the release candidate revision.
- `ctest --preset mac-ai-integration --output-on-failure` passed on the release candidate revision.

### 🗂️ Deferred to 0.8.0
- Overworld scratch-pad persistence.
- Overworld eyedropper mode.
- Graphics editor clipboard workflow.
- Music SPC/MML import.
- Final dungeon draw-routine registry dedupe/docs cleanup.

---

## v0.6.2 (February 2026)

**Type:** Release Consistency + Bundle/Validation Hardening
**Date:** 2026-02-24

### 🔄 Version Consistency
- Updated all current-release markers and app-facing metadata to `0.6.2`.
- Aligned release/coverage documentation with the current build version.

### 📦 `.yazeproj` Bundle Workflow
- Hardened bundle unpack cleanup behavior on invalid bundle failures.
- Expanded dry-run validation and bundle verification coverage.

### 🧪 Oracle Validation
- Continued smoke/preflight workflow improvements for Oracle development loops.

### 🎛️ Editor UX
- Continued refinement of dungeon placement feedback and tile selector usability.

---

## v0.6.1 (February 2026)

**Type:** Oracle Validation + Bundle Workflow + Editor UX Hardening
**Date:** 2026-02-24

### 🧪 Oracle Validation Workflow
- Added Oracle-focused smoke/preflight command flows and reporting improvements.
- Added D6 track-room threshold gating (`--min-d6-track-rooms`) to detect minecart regressions in structural checks.

### 📦 `.yazeproj` Bundle Workflow
- Added project bundle verify/pack/unpack command support in `z3ed`.
- Added safer unpack defaults:
  - rejects traversal entries,
  - cleans partial output on invalid bundle failure,
  - supports `--keep-partial-output` for debugging.
- Added `--dry-run` unpack mode for non-writing structural checks.

### 🔐 Integrity & Hashing
- Standardized SHA1 generation across platforms for bundle verification.
- Added bundle ROM hash verification (`--check-rom-hash`) and robust hash normalization.

### 🎛️ Editor UX
- Improved dungeon placement feedback near/at entity limits.
- Added custom-object overlay controls and faster D6 room navigation in dungeon workbench.
- Improved Tile16/tile selector interactions with hover previews, filter/jump UX, range validation feedback, and explicit decimal tile ID input (`d:<id>`).

---

## v0.6.0 (February 2026)

**Type:** Undo System + Dungeon Compositing + UI Overhaul
**Date:** 2026-02-10

### Undo/Redo
- Unified `UndoManager` embedded in `Editor` base class.
- Per-editor undo/redo: overworld, dungeon, graphics, music, message.
- Custom collision and water fill undo actions.

### Dungeon Editor
- SNES priority compositing with coverage masks.
- Entity drag-drop with selection inspector.
- Custom collision editor with JSON import/export.
- Water fill zone authoring with brush radius.
- Mutation tagging by domain (tile objects, collision, water fill).

### UI
- Semantic color system replacing hardcoded style pushes.
- EventBus migration replacing legacy callback navigation.
- Right panel manager and sidebar simplification.
- Viewport-relative sizing helpers for responsive dialogs.

### ROM Safety
- Write fence stack rejecting out-of-bounds writes.
- Dirty custom collision save without full room reload.

### Cleanup
- Removed `SessionObserver`, `PanelManager` callbacks, legacy navigation APIs.
- Removed deprecated `SetMutationHook` alias.
- Archived 20 stale internal docs.

---

## v0.5.6 (February 2026)

**Type:** Dungeon Editor UX + Minecart Tooling
**Date:** 2026-02-05

### ⛏️ Dungeon & Minecart Tooling
- Configurable minecart collision IDs and track object IDs (project-level `[dungeon_overlay]` settings).
- Track collision overlay with legend + per-tile direction arrows (straights, corners, T-junctions, switches).
- Minecart Track Editor audit: 32 slots, filler detection, missing-start warnings, and room coverage reporting.
- Minecart sprite overlay to flag carts placed off stop tiles.
- Camera quadrant overlay to plan fast cart routes.

### 🎯 Object Editing UX
- Custom object previews keyed by subtype and bounds derived from custom extents.
- Hover/selection now respects the active layer filter.

### 🧪 Stability
- Headless ImGui initialization hardened for editor tests.

---

## v0.5.5 (January 2026)

**Type:** Editor Foundations + Stability
**Date:** 2026-01-31

### 🧰 Editor & Architecture
- Modernized `EditorManager` for better isolation and testability.
- Introduced `yaze_core_lib` to separate core logic from the app shell.

### 🧪 Tests
- Added `AsarCompilerTest` and `EditorManagerTest` suites.

### 🎨 Graphics
- Fallback to grayscale palette for graphics sheets missing a palette.

### 🧱 Build
- Cleaned up CMake entry points and presets; unified `main` entry point logic.

---

## v0.5.4 (Release Candidate January 2026)

**Type:** Stability + Mesen2 Debugging
**Date:** 2026-01-25

### 🐞 Mesen2 Debugging
- New Mesen2 debug panel in the Agent editor (socket picker, overlay controls, save/load, screenshot capture).
- Mesen2 debug shortcut (Ctrl+Shift+M) and socket list refresh on panel open.
- New z3ed `mesen-*` CLI commands for live Mesen2 inspection and control.

### 🤖 AI & HTTP API
- Model registry caching with `/api/v1/models?refresh` support.
- CORS + error handling for HTTP API endpoints and `/symbols` format validation.
- Normalized OpenAI base URL detection for local OpenAI-compatible servers.

### 🧰 Desktop UX
- Fix message editor preview/font atlas rendering after ROM load.
- Sync editor/panel context on category switches to avoid blank views.

### 📦 Nightly Builds
- Normalize macOS nightly bundle layout so launchers resolve `yaze.app`.
- Refresh wrapper detection to handle alternate app locations.

### 🧾 Versioning
- Sync version strings across docs, build config, and project defaults to 0.5.4.

---

## v0.5.3 (Released January 2026)

**Type:** Build, WASM & Code Quality
**Date:** 2026-01-20

### 🔧 Build & Release
- Fix release validation scripts for DMG packaging.
- Create VERSION file as canonical source of truth (0.5.3).
- Update CMakeLists.txt fallback version.
- Fix wasm-ai CMake preset (add `YAZE_ENABLE_AI=ON` for AI_RUNTIME dependency).

### 🌐 WASM/Web
- Service worker: Stream responses instead of buffering (fixes memory spikes for large assets).
- Service worker: Throttle cache eviction to once per 60 seconds (reduces O(n) overhead).
- Filesystem tool: Tighten path guard to prevent `/.yazeevil` bypass attack.
- Build tool: Fix boolean output format for JSON responses.

### 🤖 AI & CLI
- Add LMStudio support with configurable `--openai_base_url` flag.
- Allow empty API key for local OpenAI-compatible servers.

---

## v0.5.2 (Released January 2026)

**Type:** Build Fix
**Date:** 2026-01-20

### 🔧 Build
- Fix build when `YAZE_AI_RUNTIME` is disabled.
- Add proper guards around AI runtime-dependent code paths.

---

## v0.5.1 (Released January 2026)

**Type:** UX + UI Polish
**Date:** 2026-01-20

### ✨ UI Modernization (ImHex-inspired)
- Restructure menus and fix sidebar toggle icon.
- Add comprehensive UI polish with animations and theming enhancements.
- Cross-platform theme file system with `~/.yaze/themes/` support.
- Complete ImHex UI modernization phases 2-5.
- Animated hover effects for themed widget buttons.
- List virtualization and expanded command palette.
- Lazy panel initialization with `OnFirstDraw` hook.
- GUI animation system with smooth hover effects.

### 🧩 Architecture
- ContentRegistry panel self-registration with `REGISTER_PANEL` macro.
- Core UI events and texture queue budget.
- Extract `yaze_cli_core` library for shared CLI infrastructure.
- `ZoomChangedEvent` published from Canvas zoom methods.

### 🗂️ Storage & Paths
- Unified app data under `~/.yaze` across desktop/CLI.
- Web build storage consolidated under `/.yaze` (IDBFS).
- Project management panel now surfaces storage paths.

### 🧭 Versioning
- Added `VERSION` file as the source of truth for build/versioning.

---

## v0.5.0 (Released January 2026)

**Type:** Platform Expansion & Stability
**Date:** 2026-01-10

### 🧩 Graphics & Palette Accuracy
- Fixed palette conversion and Tile16 tint regressions.
- Corrected palette slicing for graphics sheets and indexed → SNES planar conversion.
- Stabilized overworld palette/tilemap saves and render service GameData loads.

### 🧭 Editor UX & Reliability
- Refined dashboard/editor selection layouts and card rendering.
- Moved the layout designer into a lab target for safer experimentation.
- Hardened room loading APIs and added room count reporting for C API consumers.
- Refreshed welcome screen and help text across desktop/CLI/web to spotlight multi-provider AI and CLI workflows.

### 🤖 Automation & AI
- Added agent control server support and stabilized gRPC automation hooks.
- Expanded z3ed CLI test commands (`test-list`, `test-run`, `test-status`) and tool metadata.
- Improved agent command routing and help/schema surfacing for AI clients.
- Added OpenAI/Anthropic provider support in z3ed and refreshed AI provider docs/help.
- Introduced vision refiner/verification hooks for AI-assisted validation.

### 🌐 Web/WASM Preview
- Reduced filesystem initialization overhead and fixed `/projects` directory handling.
- Hardened browser terminal integration and storage error reporting.

### 📦 Platform & Build
- Added iOS platform scaffolding (experimental).
- Added build helper scripts and simplified nightly workflow.
- Refreshed toolchain/dependency wiring and standardized build directory policy.
- Hardened Windows gRPC builds by forcing the Win32 macro-compat include for gRPC targets.
- Fixed Linux static link order for test suites and tightened Abseil linkage.
- Release artifacts now include a release-focused README and exclude internal test helper tools.
- Windows ships as a portable zip (no installer) with trimmed runtime DLLs.

### 🧪 Testing
- Added role-based ROM selection and ROM-availability gating.
- Stabilized rendering/benchmark tests and aligned integration expectations.
- Added AgentChat history/telemetry and agent metrics unit coverage; expanded WASM debug API checks.

---

## v0.4.0 - Music Editor & UI Polish
**Released:** November 2025

- Complete SPC music editing infrastructure.
- EditorManager refactoring for better multi-window support.
- AI Agent integration (experimental).
