# Project terrain catalogs

Terrain catalogs are optional project metadata. They categorize tiles without
renumbering them or changing a ROM. The catalog format is independent of the
game; each game adapter provides tile observations and stable fingerprints.
The first adapter is `alttp.overworld.tile16.v1`, covering vanilla ALTTP and
supported ZSCustomOverworld ROMs through Yaze's existing ROM profile loader.
Other engines need an adapter; this feature does not make Yaze a general ROM
parser.

## Project setup

Add a path in a native `.yaze` project:

```ini
[files]
terrain_catalog_file=assets/terrain/catalog.json
```

The equivalent JSON project field is `yaze_project.terrain_catalog_file`.
Paths resolve against the project descriptor and serialize relatively. Opening
a project without this field clears the previous project's catalog path.
The path points to a separate JSON file; save the sidecar along with the project.
Automatic bundle asset packaging and native catalog UI are not implemented yet.

## Read-only export

```sh
z3ed overworld-terrain-catalog --rom game.sfc --map 0x33 \
  --project-file game.yaze --out loom-tiles.json --format json
```

Use `--catalog labels.json` to explicitly override the project's catalog for
this export. The output must be a new file. Existing files, including a ROM or
catalog accidentally passed as `--out`, are never replaced.

The command exports the entire parent area, including large, wide and tall
areas. All occurrence coordinates are **Tile16 coordinates within that parent**.
The current adapter accepts map IDs `0x00-0x9F`; later expanded tail maps need
an adapter update. It reports used Tile16 definitions, four Tile8 quadrants,
raw collision bytes, priority, transparency counts, palette-indexed thumbnails,
and a graphics profile. Unknown Tile8 collision lookups are `null`.

Coverage is one selected area, not a complete global collision impact report.
The initial profile uses phase zero and editor ROM graphics; custom runtime
DMA, time tinting and project graphics-preview overrides are not applied.
Exports are observations, not runtime collision or layering verification.

## Label file

```json
{
  "schema_version": 1,
  "game_id": "my-project",
  "entries": [
    {
      "id": "palm.canopy.northwest",
      "name": "Palm canopy northwest",
      "category": "vegetation",
      "part": "canopy",
      "tags": ["palm", "sand"],
      "review_state": "suggested",
      "bindings": [
        {
          "adapter": "alttp.overworld.tile16.v1",
          "tile_id": 42,
          "definition_fingerprint": "COPY_FROM_EXPORT",
          "profile_fingerprint": "COPY_FROM_EXPORT"
        }
      ]
    }
  ]
}
```

The example is illustrative, not a real tile binding. Categories, parts and
tags are project-defined. `review_state` is `suggested` or `reviewed`; exports
preserve this distinction. IDs are unique. Bindings match the adapter, tile ID,
definition fingerprint and profile fingerprint exactly. A mismatch leaves a
tile unclassified. No wildcard fallback applies a label to changed graphics.
Fingerprints use FNV-1a for stable change detection, not cryptographic security.

## Remaining authoring work

This first slice supplies project persistence, validated labels and ROM
inspection. Native filtering/label editing, reusable object footprints,
cross-map usage indexing, collision transactions and agent painting remain
separate implementation steps. New collision writes must resolve the shared
Tile8-to-Tile16-to-map impact before changing behavior.
