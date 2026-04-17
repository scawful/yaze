# Welcome Screen Initiative (2026-04)

Status: LANDED (9 tasks complete, follow-ups tracked)
Last Updated: 2026-04-17
Scope: Recent-project UX, template-driven project creation, command-palette surface, startup performance

## Summary

This initiative reworked the welcome screen around five UX problems raised by
the user:

1. **Startup perf.** Expensive ROM hash/header I/O ran on the UI thread for
   every cache-miss recent on each launch.
2. **State management.** Welcome screen held recent-project logic directly
   against the `RecentFilesManager` singleton, with no annotations, no cache,
   no way for other surfaces (command palette, future CLI) to consume the
   same list.
3. **Newcomer onboarding.** Template section had generic copy; no first-run
   guidance for users who have never opened a ROM before.
4. **Managing recents.** No pin/rename/notes; removing a missing file
   silently destroyed its history; no undo after accidental removal.
5. **Templates.** Clicking a template chained straight into a file dialog
   without asking for a project name, destination, or even confirming intent.

All 9 tasks below landed in a single feature bundle.

## Landed Work

| # | Task | Artifacts |
|---|------|-----------|
| 1 | Extract `RecentProjectsModel` | `src/app/editor/ui/recent_projects_model.{h,cc}` |
| 2 | Metadata + CRC sidecar cache | same; `recent_files_cache.json` schema v1 |
| 3 | Async first scan of recent projects | `RecentProjectsModel::{DispatchBackgroundRomScan, DrainAsyncResults}` |
| 4 | Relink-on-missing instead of silent remove | `RelinkRecent`, warning-badged cards, "Locate…" context menu |
| 5 | Pin, rename, notes per entry | `SetPinned`, `SetDisplayName`, `SetNotes`, shared InputText popup |
| 6 | Undo toast after recent-project removal | `PendingUndo` buffer, `WelcomeScreen::DrawUndoRemovalBanner` |
| 7 | First-run layout + plain-English template copy | `DrawFirstRunGuide`, `use_when` / `what_changes` / skill-level fields |
| 8 | Guided new-project dialog | `src/app/editor/ui/new_project_dialog.{h,cc}` |
| 9 | Welcome screen command surface | `CommandPalette::RegisterWelcomeCommands`, `welcome.*` entries |

### Architecture at a Glance

```
┌────────────────────┐  Refresh()      ┌────────────────────────┐
│  WelcomeScreen     │────────────────▶│  RecentProjectsModel   │
│  (ImGui render)    │                 │                        │
│                    │                 │  entries()             │
│  DrawUndoBanner    │                 │  HasUndoableRemoval()  │
│  DrawAnnotation    │◀────read-────── │  cache_ (sidecar JSON) │
│  DrawFirstRunGuide │                 │  AsyncScanState        │
└────────────────────┘                 │   ↳ detached workers   │
         │                             └────────────────────────┘
         │ callbacks                              │
         ▼                                        │
┌────────────────────┐                            │
│  UICoordinator     │──── ReadFileCrc32 ─────────┘
│                    │      (background thread)
│  new_project_      │
│  dialog_           │     ┌────────────────────┐
│       │            │────▶│  CommandPalette    │
│       │ Open       │     │  welcome.* entries │
│       ▼            │     └────────────────────┘
│  NewProjectDialog  │
└────────────────────┘
         │ Create(template, rom, name)
         ▼
┌────────────────────┐
│  EditorManager     │
│   CreateNewProject │
│   OpenRomOrProject │
└────────────────────┘
```

### Key Design Decisions

- **`RecentProjectsModel` as a seam.** Welcome screen, command palette, and
  future CLI/automation consumers all read/mutate through the same model,
  keeping the `RecentFilesManager` singleton an implementation detail. The
  model owns a dual generation counter (`RecentFilesManager::generation_` +
  `annotation_generation_`) so `Refresh()` short-circuits on the common
  per-frame no-op path.
- **Sidecar cache keyed by (path, size_bytes, mtime_epoch_ns).** Cache hits
  skip SNES header parse + full-ROM CRC32. Forward-looking fields
  (`pinned`, `display_name_override`, `notes`) landed alongside the
  performance fields to stabilize the schema for later annotation features.
- **Async scan via `shared_ptr<AsyncScanState>`.** Workers hold their own
  copy, so tearing down the model mid-scan is safe: `cancelled` flips true
  and workers check it at I/O boundaries. Per-path `in_flight` set
  prevents refresh-per-frame from spawning duplicate threads.
- **Undo is a model-side concern, not a toast-manager integration.** The
  model keeps an 8s single-slot `PendingUndo` with the cached extras, and
  the welcome screen renders an inline banner. This keeps the feature
  testable without driving it through the toast system.
