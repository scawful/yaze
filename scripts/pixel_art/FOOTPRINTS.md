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

The planner remains read-only. The explicit Lyra bundle route below connects
its complete inputs to existing document/history and project-save paths. Sparse
footprints retain skip cells instead of becoming dense `TileBrush` data.
Gameplay and custom collision require their separate native evidence.

```sh
taskpolicy -b nice -n 10 env PYTHONDONTWRITEBYTECODE=1 \
  python3 -m unittest discover -s scripts/pixel_art/tests \
  -p test_footprint_proposal.py -v
```

### Explicit Lyra editor admission

A current Yaze build can admit a complete bundle into the existing Lyra document
and undo history. Package it without opening an editor or writing a ROM:

```sh
python3 -B scripts/pixel_art/footprint_proposal.py \
  /path/to/target.json /path/to/footprint.json --at 2,2 \
  --bundle-map 0xA0 --out /path/to/new-proposal-bundle.json
```

This keeps the original target, footprint and read-only report together. The
explicit map ID is an integer `$A0..$BF`. Select that map in Yaze, then choose
**File > Advanced Lyra > Apply Lyra Proposal...**. The editor checks the full
32x32 map, loaded ROM bytes (including any copier header), complete definitions
and native profile, and recomputes the report before changing cells. Map-specific
graphics must match the bound profile. Finish any active gesture and Apply or
Revert pending property drafts first. An admitted batch is one normal Undo step;
**Save Project** uses the existing atomic sidecar publisher.

Editor admission is bounded to 1 MiB JSON, 32x32 targets and footprints, at most
64 placements, and integer/string/boolean/null metadata. Duplicate keys, unknown
schema fields, floats, stale targets and forged reports are refused. Complete
Tile32 inventories are explicitly unsupported by this sidecar adapter; no IDs,
ROM capacity or runtime maps are allocated. Unknown Tile32 capacity remains an
unresolved ROM-compilation condition. Rejected/no-change proposals leave the
document and both history stacks unchanged. Suggested mining candidates cannot
be imported directly as reviewed footprints.

### Export an actual saved project target

`project_proposal.py` closes the gap between a saved `.yaze` project and the
explicit proposal bundle. It reads the project's ROM and schema-3 sidecar,
checks the ROM's expected CRC32/SHA1 (including legacy Yaze CRC32), checks the
expanded Tile16 definition bytes, and requires an exact saved native
`lyra-verify-project` observation. The observation must match the current ROM,
sidecar, shared profile and all 96 map/state asset bindings. A caller-provided
observation is evidence of that native run, not a signature or reviewer approval.

```sh
python3 -B scripts/pixel_art/project_proposal.py /path/to/project.yaze \
  --native-receipt /path/to/lyra-verify-project.json --map 0xB3 \
  --out /path/to/new-target.json

python3 -B scripts/pixel_art/project_proposal.py /path/to/project.yaze \
  --native-receipt /path/to/lyra-verify-project.json --map 0xB3 \
  --footprint /path/to/source-bound-reviewed-grove.json --at 21,16 \
  --out /path/to/new-grove.bundle.json
```

The native observation is obtained separately using the project's qualified
CLI: `z3ed lyra-verify-project --project-context=/path/to/project.yaze
--rom=/path/to/private.sfc --format=json`. Save its JSON stdout, then export.
The exporter starts no native process, writes only a new JSON file, and refuses
changed inputs or an existing output. A footprint must already have the exact
ROM/definition/profile binding. Its IDs are never silently rebound. Use
`--protect x,y` for explicit cells the proposal must preserve. Complete source
assemblies should carry substrate clearances and source coordinates in their
reviewed provenance; the exporter does not infer cliff direction or shoreline
grammar from tile IDs.

Current editor admission checks the document's shared graphics profile. This
exporter therefore refuses distinct map graphics and custom sprite profiles,
including the current jungle, glacier, village and brown shelf maps. These need
a native per-map binding adapter before bundle admission; removing the guard
would allow an apparently correct Tile16 number to select different pixels.
Import remains one normal Undo step through **File > Advanced Lyra > Apply Lyra
Proposal...**, followed by **Save Project**. Native import acceptance for a
particular exported assembly and gameplay remain separate checks.
