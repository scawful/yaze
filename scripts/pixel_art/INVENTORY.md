# Source-backed dungeon inventory

Generate the complete registered object/routine metadata from the checked-out
Yaze source, then validate its join to the dungeon family profile. This helper
uses Python 3.10+ and the standard library. It does not load a ROM, render rooms,
copy reference artwork or require a C++ build.

Run from the repository root:

```sh
# Validate the 19 pinned metadata sources and extract all identifiers.
python3 -B scripts/pixel_art/dungeon_inventory.py --check

# Also check family joins, palette/module IDs and the profile's Yaze sources.
python3 -B scripts/pixel_art/dungeon_inventory.py --check \
  --family-bindings scripts/pixel_art/contracts/dungeon_object_families.json

# Write full metadata into a NEW JSON artifact outside the source tree.
python3 -B scripts/pixel_art/dungeon_inventory.py \
  --family-bindings scripts/pixel_art/contracts/dungeon_object_families.json \
  --output /tmp/yaze-dungeon-inventory.json

# Focused positive and failure controls, including changed-source rejection.
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s scripts/pixel_art/tests -p 'test_dungeon_inventory.py' -v
```

Without `--output`, full inventory goes to stdout; `--check` prints only a
summary. Source roots default to this script's repository root. Use
`--source-root /path/to/yaze` for another checkout. Existing files and symlinks
are refused, and the output must be a `.json` artifact outside that source tree.
There is no overwrite option. The parent output directory must already exist.
Validation completes before exclusive output creation. An I/O failure can leave
a partial new artifact; retain it for inspection and choose a fresh output path.

The default works from a normal Yaze checkout with no external disassembly.
`--usdasm-root /path/to/usdasm` explicitly opts into reading the pinned vanilla
`bank_01.asm` source. The optional source fingerprint is in
[the source contract](contracts/dungeon_inventory.sources.json). A mismatch is a
failure. With family bindings, the default checks **40 Yaze source references**
and reports `S38`/`S39` as **unbound optional USDASM references**. The explicit
external root checks those two references too. An unbound reference is never
reported as verified. No private path, ROM hash or reference image is required.

## Exact populations and meaning

| Population | Count | Contract |
|---|---:|---|
| Serializable objects | 440 | Subtype1 `0x000..0x0F7` (248), subtype2 `0x100..0x13F` (64), subtype3 `0xF80..0xFFF` (128). |
| Registry metadata entries | 448 | Includes eight `0x0F8..0x0FF` aliases rejected by the strict save validator. |
| Registered routines | 135 | IDs `0..134`; every mapped object points to a registered routine. |
| Default custom families | 3 | `0x31`, `0x32`, `0x54`; 16 + 3 + 2 default asset slots. |
| Family vocabulary | 24 | All 135 routines have a source classification or an explicit classification gap. |

`routine_id_default` is a Yaze registry index, not a CPU address. The optional
vanilla source cross-reference is separate from any target Oracle ROM tables
or hooks. `parser_payload_word_count` counts source tilemap words; it is neither
the output footprint nor a count of unique 8x8 characters. Registered base
width/height describes a base primitive, not every size-dependent footprint.
Serialization still has position, size and reserved-prefix guards beyond ID
membership. No serializer or ROM writer is called here.

The generated records retain custom-object overrides; fixed/mixed-layer
routing; full-layer erase; stateful chest/lock counters; boss-shell suppression;
and room-floor source substitution. Custom project files can override more
objects before registry lookup. Their actual contents are not loaded. Reviewed
dispatcher annotations are bound to source hashes and line ranges in the
contract; the generator does not infer their semantics from names.

Names ending in `BothBG` are not sufficient layer evidence. In this source,
Yaze BG1 is `$7E2000` / hardware BG2; Yaze BG2 is `$7E4000` / hardware BG1.
List index, object order, explicit metadata, special routing, palette/priority
and H/V flags remain separate. See the
[dungeon coverage contract](../../docs/reference/dungeon-tileset-coverage.md)
and [family profile](profiles/oracle-sky-dungeon-family.json).

## Failure behavior and source updates

The source contract records repository-relative paths, SHA-256 fingerprints,
expected ID sets/counts and the reviewed baseline `source_commit`. The helper
captures all 19 source files and checks **every fingerprint before parsing any
of that captured C++ text**. It then parses those captured bytes, avoiding a
second source read during extraction. `SOURCE_FINGERPRINT_MISMATCH` lists the
changed or missing sources and prevents output creation. Source comments count
as drift too. There is no automatic fingerprint refresh.

The extractor accepts a finite grammar: literal map assignments, bounded
inclusive range/list loops, complete designated routine registrations, literal
name/filename/count arrays, and ordered literal-return count branches. It
rejects unconsumed control flow, extra registry builders, duplicate IDs,
unknown registration fields and unsupported expressions even when a test
contract deliberately supplies a new fingerprint. It interprets only a bounded
boolean/comparison AST for count conditions, without `eval`. Routine function
bodies remain opaque pinned source metadata; they are not executed or fully
parsed. Exact known subtype1/custom-lookup helpers and the registry builder
are checked against their supported forms. The tool uses explicit exceptions,
so `python -O` does not disable validation.

When intended renderer changes invalidate the pins, review the new definitions,
update the finite grammar and dispatcher annotations where needed, then refresh
the contract deliberately and rerun the focused tests. The source contract's
baseline commit stays distinct from the generated `checkout_commit`: unrelated
later commits are allowed if all source fingerprints still match. A copied
source tree without Git reports `checkout_commit: null`.

`--family-bindings` checks source fingerprints/provenance against the inventory;
all 135 unique routine IDs, names and source references; known classification
statuses/family IDs; all 448 default/custom routes and 440 serializable routes;
dispatcher exception unions; and declared counts. It also checks every bound
profile source hash/line range, source-reference IDs, family/module backlinks,
24 palette-binding IDs and explicit unallocated production state. These are
source and metadata checks, not validation of artwork joins or palette choices.
`--family-profile` can explicitly select the profile; otherwise its relative
path resolves beside the bindings file.

## Coverage remains open

Every generated object retains `actual_character_usage: NOT_MEASURED`.
Floors/layout, doors, global blocks/torches, chest-hole overlays, effects and
animations require separate coverage inputs. Complete character usage requires
ordered room replay into disposable BG/layout buffers with raw layered traces,
outer `trace_only=false`, room headers, graphics, palettes, state and custom
assets. `ObjectTileLayout::FromTraces` merges by x/y and loses layers/order;
trace-only drawing skips writes needed by later stateful tilemap reads.

The profile's art, engine, palette and production tile-budget states remain
incomplete/unverified/unallocated. Neither this inventory nor a successful
family join establishes import readiness, complete room rendering or gameplay
acceptance.
