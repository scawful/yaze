# Working on the dungeon editor

Start with one visible editing improvement. You do not need to understand the
whole editor or clear its technical debt before making a useful change.

This guide is a code map and working procedure for maintainers and agents.
The [capability plan](../../internal/plans/editor-capability-parity-plan.md)
owns the roadmap, package status, and completion criteria. The
[current candidate handoff](../../internal/agents/dungeon-workbench-placement-handoff-2026-09-22.md)
owns commit-specific verification and remaining risks. Read their current
state before starting; a local candidate is not automatically the installed app.
Last reviewed: 2026-09-23.

## Your first coding session

**Recommended first change: make the dungeon status bar work at narrow widths.**
It takes presentation state and callbacks, so you can make design decisions
without changing room data or undo internals.

1. Open [DungeonStatusBar::Draw](../../../src/app/editor/dungeon/widgets/dungeon_status_bar.cc)
   and its [state/interface](../../../src/app/editor/dungeon/widgets/dungeon_status_bar.h).
   Trace where the current values come from before changing them.
2. Choose which information stays visible when space runs out. Keep Undo/Redo
   accessible. Decide where secondary information goes and how to reach it.
3. Implement one layout policy. Preserve the existing callbacks, theme colors,
   and widget identity. Avoid changing selection or keyboard shortcuts here.
4. Build the candidate. Inspect wide and narrow views, long selection labels,
   disabled Undo/Redo, and the UI scales you use. Record what still feels wrong.
5. Ask an agent to review clipping, callback ownership, and ImGui stack balance.
   Keep the design decision yours; accept or reject its implementation advice.

You can stop after a reviewable layout change. Reworking the Workbench docking
system is a separate task.

## Find the part you want to change

Read down one row of this map, then follow the actual call sites. Do not load
every file before editing.

| Responsibility | Entry point | What belongs there |
|---|---|---|
| Persistent workspace and inspector arrangement | [dungeon_workbench_content.cc](../../../src/app/editor/dungeon/workspace/dungeon_workbench_content.cc), [layout](../../../src/app/editor/dungeon/workspace/dungeon_workbench_layout.cc) | Composition, local tools, and which inspector is visible. |
| Door, sprite, and pot-item property controls | [dungeon_entity_inspector.cc](../../../src/app/editor/dungeon/inspectors/dungeon_entity_inspector.cc), `DrawDungeonEntityInspector` | Shared controls for Workbench and standalone rooms; temporary UI drafts. |
| Sprite placement chooser | [sprite_editor_panel.cc](../../../src/app/editor/dungeon/ui/window/sprite_editor_panel.cc), `DrawSpriteSelector` | Choosing the next placement type. This differs from editing a selected sprite. |
| Selection and sprite mutation | [interaction_context.h](../../../src/app/editor/dungeon/interaction/interaction_context.h), [sprite_interaction_handler.h](../../../src/app/editor/dungeon/interaction/sprite_interaction_handler.h), `UpdateSprite` | Authoritative selection and validated edits. Do not mirror selection inside a widget. |
| Compound edit validation | [dungeon_selection_edit.h](../../../src/app/editor/dungeon/dungeon_selection_edit.h), [dungeon_connection_edit.h](../../../src/app/editor/dungeon/dungeon_connection_edit.h) | Draft before/after values and reject invalid edits before publication. |
| Room clone/import | [dungeon_room_transfer.h](../../../src/app/editor/dungeon/dungeon_room_transfer.h), [shared controls](../../../src/app/editor/dungeon/inspectors/dungeon_room_transfer_editor.cc), [editor integration](../../../src/app/editor/dungeon/dungeon_editor_v2_room_transfer.cc) | Versioned authored data, explicit preview, detached save preflight, and one undo action. Keep resource packaging separate from numeric room references. |
| Editor history and publication | [dungeon_editor_v2_undo.cc](../../../src/app/editor/dungeon/dungeon_editor_v2_undo.cc), [dungeon_editor_v2_selection_edits.cc](../../../src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc) | Capture/finalize gestures, publish accepted data, restore selection, and refresh views. |
| Serialization and room-stream bounds | [room.h](../../../src/zelda3/dungeon/room.h), [dungeon_stream_allocator.h](../../../src/zelda3/dungeon/dungeon_stream_allocator.h) | Persisted representation and allocation. Read this layer when the task changes data, rather than layout alone. |

