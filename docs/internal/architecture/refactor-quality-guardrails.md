# Refactor Quality Guardrails

Status: ACTIVE  
Owner: `docs-janitor`  
Created: 2026-04-17  
Last Reviewed: 2026-09-22

Next Review: 2026-10-06

## Purpose

This document defines the minimum bar for human and agent editor refactors.
Each slice should remove a concrete obstacle to the next editing workflow.
The [capability plan](../plans/editor-capability-parity-plan.md) controls feature
scope; cleanup supports that work rather than becoming a parallel rewrite.

## Core rules

### 1. Editors trend toward coordinators

- Editor classes should own workflow state, domain systems, and registration of
  `WindowContent`.
- Editors should not accumulate new domain mutation logic once a service/system
  exists for that concern.
- Small orchestration UI is acceptable during migration. Large feature UI is
  not.

### 2. `WindowContent` is the UI composition unit

- Stop treating “panel” as the architectural unit.
- New UI work should use feature-oriented modules and `WindowContent`, not new
  `panels/` directories. Existing `ui/window/` units may retain their names;
  extracting an existing `*_panel.h` implementation into its matching `.cc` is
  allowed. A name change alone does not improve responsibility boundaries.
- Legacy `panels/` code may remain temporarily, but migration should move
  touched features toward `ui/<feature>/...` or another feature-oriented home.

### 3. Domain mutations live in services

- ROM patching, serialization, upgrade steps, and complex save policy must live
  outside editor UI code.
- Services should have narrow ownership. Do not move a god object into a class
  with a better name.
- Prefer constructor-injected required dependencies over nullable “initialize
  later” state.
- If a prospective service still depends on editor-only helpers, keep it in the
  editor layer first. Do not force editor helpers into `src/zelda3/...` just to
  satisfy naming.

### 4. Refactor where it helps the next feature

Do work in this order:

1. extract destructive or tightly-coupled mutation logic
2. isolate unstable or high-churn `WindowContent` surfaces
3. remove duplicated dependencies / stale ownership
4. rename and reorganize files once behavior is stable

Do not start with a broad naming sweep.

### 5. Preserve behavior before cleanup

- Extraction is not complete until command/shortcut behavior matches the old
  path.
- Treat keyboard shortcuts, mode transitions, save flows, and popup routing as
  compatibility surfaces, not incidental details.
- If a new coordinator changes semantics, keep it inert or local until parity is
  restored.
- Cleanup is allowed after parity; cleanup is not a substitute for parity.

### 6. Use callbacks for narrow interaction translation

- Input-to-command coordinators should depend on a small callback sink, not a
  concrete editor type or inheritance hierarchy.
- Callback sinks must stay command-oriented. Avoid “helper bags” that leak broad
  editor internals back into the coordinator.
- A coordinator may translate raw ImGui input, but it must not own domain state
  or perform ROM mutations.

## Logging rules

### 1. Log conditionally

- Do not build expensive log strings, symbol dumps, or trace payloads unless the
  category/level is enabled.
- Prefer `LogManager::ShouldLog(...)` when the log payload requires iteration,
  formatting, or snapshot building.
- Debug logging should help localize provenance, not become a permanent
  high-volume transcript.

### 2. Keep logs evidence-oriented

- Log source identity, counts, dimensions, IDs, and condition results.
- Avoid vague messages like “refresh failed” without the state needed to debug.
- Temporary instrumentation should be easy to remove or gate behind a dedicated
  debug section / category.

## Experiment flag rules

### 1. Use flags for unstable behavior, not permanent configuration

- `core::FeatureFlags` is for runtime/editor experiments and guarded behaviors.
- `ABSL_FLAG` is for process/runtime startup flags and developer tooling.
- Do not add a feature flag when a normal setting, persisted preference, or
  explicit user action is the better fit.

### 2. Every experiment flag needs an exit path

Each new flag should document:

- what it gates
- the safe default
- how it is validated
- what condition removes or graduates it

Avoid anonymous booleans that become permanent clutter.

## Human design and agent cleanup

The human designer owns the editing workflow: which choices appear, their names,
visual hierarchy, density, keyboard flow, and how selection feels on a real
canvas. Agents can prepare bounded implementation changes, automate evidence,
and review screenshots, but should record human interaction acceptance separately.

Start a refactor with a short brief in the existing task or plan:

1. Name one workflow and the concrete obstacle (for example, sprite layout code
   embedded in a header imported by several editor translation units).
2. Name the owned files and the responsibility that moves. Preserve widget IDs,
   callbacks, selection state, mutation hooks, and undo boundaries unless the
   task explicitly changes their behavior.
3. Set the proof before editing: body comparison for a mechanical move, build
   and affected tests, and interaction evidence for a behavioral change.
4. Extract, wire, and remove the old implementation in the same slice. Leave one
   obvious entrypoint for the next human change. Avoid new framework layers,
   pass-through service classes, and tests that only repeat the implementation.

