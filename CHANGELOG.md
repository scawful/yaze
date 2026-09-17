# Changelog

High-level release summary. For detailed notes, see
`docs/public/reference/changelog.md`.

## 0.8.0 (in development)
- Dungeon Editor completion milestone. See
  `docs/internal/plans/release-ladder-0x-2026.md` for scope. Release date,
  final merge SHA, and packaged-candidate evidence are pending.
- **Upgrading from 0.7.2**: several saves, z3ed commands, Hack Manifest
  sections, project files, menus, and package layouts behave differently. See
  the **Upgrading from 0.7.2** section in `docs/public/reference/changelog.md`
  before upgrading a project or script.
- **Dungeon Rendering Parity**:
  - Corrected draw routines against USDASM for thin floor and wall strips
    (the `_plus3`, `_plus12`, and `_plus13` routines now draw from the object's
    own origin), wall corners, diagonal walls and ceilings, stair priority,
    rails, moving walls and floors, floor-copy objects, and explicit door
    bodies.
  - Matched individual object families against the disassembly, including
    Somaria paths, pushable blocks, torch codecs, hammer pegs, light beams,
    archery curtains, bombable and rupee floors, big key locks, prison cells,
    the Fortune Teller room, Smithy Furnace, Agahnim's altar, and exact 4-word
    bar-corner payloads.
  - Removed the hard-coded `IsAllBgsObjectId` list. BothBG routing now comes
    from draw-routine registry metadata plus explicit routing for stairs and
    fixed facades, and ObjectDrawer's duplicate dimension switch was replaced
    by `DimensionService`.
  - Fixed dungeon graphics Left/Right palette slot mapping, live palette
    refresh, and placement-ghost palettes. Object selector previews now use the
    room's palettes, and room sprite previews use the room's palette-set
    selectors instead of a fixed sprite palette table.
- **Dungeon Editor Workflow**:
  - Kept the room canvas vertically stable by removing the selection action
    shelf and recent-room tab strip above it, replaced the Tools inspector's
    icon strip with a grouped tool chooser, and added **Pop out** (or **Show
    window**) so each tool is drawn in only one place at a time.
  - Toolbar actions that do not fit at narrow widths now move into an overflow
    menu (including full Compare controls) instead of disappearing, recent
    rooms moved into a **Recent Rooms** popup, the Object Selector gained a
    stream filter and **More** menu, and the dungeon palette grid adapts to
    16/8/4/2/1 columns.
  - Made issue capture local and opt-in: reports are no longer auto-saved on
    open, copy, screenshot, or close, and only **Save to Issue Log** writes
    one. The report dialog layout is stable, and object traces replay the
    earlier room-stream objects with layout context.
  - Gave the room canvas, room selector and matrix thumbnails, and Dungeon Map
    previews their own composite textures, so they can no longer overwrite
    one another's room image or the palette the issue report samples.
  - Object Selector cards show a draw-routine badge with the routine family
    and base pattern size in the tooltip.
- **Oracle and Custom Dungeon Assets**:
  - Replaced the modal Custom Object Workshop with a non-modal **Custom
    Assets** mode in the object selector covering the 21 fixed Oracle runtime
    slots (`0x31`, `0x32`, and new `0x54` sprite bodies), with **Edit Tile
    Layout** (desktop only), **Place in Room**, and a **Minecart Routes &
    Collision** shortcut on track slots.
  - Removed the track-corner alias that drew wall corners `0x100`-`0x103`
    with object `0x31` corner assets whenever a project mapped `0x31`; those
    IDs now stay wall corners unless the project maps the same ID directly.
  - Published custom asset and minecart-track sources with path confinement,
    atomic replacement, rollback, and decoded readback; WASM fails closed.
  - **Custom Assets > Reload Assets** (formerly Reload Workshop) now also
    refreshes external Oracle sprite preview art in every room, Workbench, and
    comparison viewer, and stays available when Custom Objects is disabled.
- **Fail-Closed Save Safety**:
  - Checked dungeon palette, Palette Editor, and Object Tile Editor ROM writes
    against the Hack Manifest when a project manifest is loaded, and extended
    `z3ed dungeon-set-room-property --manifest` checks to room headers and
    message-ID bytes.
  - Made z3ed dungeon write commands (`dungeon-place-object`,
    `dungeon-set-room-property`, `dungeon-generate-track-collision`, and the
    custom-collision and water-fill JSON imports) roll back on failure, and
    blocked ROM saves while Screen Editor or graphics sheet edits are pending.
    Dungeon Save and Apply Room are also blocked while minecart drafts are
    unpublished or Object Tile Editor changes are unapplied.
  - Made ROM backup restore stage the backup as unsaved until Save ROM (with a
    **Discard Restored Backup** option), made `Rom::SaveSettings::backup` copy
    the existing destination file, and marked sessions Modified when only project
    settings or project drafts are unsaved.
- **z3ed CLI**:
  - Added `dungeon-get-palette`, which resolves a US/OOS room to its shared
    dungeon palette and lists every room using it, and
    `dungeon-set-palette-color`, a dry-run-by-default color edit with
    expected-value checks, manifest ownership, a required backup, atomic save,
    and reopen readback. `palette-set-color --write` now refuses to write;
    previews still work.
  - Added `dungeon-set-door-type`, `dungeon-set-pot-item`, and
    `dungeon-list-pot-items`; added `--include-objects` to
    `dungeon-describe-room` and spawn-point schema output to `--spawn`; and
    made `dungeon-export-custom-collision-json` and
    `dungeon-export-water-fill-json` refuse `--out`/`--report` paths that
    resolve to the ROM.
  - Added `dungeon-remove-object` with exact index, ID, position, size, and
    layer guards, and made `project-bundle-verify --check-rom-hash` accept the
    iOS `romChecksum` manifest field. CRLF `project.yaze` files now load their
    settings.
