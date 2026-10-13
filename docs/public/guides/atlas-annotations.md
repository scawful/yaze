# Annotating Your Project for the Atlas

> **Status: partly implemented (Atlas P1, 2026-09-28).** Working now: the
> `[atlas]` project keys, the annotation loader (yaze.atlas/1 files and the
> two Oracle of Secrets files) and `z3ed atlas-render --view dungeon --json`.
> Not yet: the overworld view, PNG output and the editor windows; those parts
> of this page describe the plan. The design is in
> [docs/internal/plans/atlas-2026-09-27.md](../../internal/plans/atlas-2026-09-27.md).

## What the Atlas is

The Atlas draws two overview pictures of your hack:

- **Dungeon Atlas:** every underworld room ($000-$127) on the 16-wide room
  grid. Cell color = the dungeon that owns the room. Doors sit in the gaps
  between rooms. Pills show entrances, stairs, holes, pits and warps. Badges
  show chest contents and keys. Outlines mark free, reclaimable, orphan and
  reserved rooms, and dashed outlines mark rooms you plan to use.
- **Overworld Atlas:** every overworld map ($00-$9F), with large areas
  merged, region colors, gate badges (Glove, Mitt, Hammer, Boots), edges
  between maps (walkable, swim, blocked, special) and link pills (warp pads,
  special entrances, whirlpools).

yaze reads most of this from the ROM. Annotations add what the ROM cannot
tell: names, colors, planned rooms and links that only exist in your ASM.

Today (P1) the dungeon model is available as JSON from the command line:

```bash
z3ed atlas-render --rom MyHack.sfc --project MyHack.yaze --view dungeon --json rooms.json
z3ed atlas-render --rom MyHack.sfc --view dungeon --json rooms.json   # no project: vanilla defaults
```

The JSON (schema `yaze.atlas.dungeon/1`) lists every room with its area,
status, name, doors, links in and out, chest and key badges and plans, plus
door edges, areas and a per-area key table. Every fact says where it came
from: `rom`, `registry`, `annotation:<file>`, `symbol:<label>` or `default`.
stdout gets a short JSON summary (ROM SHA-1, annotation files, counts,
warnings). The command never writes the ROM.

Planned: `--view overworld`, `--out <png>`, and two editor windows with
hover, filters, click-to-open and PNG export.

## Without annotations

A project with no annotations still works. yaze uses built-in vanilla
defaults: dungeon names and colors keyed by the entrance dungeon ID (Eastern
Palace, Desert Palace, ...), Light/Dark/Special World sections, large areas
from the ROM, and the vanilla rules for tiles, items and warp pads.

## Step 1: point the project file at an annotation file

Add an `[atlas]` section to your `.yaze` file:

```ini
[atlas]
annotations=Docs/Atlas/atlas.json
defaults=vanilla
```

| Key | Meaning |
|---|---|
| `annotations` | One or more annotation files, comma separated, relative to the `.yaze` file. Later files override earlier ones. |
| `defaults` | `vanilla` (default) keeps the built-in names and colors under your own; `none` starts empty. |
| `symbols` | Optional `.sym` file for links read from ROM symbols. Lookup order: `--symbols`, the `.sym` next to the ROM, this key, then `[files] symbols_filename`. |
| `export_folder` | Where the editor's Export buttons write (planned). |

yaze keeps the section when it saves the project, together with any keys,
sections and comments it does not know. A project with `--project` must load
something project-specific: a dungeon registry
(`Docs/Dev/Planning/dungeons.json`) or `[atlas]` annotations. Otherwise, and
when a listed file is missing, `atlas-render` stops with an error instead of
guessing.

## Step 2: write the annotation file

A minimal file:

```json
{
  "schema": "yaze.atlas/1",
  "title": "My Hack",
  "dungeons": [
    {"dungeon_id": "0x04", "code": "D1", "name": "Forest Temple", "color": "#4CAF50"}
  ],
  "rooms": {
    "0x0F": {"name": "Secret shop basement"}
  },
  "plans": [
    {"key": "D9", "label": "D9?", "name": "Bonus dungeon (planned)",
     "status": "provisional", "color": "#5ADCFF", "rooms": ["0x90-0x96"]}
  ]
}
```