- **Relink instead of silent remove.** Missing files now surface as
  warning-badged cards with "Locate…" (triggers a file picker → `RelinkRecent`
  preserves annotations) and "Forget" (explicit `RemoveRecent`).
- **New-project dialog is a single modal, not a multi-step wizard.**
  Captures template + ROM + project name in one surface; ship-now design
  that sidesteps a multi-step state machine. The existing `CreateNewProject`
  chain handles project-file generation; the dialog adds the missing
  pre-flight prompts.
- **Command palette registration is one method, not scattered `AddCommand`
  calls.** `CommandPalette::RegisterWelcomeCommands` takes the model + a
  callback bundle. Per-entry commands rebuild from the same model that
  drives the cards, so pin/rename state flows through automatically on
  palette refresh.

## Validation

- `cmake --build build/presets/mac-ai --target yaze_editor -j8` — clean.
- `ctest --test-dir build/presets/mac-ai -L stable -j4` — 1688/1689; the
  single failure (`AnnotationHandlerTest.CreateMultipleAnnotationsAccumulates`)
  passes in isolation, is a pre-existing parallel-execution flake unrelated
  to this work.
- UI click-through not yet performed; see follow-ups below.

## Follow-ups

### P1 — Verify before next release

1. **Click-through the full flow.** Open welcome screen, remove a recent,
   hit Undo, verify cache restoration of annotations. Hit Pin and confirm
   the card moves to the front across Refresh cycles. Open the New Project
   dialog via a template card, validate the Browse → ROM path handoff, and
   verify the post-create toast + dashboard transition.
2. **Undo banner width.** `DrawUndoRemovalBanner` uses
   `GetContentRegionAvail()` for the rect fill; at narrow widths the button
   cluster can overlap the label. Check behaviour at ≤600px welcome width.
3. **Async scan stress.** Rapidly add/remove the same recent to shake out
   the race window where a worker's result is drained after `RemoveRecent`
   cleared the cache entry. Current design intentionally drops mismatched
   results via the `(size, mtime)` guard; confirm it holds under load.

### P2 — Next-sprint improvements

1. **Wire `pending_rom_selection_` through the wizard.** `ProjectManager::
   CreateFromTemplate` still has a `TODO: Implement template-based project
   creation`. The dialog now gathers the three inputs template creation
   actually needs — extend `EditorManager::CreateNewProject` to accept
   `rom_path` and `project_name` overrides so the dialog's inputs are
   load-bearing end-to-end rather than chained through `OpenRomOrProject`.
2. **Palette refresh on recents mutation.** `RegisterWelcomeCommands`
   registers per-entry commands at init time; mutations (add/remove/pin
   via the welcome screen) don't reflect in the palette until the next
   full `InitializeCommands` rebuild. Mirror the fix for
   `RegisterRecentFilesCommands` in a single unified refresh path.
3. **iOS FileDialog path for the New Project dialog.** Current wizard
   invokes the synchronous `FileDialogWrapper::ShowOpenFileDialog()`. On
   iOS this goes through the SwiftUI overlay; verify the Browse button
   behaves sensibly (or disable it on iOS and fall back to a drop zone).
4. **Command palette coverage tests.** Add unit tests asserting
   `RegisterWelcomeCommands` produces `Welcome: Remove Recent "x"`,
   `Welcome: Pin Recent "x"`, `Welcome: Undo Last Recent Removal`, and one
   entry per template. Fuzzy-score tests will catch regressions in the
   `"Welcome:"` prefix convention.

### P3 — Longer-horizon

1. **Thumbnail support.** `RecentProject::thumbnail_path` exists but is
   unused. A small on-save snapshot per project would make the cards much
   more scannable for users with many projects.
2. **Per-template starter ROMs.** `NewProjectDialog` could offer a curated
   list of known-clean source ROMs (matched by CRC32) instead of a free-form
   path — especially useful for "Vanilla ROM Hack" where users frequently
   point at patched ROMs by mistake.
3. **Multi-slot undo / recent-ops history.** Current design is single-slot
   with an 8s TTL. A ring buffer of the last 3 mutations with an "Undo
   recent operations…" palette command would make the welcome screen feel
   genuinely safe to experiment in.

## Files Touched

- `src/app/editor/ui/welcome_screen.{h,cc}`
- `src/app/editor/ui/recent_projects_model.{h,cc}` (new)
- `src/app/editor/ui/new_project_dialog.{h,cc}` (new)
- `src/app/editor/ui/ui_coordinator.{h,cc}`
- `src/app/editor/system/command_palette.{h,cc}`
- `src/app/editor/editor_library.cmake`

## Cache / Config Artifacts

- `${config_dir}/recent_files_cache.json` — sidecar schema v1, forward
  compatible. Safe to delete by hand (triggers a full rescan).
