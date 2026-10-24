# Read-only Tile16 footprint proposals

`footprint_proposal.py` validates a separate version-1 semantic assembly envelope.
It consumes JSON and emits a deterministic report. It has no editor transaction,
map save, ROM reader/writer, collision inference or allocator. It does not change
the existing single-Tile16 terrain-label catalog schema.

Run the original synthetic example from the repository root:

```sh
python3 -B scripts/pixel_art/footprint_proposal.py \
  scripts/pixel_art/examples/footprint-target.json \
  scripts/pixel_art/examples/footprint.json --at 2,2
```

Use `--out NEW.json` for exclusive creation. Existing files and symlinks are
refused. The planner computes and validates the entire report before opening
the output. Inputs are never modified. Repeating `--at x,y` proposes multiple
placements of the same footprint, in Tile16 units.

## Binding and content identity

Both documents require `schema_version:1`, `units:"tile16"` and an identical
`binding` with exactly these fields:

| Field | Meaning |
|---|---|
| `game_id`, `adapter_id` | Exact engine/project and data adapter identities. |
| `rom_sha256` | Exact source ROM bytes observed by the caller. |
| `definition_fingerprint` | SHA-256 of the bound complete Tile16 definition snapshot. |
| `profile_fingerprint` | SHA-256 of the canonical render-profile JSON or a caller-documented lossless profile snapshot. |
| `tile_count` | Size of that definition snapshot; every used ID must fit. |

The caller must verify these observations against real input data. Equal strings
are a compatibility guard; this JSON-only tool cannot independently certify the
ROM or exporter. The original example uses synthetic identities, not real assets.

The footprint requires a stable `semantic_id`, explicit `review_state:"reviewed"`
and `content_id`. Compute `content_id` with the module's `content_id(document)`:
SHA-256 of sorted compact ASCII JSON excluding only `content_id`. Content identity
detects accidental edits; a self-declared review state is not an approval record.
Keep the actual reviewer, scope and source observation in provenance fields.
Suggested discovery groups must be reviewed and separately bound before use.

## Footprint and constraints

`rows` is a nonempty rectangular Tile16 matrix. An integer is an occupied cell;
`null` is skip. Explicit integer `0` writes Tile16 zero. It does not mean clear,
transparency, fill, walkability or a source-exclusion mask. At least one cell
must be occupied. `anchor:[x,y]` is an offset inside this rectangle. Each `--at`
places that anchor at the destination coordinate. Out-of-bounds occupied cells
are refused; skipped cells do not write or claim coverage.

`overlap_policy` is `reject` or `same_only`. The latter permits overlapping
occupied cells only when both footprints propose the same ID. There is no
last-writer-wins mode. The destination's `protected:[[x,y],...]` cells cannot
change. `clearance` and `ports` contain explicit constraints such as
`{"at":[2,0],"allowed_tile16":[1]}`. Coordinates are relative to the footprint
origin. The planner checks each constraint against the final combined proposal,
including every placement. Ports can carry names and provenance. These checks
enforce only listed tile predicates, not engine reachability or item progression.

## Tile32 diagnostics

Destination dimensions must be even. Changed cells report exact before/after
IDs. Each affected Tile32 block reports its before/after tuple in TL,TR,BL,BR
order. Odd Tile16 placement can affect four Tile32 blocks for a 2x2 footprint.
No new Tile32 IDs are assigned.

Optional `tile32_inventory` has `complete`, `entries` and, when complete,
`capacity`. Each entry has a unique `id` and four `tiles16` values. Duplicate
tuples at distinct IDs still consume distinct slots. A complete inventory must
include every source tuple in the destination. Caller-supplied complete inventory
must include all global/reserved occupied entries, with the actual usable
capacity. The planner cannot derive global ROM availability from one map.

Incomplete or absent inventory yields `capacity.status:"unknown"`; the number
of unseen tuples is a diagnostic, not approved allocation demand. A complete
inventory checks reused IDs, unique new tuples and available slots. Compressed
screen storage, pointer allocation, tile-definition editing and save/reopen
remain separate engine checks. Every report says `writes_enabled:false`.

## Existing editor connection

The output's occupied cells and exact changes are suitable for a later adapter
to existing `TileBrush`/`PasteBrush` and document history. This module implements
no adapter and creates no parallel undo model. Editor application requires its
own source review, atomic history and disposable save/reopen qualification.
Gameplay and custom collision require their separate native evidence.

```sh
taskpolicy -b nice -n 10 env PYTHONDONTWRITEBYTECODE=1 \
  python3 -m unittest discover -s scripts/pixel_art/tests \
  -p test_footprint_proposal.py -v
```