- **Appearance and Editor Shell**:
  - Added five editor themes (Blood Moon, Catppuccin Mocha, Dracula, Rosé
    Pine, Temple of Time), with upstream MIT notices for Catppuccin, Dracula,
    and Rosé Pine kept in `assets/themes/THIRD_PARTY_NOTICES.md`; release
    bundles include only themes listed in
    `assets/themes/distributable-themes.txt`.
  - Theme files now get smart defaults for borders, scrollbars, table colors,
    links, and modal backgrounds they leave out (0.7.2 left them opaque black),
    while colors a `.theme` file declares are never replaced.
  - Reworked the welcome screen into a compact start card that never scrolls,
    replaced the What's New release-history card with a Release notes link,
    and made recent-file cards openable from the keyboard.
  - Repaired the editor chooser dashboard: `--startup_dashboard`, light-theme
    card contrast, Display Density, the advertised shortcut, duplicate
    rendering, and recent-editor parsing.
  - Renamed the Search menu's Window Finder to **Find Window…** (Ctrl+P), made
    Help → **Keyboard Shortcuts** open the shortcuts browser (Ctrl+Shift+/)
    instead of Settings, and removed the Dungeon Workbench's conflicting
    Ctrl+Shift+W hint.
  - Added an always-visible drawer icon strip and header context badges to the
    right sidebar drawer.
- **Emulator, iOS, and Platform**:
  - Added TCP endpoint support to the Mesen socket client alongside Unix
    sockets.
  - Fixed the iOS device build and added the remote desktop connection, room
    viewer, command runner, and annotation review views to the iOS app
    target.
  - Routed the macOS Quit menu item through ordered application shutdown.
  - Read Mesen2-OOS CPU registers from the server's lowercase JSON keys; 0.7.2
    read every register as `0`.
- **Release Engineering**:
  - Hardened the shared native test step used by CI and release builds: it
    runs against the configured build type (Release for release builds) and
    fails on zero discovered tests, missing or unbuilt test suites, or missing
    or empty JUnit output.
  - Consolidated pull-request WASM validation into one bounded build and
    browser smoke gate.
  - Moved Linux DEB/TGZ packages to an FHS layout and strengthened release
    validation: manifest version and Git SHA must match the release, Linux
    shared libraries resolve and the DEB installs and purges cleanly, the
    macOS app still verifies after relocation with matching bundled assets,
    Windows packaging requires app-local MSVC runtimes, and the NSIS installer
    passes a silent install/uninstall check.
  - `scripts/install-nightly-local.sh` now validates a nightly before
    switching `current` to it, and default build parallelism is bounded to four
    workers.
- **Documentation**:
  - Replaced stale feature percentages with an evidence-based tester-readiness
    matrix and a cross-platform artifact acceptance contract.
  - Generated the ALTTP quick reference from pinned usdasm and jpdasm sources,
    added a SNES hardware reference whose register addresses are checked
    against usdasm, and added a check that fails on stale or mismatched
    tables.

## 0.7.2 (July 17, 2026)
- **Dungeon RC Stabilization**:
  - Persisted the latest dungeon save-domain follow-through for entrances and special objects.
  - Rendered dungeon object/sprite selector previews by default and kept workbench-local tools open for mixed Workbench/Window workflows.
  - Hardened ALTTP door placement against invalid non-table positions and pinned the USDASM 12-entry door-table contract in tests.
  - Canonicalized dungeon room-object labels through `room_object.h` and exported those labels through `Zelda3Labels` resource maps.
  - Preserved zero-tile `DrawNothing` logic objects as explicit no-payload cases instead of forcing the conservative 8-tile fallback.
  - Added editable pit-damage room membership controls and hardened the
    pushable-block loader against invalid pointer operands and capacity
    overrun boundaries.
  - Added ROM-backed regression coverage for pit-damage persistence,
    pushable-block capacity, and BG1/BG2 object-overlap ordering.
- **Overworld Follow-through**:
  - Added a canvas context-menu Tile16 sampling action for the right-click eyedropper workflow.
- **Save Safety and Project Reliability**:
  - Made coordinated editor saves whole-ROM transactional so validation,
    backup, or disk-write failures restore the ROM and leave edits retryable.
  - Preserved Save As targets across confirmation prompts and made unsupported
    dungeon stream growth fail closed instead of risking partial writes.
  - Hardened project paths, manifests, hashes, expanded messages, palettes, and
    custom-overworld save boundaries with focused regression coverage.
- **Localization and Platform Reliability**:
  - Added the internationalization catalog pipeline and French localization.
  - Hardened file-dialog initialization plus session, asset, configuration,
    and CLI failure paths across supported desktop platforms.
- **Build/CI and Release Follow-through**:
  - Kept the post-0.7.1 preset build layout and release metadata aligned for
    the 0.7.2 release.
  - Continued WASM/browser, CLI wrapper, sanitizer, fork-PR, and Windows test
    hardening from the post-tag cleanup train.

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