The useful path for a sprite chooser is:
`DrawSprite` → `SpriteInteractionHandler::UpdateSprite` → mutation hooks →
existing undo history → view refresh. A control should call that path rather
than assign directly into `Room::GetSprites()`.

## Choose a task at the right depth

These are proposed human-owned exercises, not additional release requirements.
Agents should not implement them preemptively during unrelated cleanup.

| Depth | Bounded task | Acceptance and agent support |
|---|---|---|
| 1: presentation | Responsive status bar, above. | Information has an intentional priority at narrow widths; callbacks and disabled states remain correct. Agent reviews layout code and builds. |
| 2: interaction | Add a searchable named sprite-type chooser to `DrawSprite` in the shared inspector. Reuse `zelda3::ResolveSpriteName` and `UpdateSprite`. | Search by name/ID, keyboard choice, cancel, no-results, and unknown current values work. Hover/filter creates no edit; accepting creates one undoable edit in both viewer modes. Agent checks the mutation path and extends existing UI/lifecycle tests for changed behavior. |
| 3: composition | Improve the sprite placement chooser's empty/search/current-choice presentation in `DrawSpriteSelector`. | The user can distinguish choosing a placement type from selecting an existing sprite. Escape, room changes, focus, and wheel ownership behave deliberately. Preserve the authoritative placement handler. Agent prepares only a necessary local extraction. |
| 4: code structure | Extract one drawing responsibility that blocks your chosen feature. | Name its callers, move and wire the implementation, remove the old path, preserve IDs/callbacks, and prove the same behavior before changing it. Agent can perform the mechanical move; you review whether the new entry point is easier to work in. |

For the named chooser, start with
[shared inspector UI tests](../../../test/unit/editor/dungeon_workbench_content_test.cc),
[handler tests](../../../test/unit/editor/sprite_door_handler_test.cc), and
[editor undo lifecycle tests](../../../test/unit/editor/dungeon_undo_actions_test.cc).
These test different boundaries. A handler test cannot detect a broken ImGui
layout or prove that a click reaches the handler.

Follow the [UI guidelines](../../internal/architecture/ui-design-guidelines.md)
for names, theme values, focus, preview/commit separation, and units. Screenshots
help judge layout; live interaction is needed to judge keyboard, wheel, and drag.

## Build and check one change

Run from the repository root. These are the current macOS candidate commands;
they do not require a fixed checkout path or a configured AI provider.

```sh
git status --short --branch
git rev-parse HEAD
cmake --preset mac-ai
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4
```

For the named sprite chooser, this is a starting focused filter. Discover it
before running and confirm that the intended suites appear:

```sh
yaze_chooser_filter='*DungeonWorkbenchEntityInspectorUiTest*:*SpriteInteractionHandlerTest*:*DungeonEntityUndoLifecycleTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_chooser_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_chooser_filter" --gtest_output=xml:/tmp/yaze-chooser-tests.xml
```

Parameterised suites have a prefix; the leading `*` matters. Record selected,
passed, failed, and skipped counts. An empty filter result is not verification.
Adjust the scope to the changed feature rather than repeatedly running every
historical suite. Add regression tests for new behavior or actual failures;
cosmetic text changes do not need tests that simply repeat the new text.

The app output is `build/presets/mac-ai/bin/Debug/yaze.app`. Verify the binary
you launch before visual review. A Barista shortcut may launch another build.
The build command does not install or replace an existing app. Do not overwrite
personal ROMs or saves for testing; any write/reopen check uses an explicitly
chosen disposable copy and records its identity.

