# Release Notes

## v0.8.0

**Type:** Dungeon Editor Milestone — Rendering Parity + Workbench UX + Save Safety
**Status:** In development
**Date:** pending
**Release SHA:** pending

This is the Dungeon Editor milestone. More dungeon objects now draw from their
USDASM routines, the Workbench keeps the room canvas stable, guarded dungeon,
message, and palette writes check more before touching the ROM, and release
packages gained per-platform layout and install checks. Many of those checks
refuse work that 0.7.2 accepted, so read **Upgrading from 0.7.2** first.

### ⚠️ Upgrading from 0.7.2
Read this before upgrading a project or a script. Each item was checked against
the `v0.7.2` tag and current `master`. The full list, with exact error messages
and every affected command, is in `docs/public/reference/changelog.md` under
**Upgrading from 0.7.2**.

**Saves that used to succeed can now stop with an error**
- ROM saves stop while Graphics Editor sheet or Screen Editor edits are pending,
  and dungeon **Save**/**Apply Room** stop while Object Tile Editor edits are
  unapplied or Minecart Track Editor drafts are unpublished.
- Dungeon object, torch, pushable-block, entrance, and WaterFill saves now fail
  on data 0.7.2 accepted: object IDs, positions, or sizes that cannot be
  encoded (0.7.2's writer silently capped Type 1 sizes above 15), a third
  torch/block layer, an empty or conflicting block table, entrance room IDs
  outside `0x000`-`0x127`, and WaterFill room IDs `0x100`-`0x127` (0.7.2 kept
  only the low byte).
- With a Hack Manifest loaded, many more dungeon and palette writes are checked
  before writing, so `write_policy` `block` can stop saves that 0.7.2 allowed.

**z3ed scripts**
- `message-write` and `message-import-bundle --apply` require `--project` with a
  loaded manifest, and `--apply` now exits non-zero when a project, manifest,
  write-policy, write, save, or readback check fails (a missing ROM or parse
  errors without `--strict` still exit `0`).
- `dungeon-import-custom-collision-json` and `dungeon-import-water-fill-json`
  now **write the ROM file** when run without `--dry-run`; 0.7.2 changed only
  memory unless `--sandbox` was used.
- Dungeon write commands require a completed ROM backup, and
  `dungeon-set-room-property` no longer resets the room's other header bytes.
- JSON output changed for `--spawn` on `dungeon-get-entrance`/`entrance-info`,
  `dungeon-object-validate`, and `dungeon-place-object`. `project-bundle-verify
  --check-rom-hash` now checks manifests that carry only `romChecksum` and can
  fail them. `palette-set-color --write` is disabled; use
  `dungeon-set-palette-color`.

**Projects and manifests**
- A malformed `protected_regions`, `messages`, or `minecart_tracks` section
  makes the whole Hack Manifest fail to load, and hook regions now take
  precedence over `owned_banks`.
- One ROM file backs one session on every open path. New Project writes
  `<name>.yaze` immediately and refuses to replace an existing file. Restoring a
  backup stages it until **Save ROM**.
- CRLF `project.yaze` files now load their settings (0.7.2 ignored all of them,
  including write-policy flags), and a lone carriage return is rejected.
- Custom object paths must stay inside `custom_objects_folder`, and the minecart
  track editor needs a manifest `minecart_tracks.source`.

**Editor and packages**
- The first launch after upgrading shows the Dungeon Workbench and closes the 10
  standalone Dungeon windows it replaces, including Room List, Object Selector,
  and Palette Editor. ROM tools moved from File to Tools > ROM Analysis, and
  plain-key shortcuts no longer fire with modifiers held.
- Lower-layer pushable blocks in rooms `0xA8`, `0x66`, and `0x2C` now load 64
  tiles higher, at their real position on BG2, and several objects draw with
  their USDASM footprint.
- `.theme` files keep the colours they declare, and plot colours change in the
  built-in presets.
- Linux packages use `/usr/bin` and `/usr/share/yaze`, and Windows and Debian
  download names changed.

### 🏰 Dungeon Rendering
- Corrected ROM-driven placement and layer behavior for doors, thin floor and
  wall strips, corners, diagonal walls and ceilings, moving walls, and
  floor-copy objects. The `_plus3` solid strips and `_plus13`/`_plus12` rail
  walls now draw from the object's own origin instead of 3, 13, or 12 tiles
  away. Conditional edge and cap routines check whether a layout or object tile
  owns each position. Lower straight stairs now raise the priority of a BG1
  column next to the staircase, and spiral stairs raise the priority of the
  tiles just left and right of the staircase (BG1 for upper spirals, BG2 for
  lower spirals). Neither paints over tiles.
- Matched dozens of individual object families against USDASM, including Somaria
  paths, pushable blocks, torch codecs, hammer pegs, light beams, curtains,
  rupee and bombable floors, big key locks, prison cells, moving walls, the
  Turtle Rock pipe `0xFDC`, and exact 4-word bar-corner payloads. Fixed-size
  objects no longer offer meaningless resize handles.
- Removed the hard-coded `IsAllBgsObjectId` list. BothBG routing now comes
  from draw-routine registry metadata plus explicit routing for stairs and fixed
  facades, and ObjectDrawer's duplicate dimension switch now uses
  `DimensionService`.
- Fixed dungeon palette slot mapping and made object previews, sprite previews,
  and the placement ghost use the room's own palette set, so browsing the
  selector shows what the room will actually look like.
- Added ROM-backed parser and renderer checks, independent Mesen fixtures for
  TableRock, BigHole, rails, bombable floors, and a west door, plus structural
  BG2 validation for vanilla HDMA water-control objects.

### 🧰 Dungeon Editor Workflow
- Kept the room canvas vertically stable by removing the selection action shelf
  and recent-room tab strip from above it. Recent rooms moved to a toolbar
  popup, and the Tools inspector's icon strip became one tool chooser grouped
  into Edit, Room, and Review. A new **Pop out** (or **Show window**) opens a
  tool standalone, and each tool is drawn in only one place at a time.
- Workbench toolbar actions that no longer fit now move into an overflow menu
  instead of disappearing, including Compare with its room picker, direct room
  ID, **Swap Rooms**, **Sync View**, and **End Compare**. The Object Selector
  gained a stream filter and a **More** menu, and the dungeon palette grid now
  adapts to 16/8/4/2/1 columns.
- Made issue capture local and opt-in: reports are no longer auto-saved on
  open, copy, screenshot, or close, and only **Save to Issue Log** writes the
  report. The object trace now replays the room-stream objects before the
  selected one, with layout tilewords, layer, and floor graphics context.
- Gave the room canvas, room selector and matrix, and Dungeon Map previews their
  own composite textures instead of sharing one per room, so they can no
  longer overwrite one another. Loading a dungeon into Dungeon Map now clears
  the previous dungeon's room type badges, stair and holewarp connections, and
  room positions.
- Spawn points `0x00`-`0x06` are now edited through their dedicated spawn-point
  record instead of the regular entrance fields. The dungeon validator now warns
  when a stateful chest comes after a big-key lock in the object stream or when
  a room uses more chest/lock event slots than the engine supports, replacing
  the old chest-count error.
- Object Selector cards show a draw-routine badge: a direction or category
  glyph, or `C`/`K`/`B`/`P` for chests, big key locks, bombable floors, and
  prison cells, with the routine family and base pattern size in the tooltip.

### 🧪 Oracle & Custom Dungeon Assets
- Replaced the modal Custom Object Workshop with a non-modal **Custom Assets**
  mode covering the 21 fixed Oracle runtime assets (Tracks + Props, Ice Props,
  and new Boss Bodies slots for `0x54`), with **Edit Tile Layout** (desktop
  only), **Place in Room**, and a **Minecart Routes & Collision** shortcut on
  track slots. It edits existing slots only.
- Removed the track-corner alias that drew wall-corner objects `0x100`-`0x103`
  with object `0x31` corner assets in projects that mapped `0x31`. Those IDs
  now stay wall corners unless the project maps the same ID directly.
- Published custom asset and minecart track sources with path confinement,
  exact-source comparison, atomic replacement, rollback, and decoded readback.
  WASM fails closed rather than pretending to publish.
- **Custom Assets > Reload Assets** (formerly Reload Workshop) now also
  refreshes external Oracle sprite preview art in every room, Workbench, and
  comparison viewer, stays available when Custom Objects is disabled, and
  keeps unsaved room and tile edits.

### 🛡️ Save Safety
- When a project Hack Manifest is loaded, dungeon palette saves, Palette Editor
  saves, and Object Tile Editor ROM writes are checked against it, and
  `z3ed dungeon-set-room-property --manifest` checks room-header and message-ID
  bytes. Object Tile Editor writes also record their source and detect tile
  sources shared by several objects.
- Made z3ed `dungeon-set-room-property`, `dungeon-generate-track-collision`,
  `dungeon-import-custom-collision-json`, and `dungeon-import-water-fill-json`
  roll back on a failed write or save, and made Minecart Track Editor collision
  generation transactional.
- Made ROM backup restore refuse while ROM edits are pending, accept only
  managed backups of the active ROM, and stage the restored ROM as unsaved
  until Save ROM, with a **Discard Restored Backup** button. Session tabs now
  mark a session Modified when only project settings or project editor drafts
  are unsaved.

### 🔧 z3ed CLI
- Added `dungeon-get-palette` for the full room palette-set mapping, raw colors,
  and every room sharing the selected global US/OOS palette.
- Added dry-run-first `dungeon-set-palette-color` with mapping/color
  compare-and-swap, manifest ownership, clean-disk baseline, a two-byte write
  fence, required backup, atomic save, whole-ROM diff, and external readback.
  Legacy `palette-set-color --write` is disabled because it could report an
  in-memory change as persisted; its preview still works.
- Added `dungeon-set-door-type` and `dungeon-set-pot-item` (dry-run by default,
  expected-value checks, required `--manifest`) and `dungeon-list-pot-items`,
  `--include-objects` on `dungeon-describe-room`, and the dedicated spawn-point
  schema for `--spawn` on `dungeon-get-entrance` and `entrance-info`. Custom
  collision imports now leave unchanged rooms alone, and collision export
  refuses to write over the ROM file.
- `z3ed --self-test` now checks that two representative runtime assets
  (`agent/prompt_catalogue.yaml` and an Overworld patch) resolve from the
  installed package layout.
- Added `dungeon-remove-object`, which removes one ordinary room object only
  when its stream index, ID, position, size, and layer all match the expected
  values. It is a dry run unless `--write` is given.
- `project-bundle-verify --check-rom-hash` accepts the iOS `romChecksum`
  manifest field alongside `rom_sha1`.

### 🎨 Appearance & Editor Shell
- Added five editor themes: Blood Moon, Catppuccin Mocha, Dracula, Rosé Pine,
  and Temple of Time, with the upstream MIT notices for Catppuccin, Dracula, and
  Rosé Pine kept in `assets/themes/THIRD_PARTY_NOTICES.md`. The macOS app bundle
  includes only the themes listed in `assets/themes/distributable-themes.txt`,
  and CI checks that tracked themes match that list.
- Theme files now get smart defaults for colors they leave out, including
  `accent`, `error`, `warning`, `success`, and `info`. Omitted borders,
  scrollbars, table colors, links, histograms, highlights, and modal backgrounds
  are no longer black on file-loaded themes. Colors a `.theme` file declares
  are never replaced, so a deliberate black border or highlight is kept.
- Reworked the welcome screen into a compact start card whose Start and Recent
  panes never scroll: the Recent list is cut to what fits, and optional Start
  rows drop on small windows. The Release History card became a Release notes
  link, and the resume button now names the most recent available ROM or
  project.
- Repaired the editor chooser dashboard: `--startup_dashboard=hide` now also
  suppresses the chooser on most automatic opens after a ROM or project loads
  (it can still appear when opening from the welcome screen), cards are legible
  on light themes, Display Density has effect, the shortcut hint says Ctrl+E
  instead of F1, and the Performance Dashboard no longer draws twice per frame.
- Renamed Tools > Window Finder to **Find Window…** (Ctrl+P). Help →
  **Keyboard Shortcuts** now opens the shortcuts browser instead of Settings
  and is bound to Ctrl+Shift+/.
- The right sidebar drawer shows an icon strip below its header with one icon
  per switchable drawer. The header can also show a context badge: agent status
  for AI Agent, an unread count for Notifications, a lock icon for locked
  Properties, and the active editor's name for Help.

### 🖥️ Emulator, iOS & Platform
- Added TCP endpoint support to the Mesen socket client alongside Unix domain
  sockets, so debugging no longer requires a local socket path.
- Fixed the iOS device build (preset build paths and editor API calls) and added
  the remote desktop connection, room viewer, command runner, and annotation
  review views to the iOS app target.
- Routed the normal macOS Quit menu item through ordered application shutdown.
- Mesen2-OOS CPU registers now show real values; 0.7.2 looked for uppercase
  JSON keys the server does not send, so every register read as `0`.

### 🧱 Release Validation
- Added a dedicated Release-config native test build for Linux, macOS, and
  Windows that fails on zero tests, missing suites, wrong-configuration
  binaries, or empty JUnit results.
- Bounded reusable CI builds to four workers and separated build caches by
  configuration to reduce runner pressure and cache pollution.
- Replaced the two duplicate pull-request WASM builds with one Release-derived
  `wasm-smoke` build with a 45-minute limit and a serial Chromium Playwright
  smoke test. It now also runs for `inc/`, `cmake/`, `ext/`, and `assets/`
  changes and caches CPM at the path the build actually uses.
- Moved Linux DEB/TGZ payloads to an FHS layout and hardened package
  validation: the manifest version and Git SHA must match the release, shared
  libraries must resolve, a real APT install and purge must succeed, the macOS
  bundle signature is re-verified after relocation, Windows ZIPs must carry
  app-local MSVC runtimes, and the NSIS installer runs a silent install and
  uninstall.
- `scripts/install-nightly-local.sh` now validates a nightly before activation:
  staged directories, both executables present and running `--version`, macOS
  signing verified after resource copying, and an atomic `current` symlink swap
  only on success.

### 📚 Documentation
- Replaced stale parity percentages and Stable/Beta/WIP labels with one
  canonical desktop editor readiness matrix (Tester ready, Conditional, View
  only, Experimental) and a tester guide that distinguishes component test,
  direct ROM readback, app-path test, GUI smoke, and manual acceptance evidence.
- Generated the ALTTP quick reference from pinned usdasm and jpdasm sources and
  added a SNES hardware reference whose register addresses are checked against
  usdasm. `scripts/agents/alttp_reference.py check` fails when a generated
  section is stale or an address disagrees.

### Validation Snapshot
The figures below were added to these notes on 2026-09-13. They are not a run
against the final release head, which is **pending**.
- ROM parser/drawer parity and room fingerprint tests pass for the covered
  vanilla rooms and objects.
- `z3ed dungeon-object-validate` reported `0` mismatches across `1190` validated
  objects for the canonical vanilla ROM used by the dungeon parity audit.
- The Release test gate executed `3286` discovered stable tests locally on
  macOS instead of accepting an empty test run.
- Final tag SHA, packaged-candidate digests, hosted `Release` workflow run links,
  and a full-suite run against the release head are **pending**.

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