IDs can be `"0x0F"`, `"0F"` or a number. Room and map lists accept ranges
such as `"0x90-0x96"`. Colors are `#RRGGBB`. Keys that start with `_` are
comments. The JSON Schema is `assets/atlas/atlas-annotations-1.schema.json`.

## What you can annotate

### Dungeons and areas

| Field | Use it for |
|---|---|
| `dungeons[]` | Code, name and color for each dungeon. Key by `dungeon_id` (the entrance dungeon ID byte) or by `owner` (a dungeon ID from your project registry). |
| `areas[]` | Groups of rooms the ROM cannot assign to a dungeon, for example a hub reached by a scripted warp. Each has `key`, `code`, `name`, `color`, `rooms`. |
| `rooms.<id>.name` | A room name shown in the cell and tooltip. |
| `rooms.<id>.status` | `"reserved"` marks an empty room you want to keep. |
| `plans[]` | Rooms you plan to use. Drawn as a dashed outline in the plan color. |

### Links the ROM does not show

| Field | Use it for |
|---|---|
| `links[]` | Links made by your ASM or scripts. Example: `{"view": "dungeon", "kind": "pit", "from": "0x81", "to": "0x05", "source": "pit ring checked in the emulator"}`. Leave out `to` for a label-only pill. |
| `symbol_links[]` | Room lists your ASM keeps at a label. Example: `{"symbol": "MyHack_WarpTable", "count": 3, "width": 1, "kind": "dream", "label": "DREAM {n} IN"}`. |

A link that lands in another dungeon or area gets a red ring.

### Overworld

| Field | Use it for |
|---|---|
| `overworld.sections[]` | Section titles and where each world block sits in the picture. |
| `overworld.regions[]` | Region name, world (`LW`, `DW`, `SW`), color and maps. |
| `overworld.maps.<id>.name` | Map names. |
| `overworld.gates."<map>:<Gate>"` | A note, a role override (`ROUTE`, `POCKET`, `DECOR`) or `"suppress": true` for a gate badge. |
| `overworld.edges[]` | Force an edge between two maps to `open`, `swim`, `blocked` or `special`. |
| `overworld.closed_border_maps` | Maps whose borders are closed (special camera). |
| `overworld.isolated_maps` | Maps the walkability check must not cross. |

Gates and edges are detected from tile types by default. Use annotations to
correct the result, not to repeat it.

## Coming from Oracle of Secrets-style files

The Atlas also reads the two files used by the Oracle of Secrets map scripts,
`room_annotations.json` and `overworld_annotations.json`, without changes:

```ini
[atlas]
annotations=Scripts/Maps/room_annotations.json,Scripts/Maps/overworld_annotations.json
```

The field-by-field mapping is in section 6.5 of the design doc. The loader
also accepts these optional keys in those files, for what the old scripts
hard-coded: `title`, `dungeon_colors` (`{"D1": "#E27676"}`), `areas[].code`,
`areas[].color`, `plans[].color`, `symbol_links` and `legend_notes` in the
room file; `sections`, `legend_notes` and `title` in the overworld file.

The old scripts read the dream warps from the `.sym` themselves. For the
Atlas, add them to the room file:

```json
"symbol_links": [
  {"symbol": "Oracle_Link_WarpToRoom_room", "count": 3, "width": 1,
   "kind": "dream", "label": "DREAM {n} IN"}
]
```

With that entry, `atlas-render` on a playtest ROM with its `.sym` matches the
old `room_matrix.json` room for room.

## Checking your file

- Loading is strict. An unknown key, a bad ID or range, a room outside
  $000-$127, a map outside $00-$9F, a bad color, an unknown `schema` or
  invalid JSON stops `atlas-render` with a non-zero exit and names the file
  and the JSON pointer, for example
  `room_annotations.json: /plans/0/rooms/1: "0x1G" is not a room ID ...`.
- Warnings (in the JSON and the summary): an annotation link that repeats a
  ROM fact, a header target past $127, a symbol that is not in the `.sym`.
- Planned: the editor's Atlas windows show the same errors and warnings in
  their status line and keep the last good picture when a file has an error.
- Every tooltip line says where the fact came from: ROM, project registry,
  annotation file or default.