On Linux or Windows, inspect `cmake --list-presets` and use the matching
toolchain. Do not reuse a macOS build directory or compilation database.
Cloud/ROM-less sessions follow [cloud agents](../../internal/agents/cloud-agents.md)
and `scripts/cloud/bootstrap.sh configure build test`. Missing ROM/runtime
evidence remains a named local follow-up, not an inferred pass.

## Keep cleanup small and useful

The [refactor guardrails](../../internal/architecture/refactor-quality-guardrails.md)
are the policy. Use a behavior-preserving extraction when a feature is hard to
reach, then commit the behavior change separately. Preserve widget IDs, popup
ownership, callbacks, selection, undo boundaries, and old-room gesture finalization.

Use three distinct checks:

1. **Format and compile.** Use the formatter version in `.clang-format-version`.
   Format touched lines/files; inspect the diff. A whole-file rewrite of legacy
   layout code obscures the actual change.
2. **Analyze the affected translation units.** The
   [lint instructions](../../../scripts/README.md#lint-hooks-and-quality-gates)
   cover required tidy, compiler databases, PCH compatibility, and known tool
   failures. An absent database, parse failure, or crashed check is not clean
   analysis. Analyze the owning `.cc` when a header changes.
3. **Run the relevant behavior checks.** Use focused tests and live interaction
   for the changed workflow. The guardrail script checks architectural
   heuristics; it does not prove save safety or ImGui stack balance.

For the configured PCH-free macOS analysis directory, the existing reduced
analyzer scope can be run explicitly:

```sh
cmake --preset mac-ai -B build/analysis/mac-ai -G Ninja \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DYAZE_ENABLE_CLANG_TIDY=OFF \
  -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' \
  --warnings-as-errors='clang-analyzer-*' \
  src/app/editor/dungeon/inspectors/dungeon_entity_inspector.cc
```

Select the intended LLVM binary on `PATH` and record its version. This is an
analyzer-only command, not the full configured tidy suite. Generate any missing
build headers before analysis; keep errors visible. Do not change `.clang-tidy`
or suppress a check during a UI task just to obtain a passing result.

Run `scripts/dev/editor-guardrails.sh <base-ref> <head-ref>` with the exact
review range. The script needs Bash 4+ (`mapfile`); macOS's system Bash 3.2 is
insufficient. Use an installed compatible Bash without hardcoding that path in
shared scripts. Uncommitted changes require a staged tree or another explicit
review target; comparing `HEAD` to itself verifies nothing.

## Hand an agent one useful job

Use one implementation owner per overlapping file. Parallel agents are useful
for independent format audits, focused review, or documentation. They should
not all rediscover the project, run competing builds, or rewrite the same UI.
One owner coordinates the build and records the final candidate.

Copy this brief into the existing task and fill in its concrete values:

```text
Package and outcome: [DA-* ID; one visible behavior and its exit criteria]
Candidate: [repository/worktree, branch, exact base commit, dirty owned files]
Read first: AGENTS.md, the package in the capability plan, relevant handoff
Own: [specific files and responsibility]; preserve: [specific contracts]
Boundary: [what this slice intentionally leaves for a later named package]
Verification: [one build owner, discovered test filter, evidence still needed]
Return: changed files, exact commands/counts, unresolved risks, one next action
```

For mechanical cleanup, specify that behavior must stay unchanged. For a feature,
state the intended behavior change. For review, ask for concrete defects and
missing evidence rather than a general rewrite. Reuse the existing handoff and
canonical plan; do not create another roadmap or status file for each agent.

The [package table](../../internal/plans/editor-capability-parity-plan.md#work-packages-and-dependencies)
sets priorities. Room clone/import belongs to DA-4; connection families still
open belong to DA-3; packaged/save/reopen/runtime qualification belongs to DA-5.
Keep these separate when assigning work. Passing synthetic tests on a local
candidate cannot close the latter evidence gaps. The
[release ladder](../../internal/plans/release-ladder-0x-2026.md) controls when
overworld, graphics, screen, and audio packages follow dungeon authoring.
