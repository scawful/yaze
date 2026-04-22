# 0.x Release Ladder (Editor-First)

**Status:** ACTIVE  
**Owner (Agent ID):** docs-janitor  
**Created:** 2026-04-20  
**Last Reviewed:** 2026-04-20  
**Next Review:** 2026-05-02  
**Universe Task:** not yet assigned

## Summary

This plan defines the intended `0.x` release ladder after `v0.7.1`.

The primary release train is completion of the main ALTTP editors:
- Dungeon
- Overworld
- Graphics
- Screen
- Sprite
- Music
- Memory
- workspace/project lifecycle

Secondary trains remain important, but they should not displace the main
editor backlog:
- `z3ed` CLI expansion and hardening
- `z3dk` integration
- Oracle AI / emulator-debugging infrastructure
- Oracle of Secrets ROM-hack workflows
- iOS/macOS companion follow-through

This is a long-running `0.x` product line. `0.8.0`, `0.9.0`, `0.10.0`,
`0.11.0`, `0.12.0`, and later minors are normal milestone releases. None of
them should be treated as an implied ramp to `1.0`.

## Decisions And Constraints

- Use patch releases for stabilization, release follow-through, and narrow
  polish.
- Use minor releases for one coherent milestone or a tightly bundled set of
  related editor gaps.
- Keep the main ALTTP editors as the default prioritization anchor unless a
  secondary train directly unblocks editor completion.
- Prefer overview and navigation expansions that complement focused
  single-room editing instead of replacing it outright.
- Treat `z3dk` as a staged multi-release train, not as the sole headline that
  can consume an entire minor by default.
- Treat `z3ed`, Oracle debugging, and OoS support as continuous supporting
  workstreams that land in slices alongside the primary editor milestones.

## Release Ladder

### 0.7.2

**Primary goal:** stabilization and release follow-through

**Must-ship themes:**
- post-`0.7.1` build/docs/version alignment
- editor source-layout stabilization after the reorg
- current dungeon/workbench/object-tile/editor cleanup
- focused parity/polish already in the active panel lanes

**Secondary slices allowed:**
- `z3ed` coverage expansion
- Oracle debugger fixes that are already in-flight
- Mesen event/subscription cleanup
- low-risk cartographer performance work

**Do not let this become:**
- the z3dk release
- a broad multi-editor backlog bundle

### 0.8.0

**Primary goal:** Dungeon Editor completion milestone

**Must-ship themes:**
- object selector/browser preview parity
- verification of the remaining unknown dungeon object types
- remaining visible object/render discrepancies
- better room object type identification
- dungeon workbench/save-path follow-through so integrated workbench and
  panel flows stay reliable under real editing sessions
- responsive room navigation and toolbar layout that preserve center-canvas
  visibility in tighter windows and pane configurations
- pits/blocks persistence moved off legacy ROM-blob preservation and into
  first-class editable room-state encoders

**Secondary slices allowed:**
- narrow `z3ed` automation or validation improvements
- Oracle debugging work that materially helps dungeon parity work
- optional connected-room overview / grouped-room navigation work when it
  materially improves dungeon navigation and context without replacing
  focused single-room editing

**Do not let this become:**
- primarily a z3dk integration release
- a general platform/runtime refactor

### 0.9.0

**Primary goal:** Overworld Editor completion milestone

**Must-ship themes:**
- sprite workflow completion
- paste tracked correctly in undo/redo
- export dialog / export workflow completion
- persistent scratch pad
- dedicated eyedropper flow
- remaining Tile16 parity checklist items

**Secondary slices allowed:**
- `z3dk` M0-M1 class groundwork:
  - optional `z3dk-core` link
  - basic assemble path behind a flag
  - diagnostics surfacing
- `z3ed` CLI ergonomics that support overworld workflows

**Do not let this become:**
- a broad AI-platform milestone
- a mixed editor catch-all release

### 0.10.0

**Primary goal:** secondary editor parity release

