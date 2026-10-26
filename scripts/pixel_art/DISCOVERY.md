# Suggested Tile16 discovery import

Validate and correct an existing discovery packet before using it to draft a
placement proposal. This tool imports retained exports; it does not execute
z3ed, open an editor document, write a ROM or approve semantic labels.

```sh
python3 -B scripts/pixel_art/tile16_discovery.py \
  --grids /path/to/grids.json \
  --candidates /path/to/candidates.json \
  --exports /path/to/req-json-directory

# Optional identities and create-only private artifact output.
python3 -B scripts/pixel_art/tile16_discovery.py \
  --grids /path/to/grids.json \
  --candidates /path/to/candidates.json \
  --exports /path/to/req-json-directory \
  --rom /path/to/original-input.sfc \
  --exporter /path/to/original-z3ed \
  --exporter-source-identity recorded-source-commit \
  --source-tool /path/to/original-miner.py \
  --out /path/to/new-private-discovery-directory

PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover \
  -s scripts/pixel_art/tests -p test_tile16_discovery.py -v
```

Without `--out`, validation prints its result without writing artifacts. All
validation finishes before the output directory is created. An existing output
directory, file or symlink is refused. Files use exclusive creation and the
manifest is written last; an I/O failure can leave a partial output to inspect.

## Input guards

1. The adapter is `alttp.overworld.tile16.v1`, export schema is `1`, and source
   transparency precedes palette remapping. Profile and Tile16 definition FNV
   fingerprints are recomputed from the profile JSON and four raw Tile8 words.
2. Every export has the same ROM fingerprint, including different parent areas.
   Duplicate parent payloads must agree. Definitions cannot change across
   parents. The same profile/Tile16/definition cannot contain conflicting native
   pixels, source-zero masks or quadrant payloads across parents. Optional `--rom`
   checks the ROM bytes against the exported fingerprint.
3. Grid dimensions, source cells, physical exclusion masks, parent coverage,
   requested maps and profile fingerprints must match the exports exactly.
4. Every non-skip cell at every emitted candidate anchor must match its source.
   A candidate `null` means skip; a source-grid `-1` means excluded data. Neither
   is Tile16 `$0000`. Non-repeat source mismatches fail the import.

The optional ROM and exporter files are read for identity only. The manifest
records their SHA-256 and size, the importer's hash, all export hashes, and any
`--source-tool` hashes. `--exporter-source-identity` is a recorded claim. The
tool does not prove that a named source commit produced an exporter binary,
authenticate the exporter, or establish the truth of user-supplied metadata.
JSON parsing and SHA-256 use one byte snapshot per input; optional ROM FNV and
SHA-256 also use one snapshot. Later file replacement cannot change the hash
assigned to already parsed data. Tool identities are point-in-time snapshots;
the importer also rejects a corrector source change detected during import.

## Corrected occurrence semantics

The legacy miner canonicalized repeat matrices while retaining anchors from
larger source rectangles. Import keeps each canonical matrix and finds every
exact unit match across all supplied grids, including overlapping matches.
It recomputes areas, occurrence counts, cell counts, score and even-coordinate
anchor counts. These counts describe unit matches; they are different from the
legacy larger-rectangle counts and greedy non-overlap counts.

Discovery IDs remain the source packet's handles. They are not reassigned after
scores change. Full original repeat records are retained in `source_observation`,
including their original anchors and any extent/repeat metadata they supplied.
The importer does not reconstruct larger mined extents absent from that packet.
It does not repair the miner's dominance heuristic or certify completeness.

Each candidate has exact per-area adapter, profile and definition bindings.
All candidates remain `review_state: "suggested"`, `placement_enabled: false`
and `overlap_policy: "unapproved"`. An exact matrix match does not establish
compatible behavior in a new destination. No forest overlap rule is inferred.

## Artifacts and preview helpers

| Artifact | Meaning |
|---|---|
| `candidates.json` | Versioned suggested-discovery document with corrected anchors, original repeat records and exact bindings. It is not an editor write schema. |
| `observations.json.gz` | Deterministic compressed JSON preserving one complete, validated payload per parent, including indexed pixels, palettes, source-zero masks, Tile8 words and raw observations. |
| `manifest.json` | Input/export/tool hashes, original and corrected replay totals, artifact hashes and explicit provenance limits. |

Deduplication removes only `requested_map` from parent payloads; the manifest
retains each request-to-parent mapping. The compressed observations retain the
data needed for native rendering without keeping temporary raw exports. They do
not reproduce the original exports' whitespace or byte hashes.

`native_indexed_tile(record, profile)` returns a native 16×16 indexed `P` image
and a separate `L` source-zero mask. Mask value `255` identifies source-zero
pixels. Packed palette words remain in the lossless observations. `bgr555(word)`
uses Yaze's `(value << 3) | (value >> 2)` channel expansion.
`integer_nearest(image, scale)` permits integer scales `1..16` only and uses
explicit nearest-neighbor resampling. Neither helper downsamples a contact entry.
For a display image, composite the source-zero mask with that profile's backdrop;
do not infer transparency from the exported display index or palette RGB.

The reusable Python entry point is
`import_corpus(grids_path, candidates_path, export_dir, *, rom_path=None, exporter_path=None, exporter_source_identity=None, source_paths=())`.
It returns `(corrected_document, manifest, lossless_parent_observations)` without
writing output. Keep private ROM-derived artifacts outside the Git repository.
