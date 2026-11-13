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
  --project-context game.yaze --out loom-tiles.json --format json
```

`--project-file` is an older equivalent of `--project-context`; pass one of
them. Use `--catalog labels.json` to explicitly override the project's catalog
for this export. The output must be a new file. Existing files, including a ROM
or catalog accidentally passed as `--out`, are never replaced.

The command exports the entire parent area, including large, wide and tall
areas. All occurrence coordinates are **Tile16 coordinates within that parent**.
Map IDs `0x00-0x9F` are accepted on every ROM. The special-overworld tail
`0xA0-0xBF` is accepted only when the ROM has a validated native
special-overworld directory, up to its capacity; on other ROMs those slots are
blank fill and are refused. One profile describes the whole area. A page with
its own native appearance (revision-4 per-page graphics and palette) is
accepted only when the renderer builds it with the parent's graphics and
palette; otherwise the area is refused. It reports used Tile16 definitions, four Tile8 quadrants,
raw collision bytes, priority, transparency counts, palette-indexed thumbnails,
and a graphics profile. Unknown Tile8 collision lookups are `null`.

Coverage is one selected area, not a complete global collision impact report.
The profile uses phase zero and the editor's graphics composition. With a
project, its `[files] overworld_graphics_preview` transfers are applied over
the ordinary sheets, the way the editor and `overworld-render` draw the area;
`--graphics-preview=off` exports the ROM's sheets only, and
`--graphics-preview=<manifest.json>` uses another manifest. When the area has
transfers, `profile.runtime_overrides` is `graphics_preview` and the profile
fingerprint differs from the ROM-only profile. Time tinting and other runtime
DMA are not applied. Exports are observations, not runtime collision or
layering verification.

`graphics_composition` records what the renderer composed, in order: ordinary
sheets, the animated sheet in the top 32 characters of slot 7, then preview
transfers. It lists the manifest and its SHA256, the replaced character
ranges, and per slot the sheet, palette half and character counts by source.
Ordinary 3bpp sheets in slots 0, 3, 4 and 5 use colors 8-15 of a palette row
(`palette_half: high`); other slots use colors 0-7. Each quadrant's
`graphics_source` gives the same facts for its Tile8: `kind` is `sheet`,
`animated_sheet`, `graphics_preview` or `none`. Preview characters are raw
4bpp indices and have no sheet or palette half.

`transparent_mask` contains 256 source-zero flags in thumbnail pixel order,
including quadrant flips. Use this mask instead of inferring transparency from
`pixels`: the editor remaps source zero to index 8 in some graphics slots.
Opaque source color 8 must remain opaque. Unavailable source sheets fail export.
The mask comes from each quadrant's `graphics_source`: decoded sheet pixels,
the animated sheet, or the preview transfer. A custom tile group that names
sheet `0` explicitly is a real source and is exported. A quadrant whose slot has
no graphics source fails export with the Tile16, quadrant, character and slot.

Main palette, auxiliary palette selection, resolved colors, backdrop, sheets,
and world are part of the profile fingerprint. Keep separate bindings for
alternate Kalyxo and Eon Abyss profiles even when they share a Tile16 ID.

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