For the sprite selector, layout now belongs in
`src/app/editor/dungeon/ui/window/sprite_editor_panel.cc`; the header exposes
its interface and state. This mechanical boundary makes the UI easier to find;
it does not establish better filtering, categories, or placement behavior.

## Agent review protocol

Agent-assisted changes should be reviewed for these failure modes first:

1. responsibility moves without real narrowing
2. “service” or “coordinator” classes that become replacement god objects
3. stale docs teaching old patterns after code changes land
4. broad naming churn mixed into behavioral refactors
5. logging added without gating or provenance value
6. feature flags added without lifecycle or validation

## Review checklist

Before approving a refactor slice, answer:

1. Is the extracted class narrower than the code it replaced?
2. Did the editor lose real responsibility, or just forward to a blob?
3. Are required dependencies explicit and non-null by construction?
4. Did it follow the existing UI composition boundary without adding a parallel
   panel hierarchy?
5. Are logs gated and useful?
6. Are any new flags justified, documented, and default-safe?
7. Did docs/examples get updated where the old pattern was taught?
8. Does the extracted path preserve existing shortcut/command behavior before
   further cleanup?

If the answer to any of these is “no”, revise before expanding the refactor.

## Static analysis workflow

Use the lightest checks that can invalidate the current slice:

1. Format the touched files with the pinned `clang-format` major from
   `.clang-format-version`. Run `scripts/lint.sh check --build-dir <build-dir>
   --require-tidy <changed.cc> ...` for explicit translation units. Required mode
   rejects missing tools, databases, exact source entries, and analysis errors.
   Format headers separately and analyze their owning `.cc` files; an inferred
   compiler command for a header is not proof of coverage. A default advisory
   run can skip analysis, and its output says so.
   Tidy warnings remain advisory unless `--warnings-as-errors '<check-glob>'`
   is supplied with required mode. For example, `clang-analyzer-*` gates analyzer
   findings without promoting every existing style warning. Do not use broad
   `fix` runs on legacy editor code; inspect each behavior-sensitive suggestion.
2. `scripts/quality_check.sh [--advisory|--gate]`
   This is a separate whole-repository pass (`clang-format`, `cppcheck`), not a
   wrapper around `lint.sh`. Use it when whole-tree scope is intended.
   It is advisory by default; `--gate` exits non-zero on
   clang-format violations and cppcheck error-severity findings.
3. `scripts/dev/editor-guardrails.sh <base-ref> <head-ref>`
   Run the implemented architectural heuristics listed below. Mutation ownership,
   flag lifecycle, and meaningful responsibility boundaries still need review.
4. targeted build/test commands
   Pair static checks with the narrowest runtime validation that exercises the
   changed surface, for example:
   `cmake --build --preset mac-ai-fast --target yaze --parallel 4`
   `ctest --preset mac-ai-unit --output-on-failure -R "(Overworld|DungeonWorkbenchToolbarTest)"`

Prefer targeted tests plus architectural guardrails over a blind full-suite run
when iterating on one migration slice.

### Compilation database and tool compatibility

`scripts/dev/update_compile_commands.sh <preset>` points the root database at a
configured preset for clangd/editor navigation. `lint.sh --build-dir` selects an
analysis database without changing that symlink. Report the tool version and
database used; formatting, compiler diagnostics, tidy, and UI acceptance are
different evidence.

An Apple Clang precompiled header cannot be assumed readable by Homebrew LLVM's
clang-tidy. Multi-config databases may also include unbuilt Release PCH paths.
Use a separate single-config build with PCH disabled when that occurs; configure
the same feature scope, generate required headers, then analyze the actual
translation units. On macOS, select the active SDK explicitly if the database
omits its sysroot and tidy cannot locate standard C++ headers. Do not erase parse
failures or call an unparsed file clean. Tool crashes and completed analysis with
findings also remain separate outcomes; record any reduced check scope.
See [scripts/README.md](../../../scripts/README.md#lint-hooks-and-quality-gates)
for the scoped commands.

At the September 22 audit, the top-level CMake tidy variable was set after editor
targets were created, and the advisory CI job sampled sources without a
configured database. Neither an “enabled” configure message nor that CI job
establishes editor coverage. A follow-on tooling slice should wire an opt-in
target scope and verify the actual compiler invocation before expanding CI.

## Automation

`scripts/dev/editor-guardrails.sh` is the lightweight enforcement layer for
these rules. It currently blocks:

- new files under editor `panels/`, or new `*_panel` editor files outside the
  script's allowed paths (including the existing `ui/window/` home)
- new concrete editor downcasts
- mega-file growth in already-bloated editor `.cc` files

The script does **not** verify mutation ownership, flag documentation, ImGui
stack balance, save safety, or interaction parity. Those are review and focused
verification responsibilities; do not report them as checked by this script.