**Must-ship themes:**
- Screen Editor cut/copy/paste/find
- Sprite Editor copy/paste
- Graphics Editor clipboard parity
- Palette JSON import/export
- deeper workflow coverage for editors that currently only have smoke-level
  GUI coverage

**Secondary slices allowed:**
- Oracle desktop workflow maturation:
  - live SRAM wiring into progression views
  - Annotation Overlay implementation
- `z3ed` doctor/editor automation command expansion

**Do not let this become:**
- the full Oracle platform release
- a large project/session architecture rewrite

### 0.11.0

**Primary goal:** Music + Memory completion release

**Must-ship themes:**
- Music event clipboard
- `SaveInstruments`
- `SaveSamples` / BRR pipeline completion
- Memory Editor search

**Secondary slices allowed:**
- `z3dk` M2-M3 class work:
  - symbol extraction
  - project symbol table population
  - unified Mesen2 client consolidation
- AI/debugging infrastructure improvements tied to real disassembly, symbols,
  and trace support

**Do not let this become:**
- a full IDE-in-yaze release
- a platform migration release

### 0.12.0

**Primary goal:** workspace/project lifecycle release

**Must-ship themes:**
- layout/workspace serialization
- `.yaze` / `.yazeproj` lifecycle tightening
- settings/layout persistence coverage
- migration/path normalization regression coverage
- emulator save-state workflow completion

**Secondary slices allowed:**
- `z3dk` M4-M5 class work:
  - hover / go-to-definition
  - `.mlb` export
  - lint-on-save
- Oracle of Secrets assembly/project workflow improvements:
  - symbol navigation
  - build error mapping
  - snapshot/diff ergonomics

**Do not let this become:**
- an SDL3 migration milestone
- a plugin architecture milestone

### 0.13.0+

These are later architectural trains once the main editor backlog is no longer
the dominant product risk:
- SDL3 migration
- plugin architecture
- enhanced memory tooling beyond search
- broader documentation overhaul
- larger AI/editor convergence work

## Secondary Train Placement

### z3ed

`z3ed` should ship continuously as support infrastructure:
- ROM doctor/validation growth
- editor automation command coverage
- test/CI-friendly provider and automation workflows

It should not normally become the only headline of a minor release.

### z3dk

`z3dk` should be planned as a staged train across multiple minors:
- `0.9.x`: optional embedding + diagnostics groundwork
- `0.11.x`: symbols + Mesen client consolidation
- `0.12.x`: hover/go-to-definition + `.mlb` export + lint-on-save

The existing scoped proposal remains the feature-level reference:
`docs/internal/plans/z3dk-integration-0.8.0.md`

### Oracle AI / Debugging Infrastructure

Land in narrow slices where they directly improve debugging and automation:
- real disassembly
- execution trace buffering
- symbol loading
- memory watchpoints/breakpoints
- bulk memory reads
- progression/annotation workflow wiring

The feature-level references remain:
- `docs/internal/plans/ai-infra-improvements.md`
- `docs/internal/plans/oracle-yaze-integration.md`

### Oracle of Secrets ROM-Hack Workflow

Treat OoS support as a long-lived product workflow, not a one-off side project:
- assembly editor symbol navigation
- build-error mapping
- snippets/macros
- snapshots/diff ergonomics
- RAM live views, annotation overlays, follow-mode tooling

These should land incrementally alongside the release ladder, especially once
the main ALTTP editor milestones are under control.

## Exit Criteria

This plan is doing its job if:
- release discussions default to the editor-completion ladder first
- `0.8.0` through `0.12.0` are scoped around the main editor backlog before
  platform/AI/tooling expansion
- secondary trains remain visible, scheduled, and bounded
- roadmap/status docs stop implying that `0.8.0` is primarily the z3dk release

## Validation

- Updated the canonical roadmap to point at this plan for `0.8.0+` sequencing.
- Updated status pointers so current release framing references this ladder.
- Kept feature-level plans for `z3dk`, Oracle integration, and AI infra as
  subordinate references rather than duplicating their detailed scopes here.
