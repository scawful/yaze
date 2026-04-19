# Refactor Quality Guardrails

Status: ACTIVE  
Owner: `docs-janitor`  
Created: 2026-04-17  
Last Reviewed: 2026-04-17  
Next Review: 2026-05-01

## Purpose

This document defines the minimum bar for ongoing editor refactors, especially
incremental Gemini-assisted changes. The goal is not “perfect architecture in
one pass”; the goal is to keep each slice narrower, safer, and easier to review
than the code it replaces.

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
  `panels/` directories or `*_panel.*` files.
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

### 4. Refactor by leverage

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

## Gemini review protocol

Gemini-assisted changes should be reviewed for these failure modes first:

1. responsibility moves without real narrowing
2. “service” or “coordinator” classes that become replacement god objects
3. stale docs teaching old patterns after code changes land
4. broad naming churn mixed into behavioral refactors
5. logging added without gating or provenance value
6. feature flags added without lifecycle or validation

## Review checklist

Before approving a Gemini slice, answer:

1. Is the extracted class narrower than the code it replaced?
2. Did the editor lose real responsibility, or just forward to a blob?
3. Are required dependencies explicit and non-null by construction?
4. Did the change avoid new `panels/` or `*_panel` additions?
5. Are logs gated and useful?
6. Are any new flags justified, documented, and default-safe?
7. Did docs/examples get updated where the old pattern was taught?
8. Does the extracted path preserve existing shortcut/command behavior before
   further cleanup?

If the answer to any of these is “no”, revise before expanding the refactor.

## Static analysis workflow

Use the lightest checks that can invalidate the current slice:

1. `scripts/lint.sh check [files...]`
   Run formatting + `clang-tidy` on changed files when the compile database is
   available.
2. `scripts/quality_check.sh`
   Run a broader local quality pass (`clang-format`, `clang-tidy`, `cppcheck`)
   before large pushes or PRs.
3. `scripts/dev/editor-guardrails.sh <base-ref> <head-ref>`
   Run architectural heuristics for editor refactors. This catches new
   `panels/`, `*_panel`, concrete editor downcasts, suspicious editor-owned
   mutation logic, and undocumented flag growth.
4. targeted build/test commands
   Pair static checks with the narrowest runtime validation that exercises the
   changed surface, for example:
   `cmake --build --preset mac-ai-fast --target yaze --parallel 8`
   `ctest --preset mac-ai-unit --output-on-failure -R "(Overworld|DungeonWorkbenchToolbarTest)"`

Prefer targeted tests plus architectural guardrails over a blind full-suite run
when iterating on one migration slice.

## Automation

`scripts/dev/editor-guardrails.sh` is the lightweight enforcement layer for
these rules. It currently blocks:

- new `panels/` or `*_panel` editor files
- new concrete editor downcasts
- mega-file growth in already-bloated editor `.cc` files
- suspicious new editor-layer mutation/patching logic
- new feature/runtime flags without a corresponding doc update
