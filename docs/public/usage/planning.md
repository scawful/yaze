# Planning Editor

The Planning editor keeps a hack's plan next to the project: the current goal,
milestones, the intended play route with the items each step gives, the open
design decisions, and links to notes. It works for vanilla A Link to the Past
projects and any hack. It does not need a ROM, and it never writes the ROM.

Open it from the activity bar (route icon) or the command palette ("Switch to
Planning Editor"). Two windows open:

- **Plan Overview:** the goal and its due date, the current milestone, a route
  strip (click a step to see its note, targets and items), the decision queue
  (set status, write the answer) and references.
- **Route:** the step list (drag to reorder, add, remove), the selected step's
  fields and the item table (name, step, category, status, where, required,
  and the later steps that need it).

`room:` and `map:` targets are buttons: they open the room in the Dungeon
editor or the map in the Overworld editor. Other targets are shown as text.

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

Keys and sections the editor does not know are kept when it saves, so other
tools can store more in the same file (the Oracle of Secrets plan keeps its
overworld notes, markers and concept boards under `overworld` and
`concepts`).
