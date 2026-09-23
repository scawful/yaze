# Editor candidate status

The combined editor candidate brings together overworld/Tile16 changes from
`0f3855d6f` and dungeon authoring from `804b32eea`. The combined application build
and 1,303 focused automated tests passed; three optional asset tests skipped. Native editing and disposable-ROM acceptance
remain unchecked.

Use [the roadmap](roadmap.md), [the capability plan](plans/editor-capability-parity-plan.md),
and [the release checklist](release-checklist.md) for scope and acceptance.
Historical test counts are not evidence for the combined candidate.

See the [combined candidate handoff](agents/combined-editor-candidate-2026-09-23.md)
for evidence and the single manual testing queue.

## Room template files

The combined branch now adds native desktop room JSON file import/export.
The app/unit build and 128 focused room-document/transfer tests passed with no
skips. This is incremental evidence after the combined baseline above, not a
rerun of all 1,303 earlier tests. See the
[room file handoff](agents/dungeon-room-template-files-2026-09-23.md).

## Pit and staircase destinations

Shared Workbench/standalone destination controls now include pit arrival layer,
named plane choices, explicit stair header slots and destination navigation.
App/unit build and 264 focused tests passed with no skips. Runtime traversal and
placed-staircase mapping remain unqualified. See the
[destination handoff](agents/dungeon-destinations-2026-09-23.md).
