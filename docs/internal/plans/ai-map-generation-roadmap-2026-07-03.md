# AI-assisted map generation roadmap — 2026-07-03

**Status:** Aspirational / future train
**Trigger:** GitHub Discussion #57, `(auto)Map generation`
**Public stance:** not ready yet; current YAZE maps are still primarily authored
by hand.

## Short answer

YAZE does not currently generate a complete logical, well-organized overworld or
dungeon map from a prompt. The goal is to eventually support AI-assisted and
procedural map authoring, but it should land after the manual editors,
validators, save paths, and project compatibility layer are reliable enough to
make generated output safe to inspect, edit, and save.

The product goal is **assisted authoring**, not a black-box one-shot generator:
YAZE should propose layouts, fill or repair regions, explain constraints, and
run ROM/editor validators before the user accepts writes.

## Why it is not first

Generated maps are only useful if the editor can prove they are valid. For
ALTTP and Oracle-style hacks that means:

- Tile16 and Map32 data must round-trip and fit compression/write budgets.
- Area seams must line up across neighboring maps.
- Entrances, exits, holes, warps, sprites, items, flute points, and special
  triggers must remain internally consistent.
- Generated edits must avoid project-declared custom ASM/data ranges.
- The result must be explainable enough that a human hacker can revise it.

Until the Overworld Editor, Dungeon Editor, and ROM doctor/validation surfaces
are stronger, generation would mostly create hard-to-debug corruption risk.

## Staged path

### Stage 0 — manual editor reliability

Land first through the editor-first ladder:

- `0.8.0`: Dungeon Editor completion and persistence correctness.
- `0.9.0`: Overworld Editor completion, including sprite workflow, paste undo,
  export, eyedropper/sampling, and Tile16 parity cleanup.
- `0.10.0`–`0.12.0`: secondary editor parity, Music/Memory, and
  workspace/project lifecycle.

### Stage 1 — validators and analyzers

Build the safety layer a generator can call:

- Overworld graph export with neighboring-area seams, entrances/exits, holes,
  item/sprite lists, and palette/blockset metadata.
- Map health checks for unreachable areas, broken transitions, invalid special
  tiles, compression overflow, and write-range conflicts.
- Project metadata for reserved custom code/data ranges.
- CLI-friendly reports so AI agents can reason over map state without the GUI.

### Stage 2 — constrained assist tools

Start with small, reversible operations:

- Suggest/fill a region from a palette/blockset-constrained Tile16 set.
- Repair seams between two adjacent maps.
- Detect and suggest fixes for cliffs, water edges, paths, and border tiles.
- Generate decoration passes over a selected area without moving entrances or
  gameplay entities.
- Explain why a generated edit is rejected by validators.

### Stage 3 — template/procedural layout generation

Generate draft layouts from explicit constraints:

- biome/theme, map size, required entrances, required traversal gates
- vanilla/Hyrule Magic/ZSCustomOverworld compatibility target
- allowed Tile16 sets and reserved map IDs
- optional progression graph input for item/entrance logic

Outputs should be staged as editable diffs, not written directly to the ROM.

### Stage 4 — hack-scale expansion support

Before full-world generation is realistic, YAZE needs stronger support for
expanded content:

- half-overworld / additional-overworld planning
- address-space expansion and repointing strategy
- save compatibility and migration rules
- clear warnings when a hack uses custom ASM/data in editor write ranges

### Stage 5 — full AI-assisted world authoring

Once the above exists, an AI workflow can propose a whole connected overworld or
dungeon region, then iterate through:

1. generate draft
2. validate
3. explain failures
4. repair
5. present staged diff
6. let the user hand-edit
7. run final ROM/emulator smoke checks

## Useful public wording

> Not yet — maps are still mostly hand-authored today. The long-term goal is
> AI-assisted map creation, but we need reliable manual editing, validation,
> and ROM safety first. I want YAZE to eventually generate or suggest regions
> from constraints, repair seams, validate entrances/exits/items/sprites, and
> stage the result for human review instead of blindly writing a whole map.
> The near-term focus is finishing the Dungeon and Overworld editors so that
> generated maps have a safe editor/validator pipeline to land in.

## Validation ideas

- Golden fixture: generated diff round-trips through load/save without changing
  unrelated ROM ranges.
- Graph check: every required entrance/exit/hole resolves to a valid target.
- Seam check: adjacent maps agree along all borders.
- Compression check: generated map data fits or triggers a repointing plan.
- Project safety check: generated writes do not overlap reserved custom ranges.
- Emulator smoke: load a start state and walk through generated transitions.
