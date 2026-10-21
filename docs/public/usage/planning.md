# Planning Editor

The Planning editor keeps a hack's plan next to the project: the current goal,
milestones, the intended play route with the items each step gives, the open
design decisions, and links to notes. It works for vanilla A Link to the Past
projects and any hack. It does not need a ROM, and it never writes the ROM.

Open it from the activity bar (route icon) or the command palette ("Switch to
Planning Editor"). Three windows open:

- **Plan Overview:** the goal and its due date, the current milestone, a route
  strip (click a step to see its note, targets and items), the decision queue
  (set status, write the answer) and references.
- **Route:** the step list (drag to reorder, add, remove), the selected step's
  fields and the item table (name, step, category, status, where, required,
  and the later steps that need it).
- **Plan Maps:** each world as a grid of its overworld areas, with the
  plan's markers, notes, sketch lines and regions on top. See below.

`room:` and `map:` targets are buttons: they open the room in the Dungeon
editor or the map in the Overworld editor. `marker:` and `area:` targets
select the marker in Plan Maps (`area:D1` finds the marker whose code is
`D1`). Other targets are shown as text.

## Plan Maps

Plan Maps draws a schematic grid, one cell per 512x512 overworld area,
labelled with the map number; it does not render the overworld itself, so it
works without a ROM. Use the world buttons to switch worlds and the checkboxes
to hide markers, notes, regions or lines.

- Click a marker, note or region to edit it in the side panel (label, code,
  kind, status, room, detail). Drag markers and notes to move them.
- Double-click empty map to add a note. Right-click for **Add marker here**,
  **Add region here**, **Open map in Overworld editor**, **Open room** and
  delete.
- The marker list filters by label or code; clicking an entry scrolls the map
  to it. Items listed under `overworld.unplaced.<world>` show as "Not placed
  yet".

Each entry's `map` is kept when you move it within the same 512x512 area, so a
map number that names the parent of a large area survives small moves. Moving
it into another area sets that area's number.

Edits are held in memory until you press **Save plan**. **Reload** discards
unsaved edits and reads the file again.

## Linking a plan to a project

The plan is a JSON file named in the `.yaze` project file:

```ini
[planning]
file=MyHack.plan.json
```

`file` is absolute or relative to the `.yaze` file. It may point outside the
repository, for example to a shared notes folder used by several checkouts.
If the project has no `[planning]` section, the editor offers to create
`<project>.plan.json` next to the project file and adds the section.

## File format (`yaze.plan/1`)

```json
{
  "schema": "yaze.plan/1",
  "title": "My Hack",
  "about": "One paragraph on what this plan covers.",
  "goal": {"title": "Demo build", "detail": "...", "due": "2026-12-25"},
  "milestones": [
    {"id": "w1", "title": "Dungeons 1-3", "start": "2026-10-01",
     "end": "2026-10-31", "status": "active", "detail": "..."}
  ],
  "route": {
    "steps": [
      {"id": "D1", "code": "D1", "title": "Eastern Palace", "kind": "dungeon",
       "group": "Act 1", "note": "...", "targets": ["room:0xC9", "map:0x1E"]}
    ],
    "items": [
      {"id": "bow", "name": "Bow", "step": "D1", "category": "gear",
       "required": true, "status": "implemented", "where": "Big chest",
       "needs": [{"step": "D2", "what": "eye switches"}]}
    ]
  },
  "decisions": [
    {"id": "d1-boss", "priority": 1, "status": "open",
     "title": "Which boss for D1?", "recommendation": "...", "answer": "",
     "targets": ["room:0xC8"], "refs": ["notes/d1.md"]}
  ],
  "references": [
    {"label": "Design notes", "path": "notes/design.md", "author": "me",
     "date": "2026-10-20"}
  ]
}
```

Only `schema` is required. Status values the editor offers:

| Field | Values |
|---|---|
| milestone `status` | `planned`, `active`, `done` |
| step `kind` | `story`, `errand`, `dungeon`, `shrine`, `boss`, `optional` |
| item `status` | `implemented`, `prototype`, `decided`, `proposed`, `open` |
| decision `status` | `open`, `decided`, `dropped` |

The current milestone is the one marked `active`, otherwise the one whose
dates contain today.

Targets are `<kind>:<id>` strings: `room:0x0C`, `map:0x4B`, `area:D1`,
`step:D4`, `item:bow`, `decision:d1-boss`, `marker:<world>:<id>`.

### Overworld layer

```json
"overworld": {
  "worlds": [{"id": "light", "title": "Light World", "base": "0x00", "maps": 64}],
  "markers": [
    {"id": "light:m0", "world": "light", "x": 3100, "y": 1650, "map": "0x1E",
     "kind": "dungeon", "label": "Eastern Palace", "status": "implemented",
     "code": "D1", "room": "0xC9", "detail": "..."}
  ],
  "notes": [{"id": "note-1", "world": "light", "x": 600, "y": 900,
             "map": "0x09", "text": "...", "created": "2026-10-20T00:36:14Z"}],
  "lines": [{"id": "line-1", "world": "light", "color": "red",
             "points": [[162, 3352], [123, 2968]]}],
  "regions": [{"id": "region-1", "world": "light", "x": 512, "y": 2112,
               "radius": 256, "label": "Village", "status": "proposed"}]
}
```

Positions are world pixels: a world is 8 areas wide and each area is 512x512,
so a 64-area world spans 4096x4096 and the area under `(x, y)` is
`base + (y / 512) * 8 + x / 512`. Marker `kind` is one of `dungeon`, `shrine`,
`place`, `item`, `planned`; marker and region `status` is one of
`implemented`, `decided`, `draft`, `proposed`, `idea`. Line colors `red`,
`green`, `blue`, `yellow` and `orange` use the theme's status colors.

Keys and sections the editor does not know are kept when it saves, so other
tools can store more in the same file (the Oracle of Secrets plan keeps its
concept boards under `concepts`, and per-marker `icon`/`approx` and
`overworld.area_plans` survive edits).
